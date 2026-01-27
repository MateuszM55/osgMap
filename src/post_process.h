#pragma once

#include <vector>
#include <functional>

#include <osg/Geode>
#include <osg/Camera>
#include <osg/Texture2D>
#include <osg/Projection>
#include <osgGA/GUIEventHandler>

namespace osgMap::postfx {

/**************************************************************************************************/

class PostProcessor;

class Layer {
public:
    Layer(osg::ref_ptr<osg::Texture2D>& in_color_texture,
          osg::ref_ptr<osg::Texture2D>& out_color_texture,
          osg::ref_ptr<osg::Texture2D>& depth_texture,
          const std::string& frag_filename, PostProcessor* parent);
    virtual ~Layer(void) = 0;

    void resize(int width, int height);
    bool getActive(void)
    {
        bool val;
        m_is_active->get(val);
        return val;
    }
    inline void setActive(bool active) { m_is_active->set(active); }

protected:
    osg::ref_ptr<osg::Geode> m_render_plane;

private:
    static unsigned int s_layer_index;
    osg::ref_ptr<osg::Camera> m_camera;
    osg::ref_ptr<osg::Uniform> m_resolution;
    osg::ref_ptr<osg::Uniform> m_is_active;
};

/**************************************************************************************************/

class Passthrough final : public Layer {
public:
    Passthrough(osg::ref_ptr<osg::Texture2D>& in_color_texture,
                osg::ref_ptr<osg::Texture2D>& out_color_texture,
                osg::ref_ptr<osg::Texture2D>& depth_texture,
                PostProcessor* parent)
        : Layer(in_color_texture, out_color_texture, depth_texture,
                "passthrough.frag", parent)
    {}
    ~Passthrough(void) override {}
};

/**************************************************************************************************/

class UV final : public Layer {
public:
    UV(osg::ref_ptr<osg::Texture2D>& in_color_texture,
       osg::ref_ptr<osg::Texture2D>& out_color_texture,
       osg::ref_ptr<osg::Texture2D>& depth_texture, PostProcessor* parent)
        : Layer(in_color_texture, out_color_texture, depth_texture, "uv.frag",
                parent)
    {}
    ~UV(void) override {}
};

/**************************************************************************************************/

class FXAA final : public Layer {
public:
    struct Parameters
    {
        int number_search_steps = 8;
        float edge_threshold = 0.125f;
        float edge_threshold_min = 0.0625f;
        float blur_close_distance = 1.0f;
        float blur_far_distance = 1.5f;
    };

    FXAA(osg::ref_ptr<osg::Texture2D>& in_color_texture,
         osg::ref_ptr<osg::Texture2D>& out_color_texture,
         osg::ref_ptr<osg::Texture2D>& depth_texture, PostProcessor* parent)
        : Layer(in_color_texture, out_color_texture, depth_texture, "fxaa.frag",
                parent),
          m_edge_threshold(new osg::Uniform("u_edge_threshold", 0.125f)),
          m_edge_threshold_min(
              new osg::Uniform("u_edge_threshold_min", 0.0625f)),
          m_edge_search_steps(new osg::Uniform("u_edge_search_steps", 8)),
          m_blur_close_distance(new osg::Uniform("u_blur_close_dist", 1.0f)),
          m_blur_far_distance(new osg::Uniform("u_blur_far_dist", 1.5f))
    {
        osg::StateSet* state_set = m_render_plane->getOrCreateStateSet();
        state_set->addUniform(m_edge_threshold);
        state_set->addUniform(m_edge_threshold_min);
        state_set->addUniform(m_edge_search_steps);
        state_set->addUniform(m_blur_close_distance);
        state_set->addUniform(m_blur_far_distance);
    }
    ~FXAA(void) override {}

    void setParameters(const Parameters& params)
    {
        this->setEdgeThreshold(params.edge_threshold);
        this->setEdgeThresholdMin(params.edge_threshold_min);
        this->setNumberSearchSteps(params.number_search_steps);
        this->setBlurCloseDistance(params.blur_close_distance);
        this->setBlurFarDistance(params.blur_far_distance);
    }

