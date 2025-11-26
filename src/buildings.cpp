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

osg::Node* process_buildings(osg::Matrixd& ltw, const std::string & file_path)
{
    std::string buildings_file_path = file_path + "/buildings_levels.shp";

    // load the data
    osg::ref_ptr<osg::Node> buildings_model = osgDB::readRefNodeFile(buildings_file_path);
    if (!buildings_model)
    {
        std::cout << "Cannot load file " << buildings_file_path << std::endl;
        return nullptr;
    }

    ConvertFromGeoProjVisitor<true> cfgp;
    buildings_model->accept(cfgp);

    WorldToLocalVisitor ltwv(ltw, true);
    buildings_model->accept(ltwv);







    // GOOD LUCK!









    return buildings_model.release();
}

