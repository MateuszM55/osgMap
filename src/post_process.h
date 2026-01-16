#pragma once

#include <vector>

#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>

namespace osgMap::postfx {

/**************************************************************************************************/

class Layer {
public:
    Layer(osg::ref_ptr<osg::Texture2D> in_color_texture,
          osg::ref_ptr<osg::Texture2D> out_color_texture,
          osg::ref_ptr<osg::Texture2D> depth_texture,
          osg::ref_ptr<osg::Group> parent, int render_order,
          const std::string& vert_filename, const std::string& frag_filename);
    virtual ~Layer(void) = 0;

    inline void setViewport(int x, int y, int width, int height)
    {
        m_camera->setViewport(x, y, width, height);
    }

private:
    osg::ref_ptr<osg::Camera> m_camera;
};

/**************************************************************************************************/

class DOF : public Layer {
public:
    DOF(osg::ref_ptr<osg::Texture2D> in_color_texture,
        osg::ref_ptr<osg::Texture2D> out_color_texture,
        osg::ref_ptr<osg::Texture2D> depth_texture,
        osg::ref_ptr<osg::Group> parent, int render_order)
        : Layer(in_color_texture, out_color_texture, depth_texture, parent,
                render_order, "passthrough.vert", "dof.frag")
    {}
    ~DOF(void) override {}
};

/**************************************************************************************************/

class Bloom : public Layer {
public:
    Bloom(osg::ref_ptr<osg::Texture2D> in_color_texture,
          osg::ref_ptr<osg::Texture2D> out_color_texture,
          osg::ref_ptr<osg::Texture2D> depth_texture,
          osg::ref_ptr<osg::Group> parent, int render_order)
        : Layer(in_color_texture, out_color_texture, depth_texture, parent,
                render_order, "passthrough.vert", "bloom.frag")
    {}
    ~Bloom(void) override {}
};

/**************************************************************************************************/

class FXAA : public Layer {
public:
    FXAA(osg::ref_ptr<osg::Texture2D> in_color_texture,
         osg::ref_ptr<osg::Texture2D> out_color_texture,
         osg::ref_ptr<osg::Texture2D> depth_texture,
         osg::ref_ptr<osg::Group> parent, int render_order)
        : Layer(in_color_texture, out_color_texture, depth_texture, parent,
                render_order, "passthrough.vert", "fxaa.frag")
    {}
    ~FXAA(void) override {}
};

/**************************************************************************************************/

class PostProcessor {
public:
    PostProcessor(osg::ref_ptr<osg::Group> parent);
    ~PostProcessor(void);

    template <typename T> void pushLayer(void);
    template <typename T> T* getLayer(void);

    void resize(int width, int height);
    void attachInputCamera(osg::ref_ptr<osg::Camera> camera);

private:
    osg::ref_ptr<osg::Group> m_parent;
    osg::ref_ptr<osg::Camera> m_camera;
    osg::ref_ptr<osg::Geode> m_render_plane;
    osg::ref_ptr<osg::Texture2D> m_color_textures[2];
    osg::ref_ptr<osg::Texture2D> m_frame_texture;
    osg::ref_ptr<osg::Texture2D> m_depth_texture;

    std::vector<Layer*> m_layers;
};

/**************************************************************************************************/

template <typename T> void PostProcessor::pushLayer(void)
{
    static_assert(std::is_base_of<Layer, T>::value,
                  "T must be derived from osgMap::postfx::Layer");

    int in_idx = m_layers.size() & 0x1;
    int out_idx = (m_layers.size() + 1) & 0x1;

    m_layers.push_back(
        new T(!m_layers.size() ? m_frame_texture : m_color_textures[in_idx],
              m_color_textures[out_idx], m_depth_texture, m_parent,
              m_layers.size() + 1));
    m_render_plane->getOrCreateStateSet()->setTextureAttributeAndModes(
        0, m_color_textures[out_idx].get());
}

/**************************************************************************************************/

template <typename T> T* PostProcessor::getLayer(void)
{
    static_assert(std::is_base_of<Layer, T>::value,
                  "T must be derived from osgMap::postfx::Layer");

    for (auto* layer : m_layers)
    {
        if (auto* casted_layer = dynamic_cast<T*>(layer))
        {
            return casted_layer;
        }
    }

    return nullptr;
}

/**************************************************************************************************/

};