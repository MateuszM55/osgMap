#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osgUtil/CullVisitor>
#include <osg/CoordinateSystemNode>
#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/Billboard>
#include <osgSim/ShapeAttribute>
#include <osgViewer/View>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Program>
#include <osg/Projection>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Image>
#include <osg/Texture2D>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <cstring>
#include <map>
#include <cmath>

#include "common.h"

using namespace osg;

const float LABEL_TEXT_SIZE = 18.0f;
const float ICON_SIZE_WORLD = 8.0f;
const float MAX_VIEW_DISTANCE = 1500.0f;


const float CHAR_WIDTH_EST = 8.0f;
const float ICON_SCREEN_W = 24.0f;
const float ICON_SCREEN_H = 24.0f;
const float PADDING = 2.0f;


struct LabelData
{
    osg::Vec3 position;
    std::string name;
    std::string type;
    std::string subtype;
};

class SimpleDBFReader {
public:
    struct Record
    {
        std::string name;
        std::string type;
        std::string subtype;
    };
    std::vector<Record> records;

    struct FieldInfo
    {
        int offset = -1;
        int length = 0;
    };

    std::string cleanString(std::string s)
    {
        while (!s.empty())
        {
            unsigned char c = (unsigned char)s.back();
            if (std::isalnum(c) || std::ispunct(c) || c > 127) break;
            s.pop_back();
        }
        s.erase(s.begin(),
                std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }));
        return s;
    }

    bool load(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        char header[32];
        file.read(header, 32);

        unsigned int numRecords = 0;
        unsigned short headerSize = 0;
        unsigned short recordSize = 0;

        std::memcpy(&numRecords, header + 4, 4);
        std::memcpy(&headerSize, header + 8, 2);
        std::memcpy(&recordSize, header + 10, 2);

        FieldInfo nameField, typeField, subtypeField;
        int currentOffset = 1;

        while (file.tellg() < headerSize - 1)
        {
            char fieldDesc[32];
            file.read(fieldDesc, 32);
            if (fieldDesc[0] == 0x0D) break;

            std::string fieldName(fieldDesc, 11);
            fieldName = fieldName.c_str();
            std::transform(fieldName.begin(), fieldName.end(),
                           fieldName.begin(), ::tolower);

            unsigned char fieldLen = (unsigned char)fieldDesc[16];

            if (fieldName.find("name") == 0)
            {
                nameField.offset = currentOffset;
                nameField.length = fieldLen;
            }
            if (fieldName.find("type") == 0)
            {
                typeField.offset = currentOffset;
                typeField.length = fieldLen;
            }
            if (fieldName.find("subtype") == 0)
            {
                subtypeField.offset = currentOffset;
                subtypeField.length = fieldLen;
            }

            currentOffset += fieldLen;
        }

        file.seekg(headerSize);
        char* buffer = new char[recordSize];

        for (unsigned int i = 0; i < numRecords; ++i)
        {
            file.read(buffer, recordSize);
            if (file.gcount() < recordSize) break;

            Record rec;
            if (nameField.offset != -1)
                rec.name = cleanString(
                    std::string(buffer + nameField.offset, nameField.length));
            if (typeField.offset != -1)
                rec.type = cleanString(
                    std::string(buffer + typeField.offset, typeField.length));
            else
                rec.type = "default";
            if (subtypeField.offset != -1)
                rec.subtype = cleanString(std::string(
                    buffer + subtypeField.offset, subtypeField.length));

            std::transform(rec.type.begin(), rec.type.end(), rec.type.begin(),
                           ::tolower);
            std::transform(rec.subtype.begin(), rec.subtype.end(),
                           rec.subtype.begin(), ::tolower);

            records.push_back(rec);
        }
        delete[] buffer;
        return true;
    }
};

class OnlyGeometryExtractor : public osg::NodeVisitor {
public:
    std::vector<osg::Vec3> _positions;
    OnlyGeometryExtractor(): osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

    void apply(osg::Geode& geode) override
    {
        for (unsigned int i = 0; i < geode.getNumDrawables(); ++i)
        {
            osg::Geometry* geom = geode.getDrawable(i)->asGeometry();
            if (!geom) continue;
            osg::Vec3Array* verts =
                dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
            if (!verts) continue;
            for (const auto& v : *verts) _positions.push_back(v);
        }
        traverse(geode);
    }
};

