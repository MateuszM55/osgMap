#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>
#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/Depth>
#include <osg/Material>
#include <osg/StateSet>
#include <osg/Texture2D>
#include <osg/TexGen>
#include <osgSim/ShapeAttribute>
#include <osg/Program>
#include <osg/Shader>
#include <osg/Uniform>
#include <osg/Light>
#include <osg/LightSource>
#include <osg/ValueObject>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <filesystem>

#include "common.h"

using namespace osg;

typedef std::map<std::string, std::vector<osg::ref_ptr<osg::Node>>> Mapping;

void parse_meta_data(osg::Node* model, Mapping& umap)
{
    if (!model) return;
    osg::Group* group = model->asGroup();
    if (!group) return;
    for (unsigned i = 0; i < group->getNumChildren(); i++)
    {
        osg::Node* kido = group->getChild(i);
        if (!kido) continue;
        osgSim::ShapeAttributeList* sal =
            (osgSim::ShapeAttributeList*)kido->getUserData();
        if (!sal) continue;
        for (unsigned j = 0; j < sal->size(); j++)
        {
            if ((*sal)[j].getName().find("fclass") != std::string::npos)
            {
                if ((*sal)[j].getType() == osgSim::ShapeAttribute::STRING)
                {
                    umap[(*sal)[j].getString()].push_back(kido);
                }
            }
        }
    }
}

void process_background(osg::Node* land_model)
{
    osg::BoundingSphere bound = land_model->computeBound();
    osg::Geometry* bg =
        osg::createTexturedQuadGeometry(
            -osg::X_AXIS * bound.radius() - osg::Y_AXIS * bound.radius(),
            osg::X_AXIS * bound.radius() * 2, osg::Y_AXIS * bound.radius() * 2, 1000.f, 1000.f);

    osg::Image * img = osgDB::readImageFile ( "images/grass.dds" );
    osg::Texture2D * texture = new osg::Texture2D ( img );
    texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
    texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
    texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    texture->setUseHardwareMipMapGeneration(true);
    bg->getOrCreateStateSet()->setTextureAttributeAndModes(0, texture);

    bg->getOrCreateStateSet()->setAttributeAndModes(
        new osg::Depth(osg::Depth::LESS, 0, 1, false));
    bg->getOrCreateStateSet()->setRenderBinDetails(-11, "RenderBin");
    bg->getOrCreateStateSet()->setNestRenderBins(false);


    if (dynamic_cast<osg::Geode*>(land_model))
        land_model->asGeode()->addDrawable(bg);
    else
    {
        osg::Geode* geode=new osg::Geode;
        land_model->asGroup()->addChild(geode);
        geode->addDrawable(bg);
    }
}

void apply_texture(osg::StateSet* ss, const std::string& path)
{
    if (!ss) return;
    osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile(path);
    if (image.valid())
    {
        osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D;
        texture->setImage(image);

        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
        texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);

        texture->setFilter(osg::Texture::MIN_FILTER,
                           osg::Texture::LINEAR_MIPMAP_LINEAR);
        texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        texture->setUseHardwareMipMapGeneration(true);

        ss->setTextureAttributeAndModes(0, texture,
                                        osg::StateAttribute::ON
                                            | osg::StateAttribute::OVERRIDE);

        osg::ref_ptr<osg::TexGen> tg = new osg::TexGen;
        tg->setMode(osg::TexGen::OBJECT_LINEAR);

        tg->setPlane(osg::TexGen::S, osg::Plane(0.2, 0.0, 0.0, 0.0));
        tg->setPlane(osg::TexGen::T, osg::Plane(0.0, 0.2, 0.0, 0.0));

        ss->setTextureAttributeAndModes(
            0, tg, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    }
}