    inline void setEdgeThreshold(float threshold)
    {
        m_edge_threshold->set(threshold);
    }

    inline void setEdgeThresholdMin(float threshold)
    {
        m_edge_threshold_min->set(threshold);
    }

    inline void setNumberSearchSteps(int num_search_steps)
    {
        m_edge_search_steps->set(num_search_steps);
    }

    inline void setBlurCloseDistance(float distance)
    {
        m_blur_close_distance->set(distance);
    }

    inline void setBlurFarDistance(float distance)
    {
        m_blur_far_distance->set(distance);
    }

private:
    osg::ref_ptr<osg::Uniform> m_edge_threshold;
    osg::ref_ptr<osg::Uniform> m_edge_threshold_min;
    osg::ref_ptr<osg::Uniform> m_edge_search_steps;
    osg::ref_ptr<osg::Uniform> m_blur_close_distance;
    osg::ref_ptr<osg::Uniform> m_blur_far_distance;
};

/**************************************************************************************************/

class DOF final : public Layer {
public:
    struct Parameters
    {
        float max_blur = 0.03f;
        float blur_ramp = 100.0f;
        float focus_range = 0.986f;
    };

    DOF(osg::ref_ptr<osg::Texture2D>& in_color_texture,
        osg::ref_ptr<osg::Texture2D>& out_color_texture,
        osg::ref_ptr<osg::Texture2D>& depth_texture, PostProcessor* parent)
        : Layer(in_color_texture, out_color_texture, depth_texture, "dof.frag",
                parent),
          m_max_blur(new osg::Uniform("u_max_blur", 0.03f)),
          m_blur_ramp(new osg::Uniform("u_blur_ramp", 100.0f)),
          m_focus_range(new osg::Uniform("u_focus_range", 0.986f))
    {
        osg::StateSet* state_set = m_render_plane->getOrCreateStateSet();
        state_set->addUniform(m_max_blur);
        state_set->addUniform(m_blur_ramp);
        state_set->addUniform(m_focus_range);
    }
    ~DOF(void) override {}

    void setParameters(const Parameters& params)
    {
        this->setMaxBlur(params.max_blur);
        this->setBlurRamp(params.blur_ramp);
        this->setFocusRange(params.focus_range);
    }

    inline void setMaxBlur(float max_blur) { m_max_blur->set(max_blur); }
    inline void setBlurRamp(float blur_ramp) { m_blur_ramp->set(blur_ramp); }
    inline void setFocusRange(float focus_range)
    {
        m_focus_range->set(focus_range);
    }

private:
    osg::ref_ptr<osg::Uniform> m_max_blur;
    osg::ref_ptr<osg::Uniform> m_blur_ramp;
    osg::ref_ptr<osg::Uniform> m_focus_range;
};

/**************************************************************************************************/

class Bloom final : public Layer {
public:
    struct Parameters
    {
        float threshold = 0.9f;
        float knee = 0.4f;
        float blur_step = 0.007f;
        float intensity = 2.0f;
    };

    Bloom(osg::ref_ptr<osg::Texture2D>& in_color_texture,
          osg::ref_ptr<osg::Texture2D>& out_color_texture,
          osg::ref_ptr<osg::Texture2D>& depth_texture, PostProcessor* parent)
        : Layer(in_color_texture, out_color_texture, depth_texture,
                "bloom.frag", parent),
          m_threshold(new osg::Uniform("u_threshold", 0.9f)),
          m_knee(new osg::Uniform("u_knee", 0.4f)),
          m_blur_step(new osg::Uniform("u_blur_step", 0.007f)),
          m_intensity(new osg::Uniform("u_bloom_intensity", 2.0f))
    {
        osg::StateSet* state_set = m_render_plane->getOrCreateStateSet();
        state_set->addUniform(m_threshold);
        state_set->addUniform(m_knee);
        state_set->addUniform(m_blur_step);
        state_set->addUniform(m_intensity);
    }
    ~Bloom(void) override {}

