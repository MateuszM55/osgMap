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

#include "common.h"

using namespace osg;

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
    struct LabelItem
    {
        osg::Billboard* node;
        osg::Vec3 screenPos;
        double distToCam;
    };

    double _minDistSq;

public:
    SortAndCullLabelsCallback(double pixelDist = 45.0)
        : _minDistSq(pixelDist * pixelDist)
    {}

    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
    {
        osgUtil::CullVisitor* cv = dynamic_cast<osgUtil::CullVisitor*>(nv);
        if (!cv)
        {
            traverse(node, nv);
            return;
        }

        osg::Group* group = node->asGroup();
        if (!group)
        {
            traverse(node, nv);
            return;
        }

        osg::Camera* camera = cv->getCurrentCamera();
        if (!camera)
        {
            traverse(node, nv);
            return;
        }

        osg::Matrixd viewMat = *cv->getModelViewMatrix();
        osg::Matrixd projMat = *cv->getProjectionMatrix();
        const osg::Viewport* viewport = cv->getViewport();

        if (!viewport)
        {
            traverse(node, nv);
            return;
        }

        osg::Matrixd viewProjMat = viewMat * projMat;
        osg::Matrixd windowMat = viewport->computeWindowMatrix();
        osg::Matrixd mvpw = viewProjMat * windowMat;

        std::vector<LabelItem> visibleLabels;
        visibleLabels.reserve(group->getNumChildren());

        for (unsigned int i = 0; i < group->getNumChildren(); ++i)
        {
            osg::Billboard* bb =
                dynamic_cast<osg::Billboard*>(group->getChild(i));
            if (!bb) continue;
            if (bb->getNumDrawables() == 0) continue;

            osg::Vec3 center = bb->getPosition(0);

            osg::Vec3 eyePos = center * viewMat;

            double dist = eyePos.length();

            osg::Vec3 screenPos = center * mvpw;

            visibleLabels.push_back({ bb, screenPos, dist });
        }

        std::sort(visibleLabels.begin(), visibleLabels.end(),
                  [](const LabelItem& a, const LabelItem& b) {
                      return a.distToCam < b.distToCam;
                  });

        std::vector<osg::Vec3> acceptedPositions;

        for (auto& item : visibleLabels)
        {
            bool overlaps = false;
            for (const auto& acceptedPos : acceptedPositions)
            {
                double dx = item.screenPos.x() - acceptedPos.x();
                double dy = item.screenPos.y() - acceptedPos.y();

                if ((dx * dx + dy * dy) < _minDistSq)
                {
                    overlaps = true;
                    break;
                }
            }

            if (!overlaps)
            {
                acceptedPositions.push_back(item.screenPos);
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

    cache[filename] = ss;
    return ss.get();
}

static osg::Billboard* createLabelNode(const LabelData& data,
                                       osg::StateSet* sharedIconStateSet)
{
    osg::Billboard* bb = new osg::Billboard();
    bb->setMode(osg::Billboard::POINT_ROT_EYE);

    bool hasIcon = (sharedIconStateSet != nullptr);

    if (hasIcon)
    {
        osg::Geometry* iconGeom = new osg::Geometry();

        float w = 6.0f;
        float h = 6.0f;

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

        bb->addDrawable(iconGeom, data.position);
    }

    if (!data.name.empty())
    {
        osgText::Text* text = new osgText::Text;

        static osg::ref_ptr<osgText::Font> sharedFont =
            osgText::readRefFontFile("fonts/arial.ttf");
        if (!sharedFont) sharedFont = osgText::readRefFontFile("arial.ttf");

        text->setFont(sharedFont);

        text->setText(
            osgText::String(data.name, osgText::String::ENCODING_UTF8));

        text->setAlignment(osgText::Text::CENTER_BOTTOM);
        text->setAxisAlignment(osgText::Text::XZ_PLANE);

        text->setBackdropType(osgText::Text::OUTLINE);

        float fontSize = 3.5f;
        osg::Vec4 textColor(1.0f, 1.0f, 1.0f, 1.0f); 
        osg::Vec4 outlineColor(0.0f, 0.0f, 0.0f, 1.0f);

        auto checkType = [&](const std::string& keyword) {
            return (data.subtype.find(keyword) != std::string::npos
                    || data.type.find(keyword) != std::string::npos);
        };

        if (checkType("station") || checkType("subway") || checkType("tram"))
        {
            fontSize = 5.0f;
            textColor.set(1.0f, 0.9f, 0.2f, 1.0f);
        }
        else if (checkType("university") || checkType("school")
                 || checkType("college"))
        {
            fontSize = 4.2f;
            textColor.set(0.6f, 0.8f, 1.0f, 1.0f);
        }
        else if (checkType("townhall") || checkType("government"))
        {
            fontSize = 4.5f;
            textColor.set(1.0f, 0.6f, 0.6f, 1.0f);
        }
        else if (checkType("pub") || checkType("bar") || checkType("cafe"))
        {
            fontSize = 3.0f;
            textColor.set(0.7f, 1.0f, 0.7f, 1.0f);
        }

        text->setCharacterSize(fontSize);
        text->setColor(textColor);
        text->setBackdropColor(outlineColor);

        float offsetZ = hasIcon ? 7.0f : 0.0f;
        bb->addDrawable(text, data.position + osg::Vec3(0, 0, offsetZ));
    }

    osg::StateSet* ss = bb->getOrCreateStateSet();
    ss->setMode(GL_LIGHTING,
                osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

    return bb;
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

    std::vector<LabelData> finalLabels;
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

    labelsGroup->setCullCallback(new SortAndCullLabelsCallback(45.0));

    return labelsGroup;
}