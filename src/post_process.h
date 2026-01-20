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
    friend class PostProcessor;

public:
    Layer(osg::ref_ptr<osg::Texture2D>& in_color_texture,
          osg::ref_ptr<osg::Texture2D>& out_color_texture,
          osg::ref_ptr<osg::Texture2D>& depth_texture,
          const std::string& vert_filename, const std::string& frag_filename);
    virtual ~Layer(void) = 0;

    virtual void resize(int width, int height);

protected:
    osg::ref_ptr<osg::Camera> m_camera;
    osg::ref_ptr<osg::Geode> m_render_plane;
};

/**************************************************************************************************/

class Passthrough final : public Layer {
public:
    Passthrough(osg::ref_ptr<osg::Texture2D>& in_color_texture,
                osg::ref_ptr<osg::Texture2D>& out_color_texture,
                osg::ref_ptr<osg::Texture2D>& depth_texture)
        : Layer(in_color_texture, out_color_texture, depth_texture,
                "passthrough.vert", "passthrough.frag")
    {}
    ~Passthrough(void) override {}
};

/**************************************************************************************************/

class UV final : public Layer {
public:
    UV(osg::ref_ptr<osg::Texture2D>& in_color_texture,
       osg::ref_ptr<osg::Texture2D>& out_color_texture,
       osg::ref_ptr<osg::Texture2D>& depth_texture)
        : Layer(in_color_texture, out_color_texture, depth_texture,
                "passthrough.vert", "uv.frag")
    {}
    ~UV(void) override {}
};

/**************************************************************************************************/

class FXAA final : public Layer {
public:
    FXAA(osg::ref_ptr<osg::Texture2D>& in_color_texture,
         osg::ref_ptr<osg::Texture2D>& out_color_texture,
         osg::ref_ptr<osg::Texture2D>& depth_texture)
        : Layer(in_color_texture, out_color_texture, depth_texture,
                "passthrough.vert", "fxaa.frag")
    {
        m_render_plane->getOrCreateStateSet()->addUniform(
            new osg::Uniform("u_resolution", osg::Vec2(0.0f, 0.0f)));
    }
    ~FXAA(void) override {}

    void resize(int width, int height) override
    {
        Layer::resize(width, height);
        m_render_plane->getOrCreateStateSet()
            ->getUniform("u_resolution")
            ->set(osg::Vec2((float)width, (float)height));
    }
};

/**************************************************************************************************/

class DOF final : public Layer {
public:
    DOF(osg::ref_ptr<osg::Texture2D>& in_color_texture,
        osg::ref_ptr<osg::Texture2D>& out_color_texture,
        osg::ref_ptr<osg::Texture2D>& depth_texture)
        : Layer(in_color_texture, out_color_texture, depth_texture,
                "passthrough.vert", "dof.frag")
    {}
    ~DOF(void) override {}
};

/**************************************************************************************************/

class PostProcessor final : public osg::Group {
public:
    PostProcessor(osg::Group* scene);
    ~PostProcessor(void) = default;

    void resize(int width, int height);
    osg::Projection* getRenderPlaneProjection(void);

    template <typename T> T* getLayer(void);
    template <typename T> void pushLayer(void);

private:
    enum Buffer
    {
        FRAME_BUFFER = 0,
        DEPTH_BUFFER = 1,
        COLOR_BUFFER_A = 2,
        COLOR_BUFFER_B = 3
    };

    std::vector<Layer*> m_layers;

    osg::ref_ptr<osg::Camera> m_camera;
    osg::ref_ptr<osg::Geode> m_render_plane;
    osg::ref_ptr<osg::Texture2D> m_buffers[4];
};

/**************************************************************************************************/

template <typename T> T* PostProcessor::getLayer(void)
{
    static_assert(
        std::is_base_of<Layer, T>::value,
        "Template argument must be derived from osgMap::postfx::Layer class");

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

template <typename T> void PostProcessor::pushLayer(void)
{
    static_assert(
        std::is_base_of<Layer, T>::value,
        "Template argument must be derived from osgMap::postfx::Layer class");

    int buffer_in = m_layers.size() & 0x1 ? COLOR_BUFFER_B : COLOR_BUFFER_A;
    int buffer_out =
        buffer_in == COLOR_BUFFER_A ? COLOR_BUFFER_B : COLOR_BUFFER_A;
    buffer_in = !m_layers.size() ? Buffer::FRAME_BUFFER : buffer_in;

    Layer* new_layer = new T(m_buffers[buffer_in], m_buffers[buffer_out],
                             m_buffers[Buffer::DEPTH_BUFFER]);

    new_layer->m_camera->setRenderOrder(osg::Camera::PRE_RENDER,
                                        m_layers.size() + 1);
    this->addChild(new_layer->m_camera);
    m_layers.push_back(new_layer);
    m_render_plane->getOrCreateStateSet()->setTextureAttributeAndModes(
        0, m_buffers[buffer_out].get());
}

/**************************************************************************************************/

};