// Shader dla betonu (statyczny)
void setup_standard_shader(osg::StateSet* ss)
{
    if (!ss) return;
    osg::ref_ptr<osg::Shader> vert =
        osgDB::readRefShaderFile(osg::Shader::VERTEX, "shaders/standard.vert");
    osg::ref_ptr<osg::Shader> frag = osgDB::readRefShaderFile(
        osg::Shader::FRAGMENT, "shaders/standard.frag");
    if (vert.valid() && frag.valid())
    {
        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(vert);
        program->addShader(frag);
        ss->setAttributeAndModes(
            program, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        ss->addUniform(new osg::Uniform("baseTexture", 0));
        ss->addUniform(new osg::Uniform("texCoordScale", 0.02f));
        ss->addUniform(new osg::Uniform("animStrength", 0.0f));
        ss->addUniform(new osg::Uniform("animSpeed", 0.0f));
    }
}

// Shader dla roslinnosci (falowanie)
void setup_wind_shader(osg::StateSet* ss)
{
    if (!ss) return;
    osg::ref_ptr<osg::Shader> vert =
        osgDB::readRefShaderFile(osg::Shader::VERTEX, "shaders/wind.vert");
    osg::ref_ptr<osg::Shader> frag =
        osgDB::readRefShaderFile(osg::Shader::FRAGMENT, "shaders/wind.frag");
    if (vert.valid() && frag.valid())
    {
        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(vert);
        program->addShader(frag);
        ss->setAttributeAndModes(
            program, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        ss->addUniform(new osg::Uniform("baseTexture", 0));
        ss->addUniform(new osg::Uniform("texCoordScale", 0.01f));
        ss->addUniform(new osg::Uniform("animStrength", 0.02f));
        ss->addUniform(new osg::Uniform("animSpeed", 0.8f));
    }
}

// Shader dla cmentarza (Parallax 3D)
void setup_parallax_shader(osg::StateSet* ss)
{
    if (!ss) return;
    osg::ref_ptr<osg::Shader> vert =
        osgDB::readRefShaderFile(osg::Shader::VERTEX, "shaders/parallax.vert");
    osg::ref_ptr<osg::Shader> frag = osgDB::readRefShaderFile(
        osg::Shader::FRAGMENT, "shaders/parallax.frag");
    osg::ref_ptr<osg::Image> h_img =
        osgDB::readRefImageFile("images/cemetery_height.dds");

    if (vert.valid() && frag.valid() && h_img.valid())
    {
        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(vert);
        program->addShader(frag);
        ss->setAttributeAndModes(
            program, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

        osg::ref_ptr<osg::Texture2D> h_tex = new osg::Texture2D(h_img);
        h_tex->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
        h_tex->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
        h_tex->setFilter(osg::Texture::MIN_FILTER,
                         osg::Texture::LINEAR_MIPMAP_LINEAR);
        h_tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        h_tex->setUseHardwareMipMapGeneration(true);

        ss->setTextureAttributeAndModes(
            1, h_tex, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

        ss->addUniform(new osg::Uniform("baseTexture", 0));
        ss->addUniform(new osg::Uniform("heightMap", 1));
        ss->addUniform(new osg::Uniform("texCoordScale", 0.05f));
        ss->addUniform(new osg::Uniform("animStrength", 0.0f));
        ss->addUniform(new osg::Uniform("animSpeed", 0.0f));
    }
}

osg::Node* process_landuse(osg::Matrixd& ltw, osg::BoundingBox& wbb,
                           const std::string& file_path)
{
    std::string shp_file_path = file_path + "/gis_osm_landuse_a_free_1.shp";

    std::error_code ec;
    uintmax_t fileSize = std::filesystem::file_size(shp_file_path, ec);
    if (ec)
    {
        std::cout << "Blad: Nie mozna odczytac rozmiaru pliku " << shp_file_path
                  << std::endl;
        return nullptr;
    }

    std::string cacheFileName = "landuse_" + std::to_string(fileSize) + ".osgb";

    if (std::filesystem::exists(cacheFileName))
    {
        std::cout << "Znaleziono cache Landuse [" << cacheFileName
                  << "]. Wczytywanie..." << std::endl;

        osg::ref_ptr<osg::Node> cachedNode = osgDB::readNodeFile(cacheFileName);

        if (cachedNode.valid())
        {

            if (cachedNode->getUserValue("ltw_matrix", ltw))
            {}
            else
            {
                std::cout << "Ostrzezenie: Cache nie zawiera macierzy LTW!"
                          << std::endl;
            }

            osg::Vec3f minBB, maxBB;
            if (cachedNode->getUserValue("wbb_min", minBB)
                && cachedNode->getUserValue("wbb_max", maxBB))
            {
                wbb._min = minBB;
                wbb._max = maxBB;
            }
            else
            {
                std::cout << "Ostrzezenie: Cache nie zawiera WBB!" << std::endl;
            }

            return cachedNode.release();
        }
        std::cout << "Blad wczytywania cache, powrot do generowania..."
                  << std::endl;
    }

    std::cout << "--- Start Landuse (Urbanizacja - Generowanie) ---"
              << std::endl;

    osg::ref_ptr<osg::Node> land_model = osgDB::readRefNodeFile(shp_file_path);
    if (!land_model) return nullptr;

    osg::ref_ptr<osg::Group> land_group = new osg::Group;
    osg::ref_ptr<osg::Light> light = new osg::Light;
    light->setLightNum(1);
    light->setPosition(osg::Vec4(1.0, 1.0, 1.0, 0.0));
    light->setDiffuse(osg::Vec4(1.0, 1.0, 0.9, 1.0));
    light->setAmbient(osg::Vec4(0.4, 0.4, 0.4, 1.0));

    osg::ref_ptr<osg::LightSource> lightSource = new osg::LightSource;
    lightSource->setLight(light);
    land_group->addChild(lightSource);
    land_group->addChild(land_model);

    osg::StateSet* rootSS = land_group->getOrCreateStateSet();
    rootSS->setMode(GL_LIGHTING,
                    osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    rootSS->setMode(GL_LIGHT1, osg::StateAttribute::ON);
    rootSS->setMode(GL_LIGHT0, osg::StateAttribute::OFF);

    osg::BoundingBox mgbb;
    ComputeBoundsVisitor cbv(mgbb);
    land_model->accept(cbv);
    if (ellipsoid.valid())
    {
        ellipsoid->computeLocalToWorldTransformFromLatLongHeight(
            osg::DegreesToRadians(mgbb.center().y()),
            osg::DegreesToRadians(mgbb.center().x()), 0.0, ltw);
    }
    ConvertFromGeoProjVisitor<true> cfgp;
    land_model->accept(cfgp);
    wbb = cfgp._box;
    WorldToLocalVisitor ltwv(ltw, true);
    land_model->accept(ltwv);

    Mapping umap;
    parse_meta_data(land_model, umap);
    for (Mapping::iterator it = umap.begin(); it != umap.end(); ++it)
    {
        std::string name = it->first;
        std::vector<osg::ref_ptr<osg::Node>>& nodes = it->second;
        osg::ref_ptr<osg::StateSet> ss = new osg::StateSet();

        // === WARSTWA 0-3: TERENY MIEJSKIE (na spodzie) ===
        if (name.find("residential") != std::string::npos)
        {
            apply_texture(ss, "images/concrete.dds");
            setup_standard_shader(ss);
        }
        else if (name.find("industrial") != std::string::npos)
        {
            apply_texture(ss, "images/concrete.dds");
            setup_standard_shader(ss);
        }
        else if (name.find("commercial") != std::string::npos)
        {
            apply_texture(ss, "images/concrete.dds");
            setup_standard_shader(ss);
        }
        else if (name.find("retail") != std::string::npos)
        {
            apply_texture(ss, "images/concrete.dds");
            setup_standard_shader(ss);
        }
        // === WARSTWA 4-6: ROLNICTWO, WOJSKO, KAMIENIOŁOMY ===
        else if (name.find("farmland") != std::string::npos)
        {
            apply_texture(ss, "images/farmland.dds");
            setup_standard_shader(ss);
        }
        else if (name.find("farmyard") != std::string::npos)
        {
            apply_texture(ss, "images/farmland.dds");
            setup_standard_shader(ss);
        }
        else if (name.find("quarry") != std::string::npos)
        {
            apply_texture(ss, "images/rock.dds");
            setup_standard_shader(ss);
        }
        else if (name.find("military") != std::string::npos)
        {
            apply_texture(ss, "images/military.dds");
            setup_standard_shader(ss);
        }
        // === WARSTWA 8-10: TRAWA, ŁĄKI, ZAROŚLA ===
        else if (name.find("grass") != std::string::npos)
        {
            apply_texture(ss, "images/grass.dds");
            setup_wind_shader(ss);
        }
        else if (name.find("meadow") != std::string::npos)
        {
            apply_texture(ss, "images/grass.dds");
            setup_wind_shader(ss);
        }
        else if (name.find("scrub") != std::string::npos)
        {
            apply_texture(ss, "images/scrub.dds");
            setup_wind_shader(ss);
        }
        else if (name.find("heath") != std::string::npos)
        {
            apply_texture(ss, "images/scrub.dds");
            setup_wind_shader(ss);
        }
        // === WARSTWA 12: LASY ===
        else if (name.find("forest") != std::string::npos)
        {
            apply_texture(ss, "images/forest.dds");
            setup_wind_shader(ss);
        }
        // === WARSTWA 13-15: SADY, DZIAŁKI, REZERWATY ===
        else if (name.find("orchard") != std::string::npos)
        {
            apply_texture(ss, "images/orchard.dds");
            setup_wind_shader(ss);
        }
        else if (name.find("nature_reserve") != std::string::npos)
        {
            apply_texture(ss, "images/orchard.dds");
            setup_wind_shader(ss);
        }
        else if (name.find("allotments") != std::string::npos)
        {
            apply_texture(ss, "images/allotments.dds");
            setup_wind_shader(ss);
        }
        // === WARSTWA 16-17: PARKI, TERENY REKREACYJNE ===
        else if (name.find("park") != std::string::npos)
        {
            apply_texture(ss, "images/grass.dds");
            setup_wind_shader(ss);
        }
        else if (name.find("recreation_ground") != std::string::npos)
        {
            apply_texture(ss, "images/sport_green.dds");
            setup_wind_shader(ss);
        }
        // === WARSTWA 18: CMENTARZ (najwyżej - parallax) ===
        else if (name.find("cemetery") != std::string::npos)
        {
            apply_texture(ss, "images/cemetery.dds");
            setup_parallax_shader(ss);
        }
        else
        {
            // Usun nierozpoznane geometrie z grafu sceny
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                while (nodes[i].valid() && nodes[i]->getNumParents())
                    nodes[i]
                        ->getParent(nodes[i]->getNumParents() - 1)
                        ->removeChild(nodes[i]);
            }
            continue;
        }
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i].valid()) nodes[i]->setStateSet(ss);
        }
    }
    land_model->getOrCreateStateSet()->setAttributeAndModes(
        new osg::Depth(osg::Depth::LESS, 0, 1, false));
    land_model->getOrCreateStateSet()->setRenderBinDetails(-10, "RenderBin");
    land_model->getOrCreateStateSet()->setNestRenderBins(false);

    osgUtil::Optimizer optimizer;
    optimizer.optimize(land_group, osgUtil::Optimizer::ALL_OPTIMIZATIONS);

    land_group->setUserValue("ltw_matrix", ltw);
    land_group->setUserValue("wbb_min", wbb._min);
    land_group->setUserValue("wbb_max", wbb._max);

    process_background(land_group);

    std::cout << "Zapisuje cache Landuse: " << cacheFileName << std::endl;
    osgDB::writeNodeFile(*land_group, cacheFileName);

    std::cout << "--- Koniec Landuse ---" << std::endl;
    return land_group.release();
}
