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

osg::Node* process_labels(osg::Matrixd& ltw, const std::string & file_path)
{
    std::string labels_file_path = file_path + "/osm_points.shp";

    // load the data
    osg::ref_ptr<osg::Node> labels_model = osgDB::readRefNodeFile(labels_file_path);
    if (!labels_model)
    {
        std::cout << "Cannot load file " << labels_file_path << std::endl;
        return nullptr;
    }

    ConvertFromGeoProjVisitor<false> cfgp;
    labels_model->accept(cfgp);

    WorldToLocalVisitor ltwv(ltw, true);
    labels_model->accept(ltwv);







    // GOOD LUCK!









    return labels_model.release();
}


