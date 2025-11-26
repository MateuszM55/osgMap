#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>

#include <iostream>

#include "common.h"

using namespace osg;

osg::Node* process_landuse(osg::Matrixd& ltw, const std::string & file_path)
{
    std::string land_file_path = file_path + "/gis_osm_landuse_a_free_1.shp";

    // load the data
    osg::ref_ptr<osg::Node> land_model = osgDB::readRefNodeFile(land_file_path);
    if (!land_model)
    {
        std::cout << "Cannot load file " << land_file_path << std::endl;
        return nullptr;
    }

    osg::BoundingBox mgbb;

    ComputeBoundsVisitor cbv(mgbb);
    land_model->accept(cbv);

    ellipsoid->computeLocalToWorldTransformFromLatLongHeight(
        osg::DegreesToRadians(mgbb.center().y()),
        osg::DegreesToRadians(mgbb.center().x()), 0.0, ltw);


    ConvertFromGeoProjVisitor<true> cfgp;
    land_model->accept(cfgp);

    WorldToLocalVisitor ltwv(ltw, true);
    land_model->accept(ltwv);







    // GOOD LUCK!









    return land_model.release();
}
