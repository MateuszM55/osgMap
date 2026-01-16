#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>

#include <osgSim/ShapeAttribute>

#include <osg/Geode>
#include <osg/Geometry>

#include <osg/CopyOp>
#include <osgUtil/Tessellator>

#include <osgDB/WriteFile>
#include <filesystem>
#include <system_error>

#include <iostream>
#include <vector>
#include <algorithm>

#include "common.h"

using namespace osg;


class GeomVisitor : public osg::NodeVisitor {
public:
    GeomVisitor(): osg::NodeVisitor(NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

    osg::Geode* getFirstGeode() const { return _firstGeode; }

    void apply(osg::Node& node) override
    {

        if (!_firstGeode) traverse(node);
    }

    void apply(osg::Geode& geode) override
    {
        if (!_firstGeode) _firstGeode = &geode;
    }

private:
    osg::Geode* _firstGeode = nullptr;
};


static void extrude_simple(osg::Geode* geode, osg::Geometry* baseGeom,
                           float hMeters)
{
    if (!geode || !baseGeom || hMeters <= 0.f) return;

    osg::Vec3Array* v =
        dynamic_cast<osg::Vec3Array*>(baseGeom->getVertexArray());
    if (!v || v->size() < 3) return;

    // Dach
    osg::ref_ptr<osg::Vec3Array> roofVerts = new osg::Vec3Array;
    roofVerts->reserve(v->size());
    for (unsigned i = 0; i < v->size(); ++i)
    {
        osg::Vec3 p = (*v)[i];
        p.z() += hMeters;
        roofVerts->push_back(p);
    }

    // Dach - kopiujemy TOPOLOGIĘ z baseGeom (PrimitiveSety), a tylko podnosimy
    // wierzchołki
    osg::ref_ptr<osg::Geometry> roof = new osg::Geometry;
    roof->setVertexArray(roofVerts.get());

    // Skopiuj wszystkie PrimitiveSety z baseGeom (indeksy/triangulacja)
    if (baseGeom->getNumPrimitiveSets() > 0)
    {
        for (unsigned int ps = 0; ps < baseGeom->getNumPrimitiveSets(); ++ps)
        {
            osg::PrimitiveSet* src = baseGeom->getPrimitiveSet(ps);
            if (!src) continue;

            osg::ref_ptr<osg::Object> clonedObj =
                src->clone(osg::CopyOp::DEEP_COPY_ALL);
            osg::PrimitiveSet* clonedPS =
                dynamic_cast<osg::PrimitiveSet*>(clonedObj.get());
            if (clonedPS) roof->addPrimitiveSet(clonedPS);
        }
    }
    else
    {
        // Fallback gdyby baseGeom nie miało primitive setów:
        roof->addPrimitiveSet(
            new osg::DrawArrays(GL_POLYGON, 0, roofVerts->size()));

        // Tessellacja dla wklęsłych polygonów
        osgUtil::Tessellator tess;
        tess.setTessellationType(osgUtil::Tessellator::TESS_TYPE_GEOMETRY);
        tess.setWindingType(osgUtil::Tessellator::TESS_WINDING_ODD);
        tess.retessellatePolygons(*roof);
    }

    // Normalna dachu
    osg::ref_ptr<osg::Vec3Array> roofN = new osg::Vec3Array;
    roofN->push_back(osg::Vec3(0.f, 0.f, 1.f));
    roof->setNormalArray(roofN.get(), osg::Array::BIND_OVERALL);


    // Ściany
    osg::ref_ptr<osg::Vec3Array> wallVerts = new osg::Vec3Array;
    wallVerts->reserve(v->size() * 6);

    for (unsigned i = 0; i < v->size(); ++i)
    {
        osg::Vec3 b0 = (*v)[i];
        osg::Vec3 b1 = (*v)[(i + 1) % v->size()];
        osg::Vec3 t0 = b0;
        t0.z() += hMeters;
        osg::Vec3 t1 = b1;
        t1.z() += hMeters;

        wallVerts->push_back(b0);
        wallVerts->push_back(b1);
        wallVerts->push_back(t1);
        wallVerts->push_back(b0);
        wallVerts->push_back(t1);
        wallVerts->push_back(t0);
    }

    osg::ref_ptr<osg::Geometry> walls = new osg::Geometry;
    walls->setVertexArray(wallVerts.get());
    walls->addPrimitiveSet(
        new osg::DrawArrays(GL_TRIANGLES, 0, wallVerts->size()));


    // normalne (na szybko, per trójkąt)
    osg::ref_ptr<osg::Vec3Array> wallN = new osg::Vec3Array;
    wallN->reserve(wallVerts->size());
    for (unsigned i = 0; i < wallVerts->size(); i += 3)
    {
        osg::Vec3 a = (*wallVerts)[i + 0];
        osg::Vec3 b = (*wallVerts)[i + 1];
        osg::Vec3 c = (*wallVerts)[i + 2];
        osg::Vec3 n = (b - a) ^ (c - a);
        n.normalize();
        wallN->push_back(n);
        wallN->push_back(n);
        wallN->push_back(n);
    }
    walls->setNormalArray(wallN.get(), osg::Array::BIND_PER_VERTEX);


    roof->setStateSet(baseGeom->getStateSet());
    walls->setStateSet(baseGeom->getStateSet());


    // Do testów koloru ścian
    // walls->getOrCreateStateSet()->setMode(GL_LIGHTING,
    // osg::StateAttribute::OFF);

    geode->addDrawable(walls.get());
    geode->addDrawable(roof.get());

    geode->dirtyBound();
}


void parse_meta_data(osg::Node* model)
{
    osg::Group* group = model ? model->asGroup() : nullptr;
    if (!group) return;

    // wysokość z metadanych
    std::vector<float> heights;
    heights.reserve(group->getNumChildren());

    for (unsigned i = 0; i < group->getNumChildren(); i++)
    {
        osg::Node* kido = group->getChild(i);
        osgSim::ShapeAttributeList* sal =
            dynamic_cast<osgSim::ShapeAttributeList*>(kido->getUserData());

        float height = 0.f;
        if (sal)
        {
            for (unsigned j = 0; j < sal->size(); j++)
            {
                if ((*sal)[j].getName().find("height") != std::string::npos)
                {
                    if ((*sal)[j].getType() == osgSim::ShapeAttribute::DOUBLE)
                        height =
                            float((*sal)[j].getDouble()) / 100.f; // cm -> m
                    else if ((*sal)[j].getType()
                             == osgSim::ShapeAttribute::INTEGER)
                        height = float((*sal)[j].getInt()) / 100.f; // cm -> m
                    break;
                }
            }
        }

        heights.push_back(height);
    }


    GeomVisitor gv;
    model->accept(gv);
    osg::Geode* geode = gv.getFirstGeode();
    if (!geode) return;

    // 3) zbierz geometrię
    std::vector<osg::ref_ptr<osg::Geometry>> geoms;
    geoms.reserve(geode->getNumDrawables());

    for (unsigned i = 0; i < geode->getNumDrawables(); ++i)
    {
        osg::Geometry* g = dynamic_cast<osg::Geometry*>(geode->getDrawable(i));
        if (g) geoms.push_back(g);
    }

    unsigned K = std::min((unsigned)heights.size(), (unsigned)geoms.size());

    std::cout << "[INFO] Extruding buildings...\n";

    // Extrusion
    for (unsigned i = 0; i < K; ++i)
    {
        float h = heights[i];
        if (h <= 0.f) continue;

        extrude_simple(geode, geoms[i].get(), h);
        geode->removeDrawable(geoms[i].get());

        // Postęp
        if (i > 0 && (i % 10000) == 0)
        {
            std::cout << "[INFO] Extruded " << i << " buildings\n";
        }
    }

    geode->dirtyBound();

    std::cout << "[INFO] Extrusion finished.\n";
}


osg::Node* process_buildings(osg::Matrixd& ltw, const std::string& file_path)
{
    const std::string buildings_file_path = file_path + "/buildings_levels.shp";

    // CACHE W KATALOGU ROBOCZYM (CWD)
    const std::filesystem::path cachePath =
        std::filesystem::current_path() / "buildings.osgb";

    // 1) Cache
    if (std::filesystem::exists(cachePath))
    {
        std::cout << "[BUILDINGS] Znaleziono cache [" << cachePath.string()
                  << "]\n";
        osg::ref_ptr<osg::Node> cached =
            osgDB::readRefNodeFile(cachePath.string());
        if (cached.valid())
        {
            std::cout << "[BUILDINGS] Cache OK, pomijam generowanie\n";
            return cached.release();
        }
        std::cout << "[BUILDINGS] Cache uszkodzony, generuje od nowa\n";
    }

    // 2) Wczytaj SHP
    osg::ref_ptr<osg::Node> buildings_model =
        osgDB::readRefNodeFile(buildings_file_path);
    if (!buildings_model.valid())
    {
        std::cout << "[BUILDINGS] Cannot load " << buildings_file_path << "\n";
        return nullptr;
    }

    // 3) Transformacje
    ConvertFromGeoProjVisitor<true> cfgp;
    buildings_model->accept(cfgp);

    WorldToLocalVisitor ltwv(ltw, true);
    buildings_model->accept(ltwv);

    // 4) Extrusion
    std::cout << "[BUILDINGS] Extruding buildings...\n";
    parse_meta_data(buildings_model.get());

    // 5) Zapis cache
    std::cout << "[BUILDINGS] Zapisuje cache: " << cachePath.string() << "\n";
    osgDB::writeNodeFile(*buildings_model, cachePath.string());

    return buildings_model.release();
}
