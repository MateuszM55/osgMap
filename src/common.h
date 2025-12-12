#ifndef COMMON_H
#define COMMON_H

namespace osgViewer { class Viewer; }


osg::Node* process_landuse(osg::Matrixd& ltw, osg::BoundingBox& wbb, const std::string & file_path);
osg::Node* process_water(osg::Matrixd& ltw, const std::string & file_path);
osg::Node* process_buildings(osg::Matrixd& ltw, const std::string & file_path);
osg::Node* process_roads(osg::Matrixd& ltw, const std::string & file_path);
osg::Node* process_labels(osg::Matrixd& ltw, const std::string & file_path);


extern osg::ref_ptr<osg::EllipsoidModel> ellipsoid;
extern osg::ref_ptr<osgViewer::Viewer> viewer;

////////////////////////////////////////////////////////////////////////////////

class WorldToLocalVisitor : public osg::NodeVisitor
{
    osg::Matrixd _mat;
    bool  _zeroH;

public:

    WorldToLocalVisitor(osg::Matrixd & m, bool zeroHeights=true) :
        osg::NodeVisitor(NodeVisitor::TRAVERSE_ALL_CHILDREN),
        _mat(m)
    {
        _mat = osg::Matrixd::inverse(_mat);
        _zeroH = zeroHeights;
    }
    //////////////////////////////////////////////////////////////////////////////
    void apply(osg::Node & node)
    {
        traverse(node);
    }
    //////////////////////////////////////////////////////////////////////////////
    void apply(osg::Geode & node)
    {

        if (!_zeroH)
        {
            for (unsigned i = 0; i < node.getNumDrawables(); i++)
            {
                osg::Geometry* geom = dynamic_cast <osg::Geometry*> (node.getDrawable(i));

                if (!geom) continue;

                osg::Vec3Array* verts = (osg::Vec3Array*) geom->getVertexArray();

                for (unsigned j = 0; j < verts->size(); j++)
                {
                    (*verts)[j] = _mat.preMult((*verts)[j]);
                }
            }
        }
        else
        {
            for (unsigned i = 0; i < node.getNumDrawables(); i++)
            {
                osg::Geometry* geom = dynamic_cast <osg::Geometry*> (node.getDrawable(i));

                if (!geom) continue;

                osg::Vec3Array* verts = (osg::Vec3Array*) geom->getVertexArray();

                for (unsigned j = 0; j < verts->size(); j++)
                {
                    (*verts)[j] = _mat.preMult((*verts)[j]);
                    (*verts)[j][2] = 0.f;
                }
            }
        }
    }
};

////////////////////////////////////////////////////////////////////////////////

template <bool use_normals>
class ConvertFromGeoProjVisitor : public osg::NodeVisitor
{
    bool _use_normals;

public:

    osg::BoundingBox _box;

public:

    ConvertFromGeoProjVisitor() : 
        osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
    {
        _box.init();
    }

    virtual void apply(osg::Geode& node)
    {
        for (unsigned int j = 0; j < node.getNumDrawables(); ++j)
        {
            osg::Geometry *geom = dynamic_cast<osg::Geometry *>(node.getDrawable(j));
            if (!geom) continue;

            osg::Vec3Array * verts = (osg::Vec3Array*) geom->getVertexArray();

            osg::Vec3Array * norms = nullptr;

            if constexpr (use_normals)
            {
                norms = (osg::Vec3Array*) geom->getNormalArray();
                if (!norms)
                {
                    norms = new osg::Vec3Array(verts->size());
                }
            }

            for (unsigned k = verts->size(); k--;)
            {
                osg::Vec3d geo = (*verts)[k];
                osg::Vec3d pos;
                ellipsoid->convertLatLongHeightToXYZ(osg::DegreesToRadians(geo[1]),
                    osg::DegreesToRadians(geo[0]), geo[2], pos[0], pos[1], pos[2]);

                (*verts)[k] = pos;

                _box.expandBy(pos);

                if constexpr (use_normals)
                {
                    (*norms)[k] = pos;
                    (*norms)[k].normalize();
                }
            }

            geom->setNormalArray(norms);
        }

        traverse(node);
    }
};

////////////////////////////////////////////////////////////////////////////////

class ComputeBoundsVisitor : public osg::NodeVisitor
{
public:

    ComputeBoundsVisitor( osg::BoundingBox & bb, const char* name=NULL ) : _bb(bb),
        osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
    {
        if ( name ) _name=name;
    }

    virtual void apply(osg::Geode& geo)
    {
        if ( !_name.empty() ) {
            if ( geo.getName().compare(_name) != 0 ) return;
        }

        osg::Matrix matrix = osg::computeLocalToWorld( this->getNodePath() );

        for ( unsigned j=0; j<geo.getNumDrawables(); ++j ) {

            osg::Geometry *geom = dynamic_cast<osg::Geometry *>( geo.getDrawable( j ) );
            if ( !geom  ) continue;
            osg::Vec3Array* verts = (osg::Vec3Array*)geom->getVertexArray();
            for ( unsigned k=0; k<verts->size(); k++ ) {
                _bb.expandBy ( matrix.preMult((*verts)[k]) );
            }
        }
    }

    osg::BoundingBox & _bb;
    std::string _name;
};
#endif // COMMON_H