class SortAndCullLabelsCallback : public osg::NodeCallback {
    struct ScreenBox
    {
        double minX, maxX;
        double minY, maxY;

        bool overlaps(const ScreenBox& other) const
        {
            if (maxX < other.minX) return false;
            if (minX > other.maxX) return false;
            if (maxY < other.minY) return false;
            if (minY > other.maxY) return false;
            return true;
        }
    };

    struct LabelItem
    {
        osg::MatrixTransform* node;
        ScreenBox box;
        double distToCam;
    };

public:
    SortAndCullLabelsCallback() {}

    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
    {
        osgUtil::CullVisitor* cv = dynamic_cast<osgUtil::CullVisitor*>(nv);
        if (!cv)
        {
            traverse(node, nv);
            return;
        }

        osg::Group* group = node->asGroup();
        if (!group) return;

        osg::Camera* camera = cv->getCurrentCamera();
        if (!camera) return;

        osg::Matrixd viewMat = *cv->getModelViewMatrix();
        osg::Matrixd projMat = *cv->getProjectionMatrix();
        const osg::Viewport* viewport = cv->getViewport();
        if (!viewport) return;

        osg::Matrixd mvpw = viewMat * projMat * viewport->computeWindowMatrix();

        std::vector<LabelItem> visibleLabels;
        visibleLabels.reserve(group->getNumChildren());

        for (unsigned int i = 0; i < group->getNumChildren(); ++i)
        {
            osg::MatrixTransform* mt =
                dynamic_cast<osg::MatrixTransform*>(group->getChild(i));
            if (!mt) continue;

            osg::Vec3 centerWorld = mt->getMatrix().getTrans();
            osg::Vec3 eyePos = centerWorld * viewMat;

            if (eyePos.z() > 0) continue;

            double dist = eyePos.length();
            if (dist > MAX_VIEW_DISTANCE) continue;

            osg::Vec3 screenPos = centerWorld * mvpw;

            if (screenPos.x() < 0 || screenPos.x() > viewport->width()
                || screenPos.y() < 0 || screenPos.y() > viewport->height())
            {
                continue;
            }

            std::string labelName = mt->getName();

            double widthPixels = (labelName.length() * CHAR_WIDTH_EST);
            if (widthPixels < ICON_SCREEN_W) widthPixels = ICON_SCREEN_W;

            double heightPixels = LABEL_TEXT_SIZE + ICON_SCREEN_H;

            ScreenBox box;
            box.minX = screenPos.x() - (widthPixels / 2.0) - PADDING;
            box.maxX = screenPos.x() + (widthPixels / 2.0) + PADDING;
            box.minY = screenPos.y() - (ICON_SCREEN_H / 2.0) - PADDING;
            box.maxY = screenPos.y() + heightPixels + PADDING;

            visibleLabels.push_back({ mt, box, dist });
        }

        std::sort(visibleLabels.begin(), visibleLabels.end(),
                  [](const LabelItem& a, const LabelItem& b) {
                      return a.distToCam < b.distToCam;
                  });

        std::vector<ScreenBox> acceptedBoxes;
        acceptedBoxes.reserve(visibleLabels.size());

        for (auto& item : visibleLabels)
        {
            bool overlaps = false;
            for (const auto& existingBox : acceptedBoxes)
            {
                if (item.box.overlaps(existingBox))
                {
                    overlaps = true;
                    break;
                }
            }

            if (!overlaps)
            {
                acceptedBoxes.push_back(item.box);
                item.node->accept(*nv);
            }
        }
    }
};

static std::string determineIconTexture(const std::string& type,
                                        const std::string& subtype)
{
    auto matches = [&](const std::string& keyword) {
        return (subtype.find(keyword) != std::string::npos
                || type.find(keyword) != std::string::npos);
    };

    if (matches("bus_stop")) return "bus.png";
    if (matches("tram_stop")) return "tram.png";
    if (matches("subway_entrance")) return "subway.png";
    if (matches("station")) return "train.png";
    if (matches("halt")) return "default.png";

    if (matches("university")) return "university.png";
    if (matches("college")) return "university.png";
    if (matches("school")) return "school.png";
    if (matches("kindergarten")) return "school.png";

    if (matches("bar")) return "bar.png";
    if (matches("pub")) return "bar.png";
    if (matches("cafe")) return "cafe.png";
    if (matches("restaurant")) return "restaurant.png";
    if (matches("fast_food")) return "restaurant.png";

    if (matches("townhall")) return "hall.png";
    if (matches("government")) return "hall.png";
    if (matches("public_building")) return "hall.png";

    return "default.png";
}