    void setParameters(const Parameters& params)
    {
        this->setThreshold(params.threshold);
        this->setKnee(params.knee);
        this->setBlurStep(params.blur_step);
        this->setIntensity(params.intensity);
    }

    inline void setThreshold(float threshold) { m_threshold->set(threshold); }
    inline void setKnee(float knee) { m_knee->set(knee); }
    inline void setBlurStep(float blur_step) { m_blur_step->set(blur_step); }
    inline void setIntensity(float intensity) { m_intensity->set(intensity); }

private:
    osg::ref_ptr<osg::Uniform> m_threshold;
    osg::ref_ptr<osg::Uniform> m_knee;
    osg::ref_ptr<osg::Uniform> m_blur_step;
    osg::ref_ptr<osg::Uniform> m_intensity;
};

/**************************************************************************************************/

class PostProcessor final : public osg::Group {
public:
    PostProcessor(osg::Group* scene);
    ~PostProcessor(void) = default;

    void resize(int width, int height);
    osgGA::GUIEventHandler* getResizeHandler(void);
    osg::Projection* getRenderPlaneProjection(void);

    template <typename T> T* getLayer(void);
    template <typename T> void pushLayer(void);
    template <typename T>
    osgGA::GUIEventHandler*
    getActivationHandler(osgGA::GUIEventAdapter::KeySymbol activation_key);

private:
    class ResizeHandler : public osgGA::GUIEventHandler {
    public:
        ResizeHandler(const std::function<bool(int, int)>& handler);
        bool handle(const osgGA::GUIEventAdapter& ea,
                    osgGA::GUIActionAdapter& aa);

    private:
        std::function<bool(int, int)> m_handler;
    };

    class ActivationHandler : public osgGA::GUIEventHandler {
    public:
        ActivationHandler(
            const std::function<bool(osgGA::GUIEventAdapter::KeySymbol)>&
                handler);
        bool handle(const osgGA::GUIEventAdapter& ea,
                    osgGA::GUIActionAdapter& aa);

    private:
        std::function<bool(osgGA::GUIEventAdapter::KeySymbol)> m_handler;
    };

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
    std::vector<osg::ref_ptr<osg::Texture2D>> m_buffers;
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


    m_buffers.push_back(new osg::Texture2D);
    m_buffers.back()->setInternalFormat(GL_RGBA);
    m_buffers.back()->setFilter(osg::Texture::MIN_FILTER,
                                osg::Texture::NEAREST);
    m_buffers.back()->setFilter(osg::Texture::MAG_FILTER,
                                osg::Texture::NEAREST);

    int buffer_in = m_layers.size() > 0 ? m_layers.size() + 1 : 0;
    int buffer_out = m_layers.size() + 2;

    Layer* new_layer = new T(m_buffers[buffer_in], m_buffers[buffer_out],
                             m_buffers[Buffer::DEPTH_BUFFER], this);

    m_layers.push_back(new_layer);
    m_render_plane->getOrCreateStateSet()->setTextureAttributeAndModes(
        0, m_buffers[buffer_out].get());
}

/**************************************************************************************************/

template <typename T>
osgGA::GUIEventHandler* PostProcessor::getActivationHandler(
    osgGA::GUIEventAdapter::KeySymbol activation_key)
{
    static_assert(
        std::is_base_of<Layer, T>::value,
        "Template argument must be derived from osgMap::postfx::Layer class");

    return new ActivationHandler(
        [this, activation_key](osgGA::GUIEventAdapter::KeySymbol key) -> bool {
            Layer* layer = nullptr;
            if (activation_key != key || !(layer = this->getLayer<T>()))
            {
                return false;
            }
            layer->setActive(!layer->getActive());
            return true;
        });
}

/**************************************************************************************************/
};