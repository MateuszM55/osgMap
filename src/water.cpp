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

osg::Node* process_water(osg::Matrixd& ltw, const std::string & file_path)
{
    std::string water_file_path = file_path + "/gis_osm_water_a_free_1.shp";

    // load the data
    osg::ref_ptr<osg::Node> water_model = osgDB::readRefNodeFile(water_file_path);
    if (!water_model)
    {
        std::cout << "Cannot load file " << water_file_path << std::endl;
        return nullptr;
    }

    ConvertFromGeoProjVisitor<true> cfgp;
    water_model->accept(cfgp);

    WorldToLocalVisitor ltwv(ltw, true);
    water_model->accept(ltwv);







    // GOOD LUCK!









    return water_model.release();
}