static osg::StateSet*
getSharedStateSet(const std::string& filename,
                  std::map<std::string, osg::ref_ptr<osg::StateSet>>& cache)
{
    if (filename.empty()) return nullptr;

    auto it = cache.find(filename);
    if (it != cache.end())
    {
        return it->second.get();
    }

    std::string localPath = "images/labelsTextures/" + filename;

    osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile(localPath);

    if (!image)
    {
        std::cerr << "Warning: Texture not found: " << localPath << std::endl;
        return nullptr;
    }

    osg::ref_ptr<osg::StateSet> ss = new osg::StateSet();
    osg::Texture2D* tex = new osg::Texture2D(image);
    tex->setFilter(osg::Texture::MIN_FILTER,
                   osg::Texture::LINEAR_MIPMAP_LINEAR);
    tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);

    ss->setTextureAttributeAndModes(0, tex, osg::StateAttribute::ON);

    osg::BlendFunc* bf =
        new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ss->setAttributeAndModes(bf, osg::StateAttribute::ON);

    ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    osg::Depth* depth = new osg::Depth;
    depth->setWriteMask(false);
    depth->setFunction(osg::Depth::LESS);
    ss->setAttributeAndModes(depth, osg::StateAttribute::ON);

    cache[filename] = ss;
    return ss.get();
}

static osg::MatrixTransform* createLabelNode(const LabelData& data,
                                             osg::StateSet* sharedIconStateSet)
{
    osg::MatrixTransform* mt = new osg::MatrixTransform;
    mt->setMatrix(osg::Matrix::translate(data.position));
    mt->setName(data.name);

    bool hasIcon = (sharedIconStateSet != nullptr);
    float offsetZ = 0.0f;

    if (hasIcon)
    {
        osg::Billboard* bb = new osg::Billboard();
        bb->setMode(osg::Billboard::POINT_ROT_EYE);

        osg::Geometry* iconGeom = new osg::Geometry();
        float w = ICON_SIZE_WORLD / 2.0f;
        float h = ICON_SIZE_WORLD / 2.0f;

        osg::Vec3Array* verts = new osg::Vec3Array;
        verts->push_back(osg::Vec3(-w, 0, -h));
        verts->push_back(osg::Vec3(w, 0, -h));
        verts->push_back(osg::Vec3(w, 0, h));
        verts->push_back(osg::Vec3(-w, 0, h));
        iconGeom->setVertexArray(verts);

        osg::Vec2Array* texcoords = new osg::Vec2Array;
        texcoords->push_back(osg::Vec2(0, 0));
        texcoords->push_back(osg::Vec2(1, 0));
        texcoords->push_back(osg::Vec2(1, 1));
        texcoords->push_back(osg::Vec2(0, 1));
        iconGeom->setTexCoordArray(0, texcoords);

        osg::Vec4Array* colors = new osg::Vec4Array;
        colors->push_back(osg::Vec4(1, 1, 1, 1));
        iconGeom->setColorArray(colors, osg::Array::BIND_OVERALL);

        iconGeom->addPrimitiveSet(
            new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));

        iconGeom->setStateSet(sharedIconStateSet);
        bb->addDrawable(iconGeom, osg::Vec3(0, 0, 0));
        bb->getOrCreateStateSet()->setMode(GL_LIGHTING,
                                           osg::StateAttribute::OFF
                                               | osg::StateAttribute::OVERRIDE);

        mt->addChild(bb);
        offsetZ = h * 1.5f;
    }

    if (!data.name.empty())
    {
        osg::Geode* textGeode = new osg::Geode();
        osgText::Text* text = new osgText::Text;

        static osg::ref_ptr<osgText::Font> sharedFont =
            osgText::readRefFontFile("fonts/arial.ttf");
        if (!sharedFont) sharedFont = osgText::readRefFontFile("arial.ttf");

        text->setFont(sharedFont);
        text->setText(
            osgText::String(data.name, osgText::String::ENCODING_UTF8));

        text->setCharacterSizeMode(osgText::Text::SCREEN_COORDS);
        text->setCharacterSize(LABEL_TEXT_SIZE);

        text->setAlignment(osgText::Text::CENTER_BOTTOM);
        text->setAxisAlignment(osgText::Text::SCREEN);

        text->setBackdropType(osgText::Text::OUTLINE);

        osg::Vec4 textColor(1.0f, 1.0f, 1.0f, 1.0f);
        osg::Vec4 outlineColor(0.0f, 0.0f, 0.0f, 1.0f);

        auto checkType = [&](const std::string& keyword) {
            return (data.subtype.find(keyword) != std::string::npos
                    || data.type.find(keyword) != std::string::npos);
        };

        if (checkType("station") || checkType("subway") || checkType("tram"))
            textColor.set(1.0f, 0.9f, 0.2f, 1.0f);
        else if (checkType("university") || checkType("school")
                 || checkType("college"))
            textColor.set(0.6f, 0.8f, 1.0f, 1.0f);
        else if (checkType("townhall") || checkType("government"))
            textColor.set(1.0f, 0.6f, 0.6f, 1.0f);
        else if (checkType("pub") || checkType("bar") || checkType("cafe"))
            textColor.set(0.7f, 1.0f, 0.7f, 1.0f);

        text->setColor(textColor);
        text->setBackdropColor(outlineColor);
        text->setPosition(osg::Vec3(0, 0, offsetZ));

        textGeode->addDrawable(text);
        textGeode->getOrCreateStateSet()->setMode(
            GL_LIGHTING,
            osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

        mt->addChild(textGeode);
    }

    return mt;
}

