#include "post_process.h"

#include <string>
#include <functional>

#include <osg/Geometry>
#include <osg/MatrixTransform>

using namespace osg;
using namespace osgMap::postfx;

static std::string s_shader_path = SHADER_PATH;

// GOOD LUCK! // Thanks, I'll need it

/**************************************************************************************************/
/* MISC */
/**************************************************************************************************/

class ResizeHandler : public osgGA::GUIEventHandler {
public:
    ResizeHandler(const std::function<bool(int, int)>& handler);
    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa);

private:
    std::function<bool(int, int)> m_handler;
};

/**************************************************************************************************/

osg::Program* createProgram(const std::string& vert_filename,
                            const std::string& frag_filename);

/**************************************************************************************************/

osg::Geode* createRenderPlane(osg::ref_ptr<osg::Program>& program,
                              osg::ref_ptr<osg::Texture2D>& color_texture,
                              osg::ref_ptr<osg::Texture2D>& depth_texture);

/**************************************************************************************************/
/* LAYER */
/**************************************************************************************************/

unsigned int Layer::s_layer_index =
    1; // 0 is reserved for the PostProcessor class

/**************************************************************************************************/

Layer::Layer(osg::ref_ptr<osg::Texture2D>& in_color_texture,
             osg::ref_ptr<osg::Texture2D>& out_color_texture,
             osg::ref_ptr<osg::Texture2D>& depth_texture,
             const std::string& frag_filename, PostProcessor* parent)
    : m_camera(new osg::Camera)
{
    osg::ref_ptr<osg::Program> program =
        createProgram("passthrough.vert", frag_filename);
    m_render_plane =
        createRenderPlane(program, in_color_texture, depth_texture);

    m_camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    m_camera->attach(osg::Camera::COLOR_BUFFER0, out_color_texture.get());
    m_camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    m_camera->setViewMatrix(osg::Matrix::identity());
    m_camera->setProjectionMatrix(osg::Matrix::ortho2D(-1, 1, -1, 1));
    m_camera->setRenderOrder(osg::Camera::PRE_RENDER, s_layer_index++);
    m_camera->setAllowEventFocus(false);
    m_camera->setClearMask(0);
    m_camera->setInheritanceMask(m_camera->getInheritanceMask()
                                 & ~osg::Camera::CULL_MASK);
    m_camera->addChild(m_render_plane);
    parent->addChild(m_camera);
}

/**************************************************************************************************/

Layer::~Layer(void) = default;

/**************************************************************************************************/

void Layer::resize(int width, int height)
{
    m_camera->setViewport(0, 0, width, height);
    m_camera->resizeAttachments(width, height);
}

/**************************************************************************************************/
/* POST PROCESSOR */
/**************************************************************************************************/

PostProcessor::PostProcessor(osg::Group* scene): m_camera(new osg::Camera)
{
    for (int i = 0; i < 4; ++i)
    {
        m_buffers[i] = new osg::Texture2D;
        if (i == Buffer::DEPTH_BUFFER)
        {
            m_buffers[i]->setSourceType(GL_FLOAT);
            m_buffers[i]->setSourceFormat(GL_DEPTH_COMPONENT);
            m_buffers[i]->setInternalFormat(GL_DEPTH_COMPONENT24);
            m_buffers[i]->setShadowComparison(false);
        }
        else
        {
            m_buffers[i]->setInternalFormat(GL_RGBA);
            m_buffers[i]->setFilter(osg::Texture::MIN_FILTER,
                                    osg::Texture::NEAREST);
            m_buffers[i]->setFilter(osg::Texture::MAG_FILTER,
                                    osg::Texture::NEAREST);
        }
    }

    m_camera->setRenderOrder(osg::Camera::PRE_RENDER, 0);
    m_camera->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    m_camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    m_camera->setAllowEventFocus(false);
    m_camera->setInheritanceMask(m_camera->getInheritanceMask()
                                 & ~osg::Camera::CULL_MASK);
    m_camera->attach(osg::Camera::DEPTH_BUFFER,
                     m_buffers[Buffer::DEPTH_BUFFER].get());
    m_camera->attach(osg::Camera::COLOR_BUFFER0,
                     m_buffers[Buffer::FRAME_BUFFER].get());
    m_camera->addChild(scene);

    osg::ref_ptr<osg::Program> program =
        createProgram("passthrough.vert", "passthrough.frag");
    m_render_plane = createRenderPlane(program, m_buffers[Buffer::FRAME_BUFFER],
                                       m_buffers[Buffer::DEPTH_BUFFER]);

    this->addChild(m_camera);
}

