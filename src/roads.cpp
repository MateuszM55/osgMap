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

osg::Node* process_roads(osg::Matrixd& ltw, const std::string & file_path)
{
    std::string roads_file_path = file_path + "/gis_osm_roads_free_1.shp";

    // load the data
    osg::ref_ptr<osg::Node> roads_model = osgDB::readRefNodeFile(roads_file_path);
    if (!roads_model)
    {
        std::cout << "Cannot load file " << roads_file_path << std::endl;
        return nullptr;
    }

    ConvertFromGeoProjVisitor<true> cfgp;
    roads_model->accept(cfgp);

    WorldToLocalVisitor ltwv(ltw, true);
    roads_model->accept(ltwv);







    // GOOD LUCK!









    return roads_model.release();
}