osg::Node* createHUD() { return new osg::Group; }

osg::Node* process_labels(osg::Matrixd& ltw, const std::string& file_path)
{
    std::string shp_path = file_path + "/test_pointss.shp";
    std::string dbf_path = file_path + "/test_pointss.dbf";

    osg::ref_ptr<osg::Node> raw_model = osgDB::readRefNodeFile(shp_path);
    if (!raw_model)
    {
        shp_path = file_path + "/osm_points.shp";
        dbf_path = file_path + "/osm_points.dbf";
        raw_model = osgDB::readRefNodeFile(shp_path);
        if (!raw_model) return new osg::Group;
    }

    ConvertFromGeoProjVisitor<false> cfgp;
    raw_model->accept(cfgp);
    WorldToLocalVisitor ltwv(ltw, true);
    raw_model->accept(ltwv);

    OnlyGeometryExtractor extractor;
    raw_model->accept(extractor);

    SimpleDBFReader dbfReader;
    bool hasDBF = dbfReader.load(dbf_path);

    size_t count =
        std::min(extractor._positions.size(), dbfReader.records.size());
    if (!hasDBF) count = 0;

    std::map<std::string, osg::ref_ptr<osg::StateSet>> iconStateSets;
    osg::Group* labelsGroup = new osg::Group;

    for (size_t i = 0; i < count; ++i)
    {
        LabelData ld;
        ld.position = extractor._positions[i];
        ld.name = dbfReader.records[i].name;
        ld.subtype = dbfReader.records[i].subtype;
        ld.type = dbfReader.records[i].type;

        if (ld.name.length() < 2) continue;
        if (ld.name == "public_transport" || ld.name == "bus_stop"
            || ld.name == "shelter" || ld.name == "platform")
            continue;

        bool allDigits = true;
        for (char c : ld.name)
        {
            if (!std::isdigit((unsigned char)c)
                && !std::isspace((unsigned char)c))
            {
                allDigits = false;
                break;
            }
        }
        if (allDigits) continue;

        ld.position.z() += 25.0f;

        std::string iconFile = determineIconTexture(ld.type, ld.subtype);
        osg::StateSet* iconSS = nullptr;
        if (!iconFile.empty())
        {
            iconSS = getSharedStateSet(iconFile, iconStateSets);
        }

        labelsGroup->addChild(createLabelNode(ld, iconSS));
    }

    std::cout << "--- LABELS: Utworzono " << labelsGroup->getNumChildren()
              << " etykiet." << std::endl;

    labelsGroup->setCullCallback(new SortAndCullLabelsCallback());

    return labelsGroup;
}