/**************************************************************************************************/

void PostProcessor::resize(int width, int height)
{
    m_camera->setViewport(0, 0, width, height);
    m_camera->resizeAttachments(width, height);
    for (auto* layer : m_layers)
    {
        layer->resize(width, height);
    }
}

/**************************************************************************************************/

osgGA::GUIEventHandler* PostProcessor::getResizeHandler(void)
{
    return new ResizeHandler([this](int width, int height) -> bool {
        this->resize(width, height);
        return false;
    });
}

/**************************************************************************************************/

osg::Projection* PostProcessor::getRenderPlaneProjection(void)
{
    osg::ref_ptr<osg::MatrixTransform> model_view = new osg::MatrixTransform;
    model_view->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    model_view->setMatrix(osg::Matrixf::identity());
    model_view->addChild(m_render_plane);

    osg::Projection* projection = new osg::Projection;
    projection->setMatrix(osg::Matrixf::ortho2D(-1.0, 1.0, -1.0, 1.0));
    projection->addChild(model_view);

    return projection;
}

/**************************************************************************************************/
/* MISC */
/**************************************************************************************************/

ResizeHandler::ResizeHandler(const std::function<bool(int, int)>& handler)
    : m_handler(handler)
{}

/**************************************************************************************************/

bool ResizeHandler::handle(const osgGA::GUIEventAdapter& ea,
                           osgGA::GUIActionAdapter& aa)
{
    return ea.getEventType() & osgGA::GUIEventAdapter::RESIZE
        ? m_handler(ea.getWindowWidth(), ea.getWindowHeight())
        : false;
}

/**************************************************************************************************/

osg::Program* createProgram(const std::string& vert_filename,
                            const std::string& frag_filename)
{
    osg::Program* program = new osg::Program;
    program->addShader(osg::Shader::readShaderFile(
        osg::Shader::VERTEX, s_shader_path + vert_filename));
    program->addShader(osg::Shader::readShaderFile(
        osg::Shader::FRAGMENT, s_shader_path + frag_filename));
    return program;
}

/**************************************************************************************************/

osg::Geode* createRenderPlane(osg::ref_ptr<osg::Program>& program,
                              osg::ref_ptr<osg::Texture2D>& color_texture,
                              osg::ref_ptr<osg::Texture2D>& depth_texture)
{
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    vertices->push_back(osg::Vec3(-1.0f, -1.0f, 0.0f));
    vertices->push_back(osg::Vec3(1.0f, -1.0f, 0.0f));
    vertices->push_back(osg::Vec3(1.0f, 1.0f, 0.0f));
    vertices->push_back(osg::Vec3(-1.0f, -1.0f, 0.0f));
    vertices->push_back(osg::Vec3(1.0f, 1.0f, 0.0f));
    vertices->push_back(osg::Vec3(-1.0f, 1.0f, 0.0f));

    osg::ref_ptr<osg::Vec2Array> tex_coords = new osg::Vec2Array;
    tex_coords->push_back(osg::Vec2(0.0f, 0.0f));
    tex_coords->push_back(osg::Vec2(1.0f, 0.0f));
    tex_coords->push_back(osg::Vec2(1.0f, 1.0f));
    tex_coords->push_back(osg::Vec2(0.0f, 0.0f));
    tex_coords->push_back(osg::Vec2(1.0f, 1.0f));
    tex_coords->push_back(osg::Vec2(0.0f, 1.0f));

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    geometry->setVertexArray(vertices);
    geometry->setTexCoordArray(0, tex_coords.get(),
                               osg::Array::Binding::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 6));

    osg::Geode* plane = new osg::Geode;
    plane->addDrawable(geometry);

    osg::StateSet* state_set = plane->getOrCreateStateSet();
    state_set->setAttribute(program);
    state_set->setMode(GL_BLEND, osg::StateAttribute::OFF);
    state_set->setMode(GL_CULL_FACE, osg::StateAttribute::ON);
    state_set->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    state_set->addUniform(new osg::Uniform("color_texture", 0));
    state_set->addUniform(new osg::Uniform("depth_texture", 1));
    state_set->setTextureAttributeAndModes(0, color_texture.get());
    state_set->setTextureAttributeAndModes(1, depth_texture.get());

    return plane;
}

/**************************************************************************************************/
