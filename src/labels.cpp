#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
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

// --- STRUKTURA DANYCH ---
struct LabelData
{
    osg::Vec3 position;
    std::string name;
    std::string type;
    std::string subtype;
};

// --- CZYTNIK DBF ---
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

// --- ESTRAKCJA GEOMETRII ---
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

// --- HELPER: LOGIKA DOBIERANIA IKON ---
std::string determineIconTexture(const std::string& type,
                                 const std::string& subtype)
{
    // Transport
    if (subtype.find("bus_stop") != std::string::npos) return "bus.png";
    if (subtype.find("tram_stop") != std::string::npos) return "tram.png";
    if (subtype.find("subway") != std::string::npos
        || subtype.find("metro") != std::string::npos)
        return "subway.png";
    if (subtype.find("railway") != std::string::npos
        || subtype.find("train") != std::string::npos)
        return "train.png";

    // Edukacja
    if (subtype.find("university") != std::string::npos
        || type.find("university") != std::string::npos)
        return "university.png";
    if (subtype.find("school") != std::string::npos
        || type.find("school") != std::string::npos)
        return "school.png";
    if (subtype.find("kindergarten") != std::string::npos) return "school.png";

    // Jedzenie / Bary
    if (subtype.find("bar") != std::string::npos
        || subtype.find("pub") != std::string::npos)
        return "bar.png";
    if (subtype.find("cafe") != std::string::npos) return "cafe.png";
    if (subtype.find("restaurant") != std::string::npos)
        return "restaurant.png";
    if (subtype.find("fast_food") != std::string::npos) return "restaurant.png";
    if (type.find("food") != std::string::npos) return "restaurant.png";

    // Inne
    if (subtype.find("bank") != std::string::npos) return "bank.png";
    if (subtype.find("hospital") != std::string::npos
        || type.find("hospital") != std::string::npos)
        return "hospital.png";
    if (subtype.find("pharmacy") != std::string::npos) return "pharmacy.png";
    if (subtype.find("post_office") != std::string::npos)
        return "post_office.png";
    if (subtype.find("shop") != std::string::npos
        || subtype.find("supermarket") != std::string::npos)
        return "supermarket.png";

    return "";
}

// --- HELPER: £ADOWANIE TEKSTUR ---
// Zmieniono: usuniêto basePath, szuka tylko w folderze images/ obok exe
osg::StateSet*
getSharedStateSet(const std::string& filename,
                  std::map<std::string, osg::ref_ptr<osg::StateSet>>& cache)
{
    if (filename.empty()) return nullptr;

    // 1. SprawdŸ cache
    auto it = cache.find(filename);
    if (it != cache.end())
    {
        return it->second.get();
    }

    // 2. £adowanie TYLKO z folderu images/ w katalogu roboczym (tam gdzie exe)
    std::string localPath = "images/" + filename;

    osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile(localPath);

    if (!image)
    {
        // Opcjonalny fallback: spróbuj bez folderu images (jeœli pliki s¹ luzem
        // obok exe)
        image = osgDB::readRefImageFile(filename);
    }

    if (!image)
    {
        std::cerr << "Warning: Texture not found in local 'images/' folder: "
                  << localPath << std::endl;
        return nullptr;
    }

    // 3. Tworzenie StateSet
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

    // 4. Cache
    cache[filename] = ss;
    return ss.get();
}

// --- TWORZENIE BILLBOARDU ---
osg::Billboard* createLabelNode(const LabelData& data,
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
        text->setText(data.name);
        text->setAlignment(osgText::Text::CENTER_BOTTOM);
        text->setAxisAlignment(osgText::Text::XZ_PLANE);
        text->setCharacterSize(3.5f);
        text->setBackdropType(osgText::Text::OUTLINE);
        text->setColor(osg::Vec4(1, 1, 1, 1));

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
            // TERAZ: wywo³anie bez file_path, szuka tylko lokalnie w images/
            iconSS = getSharedStateSet(iconFile, iconStateSets);
        }

        labelsGroup->addChild(createLabelNode(ld, iconSS));
    }

    std::cout << "--- LABELS: Utworzono " << labelsGroup->getNumChildren()
              << " etykiet." << std::endl;
    std::cout << "--- TEXTURES: Zaladowano " << iconStateSets.size()
              << " tekstur (szukano w ./images/)." << std::endl;

    return labelsGroup;
}