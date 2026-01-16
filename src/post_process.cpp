#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>

#include <iostream>

#include "post_process.h"

using namespace osg;
using namespace osgMap::postfx;

static std::string s_shader_path = SHADER_PATH;

// GOOD LUCK! // Thanks, I'll need it

/**************************************************************************************************/
/* MISC                                                                                           */
/**************************************************************************************************/

osg::ref_ptr<osg::Program> createProgram(const std::string& vert_filename,
    const std::string& frag_filename)
{
    osg::ref_ptr<osg::Program> program = new osg::Program;
    program->addShader(osg::Shader::readShaderFile(
        osg::Shader::VERTEX, s_shader_path + vert_filename));
    program->addShader(osg::Shader::readShaderFile(
        osg::Shader::FRAGMENT, s_shader_path + frag_filename));
    return program;
}

/**************************************************************************************************/

osg::ref_ptr<osg::Geode>
createRenderPlane(osg::ref_ptr<osg::Program> program,
                  osg::ref_ptr<osg::Texture2D> color_texture,
                  osg::ref_ptr<osg::Texture2D> depth_texture)
{
    osg::ref_ptr<osg::Geode> render_plane_geode = new osg::Geode;
    render_plane_geode->addDrawable(osg::createTexturedQuadGeometry(
        osg::Vec3(-1.0f, -1.0f, 0.0f), osg::Vec3(2.0f, 0.0f, 0.0f),
        osg::Vec3(0.0f, 2.0f, 0.0f)));

    osg::StateSet* state_set = render_plane_geode->getOrCreateStateSet();
    state_set->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    state_set->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    state_set->setAttributeAndModes(program.get());
    state_set->setTextureAttributeAndModes(0, color_texture.get());
    state_set->setTextureAttributeAndModes(1, depth_texture.get());
    state_set->addUniform(new osg::Uniform("color_texture", 0));
    state_set->addUniform(new osg::Uniform("depth_texture", 1));

    return render_plane_geode;
}

/**************************************************************************************************/
/* LAYER                                                                                          */
/**************************************************************************************************/

Layer::Layer(osg::ref_ptr<osg::Texture2D> in_color_texture,
             osg::ref_ptr<osg::Texture2D> out_color_texture,
             osg::ref_ptr<osg::Texture2D> depth_texture,
             osg::ref_ptr<osg::Group> parent, int render_order,
             const std::string& vert_filename, const std::string& frag_filename)
    : m_camera(new osg::Camera)
{
    osg::ref_ptr<osg::Program> program = createProgram(vert_filename, frag_filename);
    osg::ref_ptr<osg::Geode> render_plane_geode = createRenderPlane(program, in_color_texture, depth_texture);

    m_camera->setRenderOrder(osg::Camera::PRE_RENDER, render_order);
    m_camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    m_camera->attach(osg::Camera::COLOR_BUFFER0, out_color_texture.get());
    m_camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    m_camera->setViewMatrix(osg::Matrix::identity());
    m_camera->setProjectionMatrix(osg::Matrix::ortho2D(-1, 1, -1, 1));
    m_camera->setClearMask(0);
    m_camera->addChild(render_plane_geode.get());

    parent->addChild(m_camera);
}

/**************************************************************************************************/

Layer::~Layer(void) = default;

/**************************************************************************************************/
/* POST PROCESSOR                                                                                 */
/**************************************************************************************************/

PostProcessor::PostProcessor(osg::ref_ptr<osg::Group> parent)
    : m_parent(parent), m_camera(new osg::Camera),
      m_frame_texture(new osg::Texture2D), m_depth_texture(new osg::Texture2D)
{
    for (int i = 0; i < 2; ++i)
    {
        m_color_textures[i] = new osg::Texture2D;
        m_color_textures[i]->setInternalFormat(GL_RGBA);
        m_color_textures[i]->setFilter(osg::Texture::MIN_FILTER,
                                      osg::Texture::NEAREST);
        m_color_textures[i]->setFilter(osg::Texture::MAG_FILTER,
                                      osg::Texture::NEAREST);
    }

    m_frame_texture->setInternalFormat(GL_RGBA);
    m_frame_texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::NEAREST);
    m_frame_texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::NEAREST);

    m_depth_texture->setSourceType(GL_FLOAT);
    m_depth_texture->setSourceFormat(GL_DEPTH_COMPONENT);
    m_depth_texture->setInternalFormat(GL_DEPTH_COMPONENT24);
    m_depth_texture->setShadowComparison(false);

    osg::ref_ptr<osg::Program> program =
        createProgram("passthrough.vert", "passthrough.frag");
    m_render_plane =
        createRenderPlane(program, m_color_textures[0], m_depth_texture);

    m_camera->setRenderOrder(osg::Camera::POST_RENDER);
    m_camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    m_camera->setViewMatrix(osg::Matrix::identity());
    m_camera->setProjectionMatrix(osg::Matrix::ortho2D(-1, 1, -1, 1));
    m_camera->setClearMask(0);
    m_camera->addChild(m_render_plane.get());

    parent->addChild(m_camera);
}

/**************************************************************************************************/

PostProcessor::~PostProcessor(void)
{
    for (auto* layer : m_layers)
    {
        delete layer;
    }
}

/**************************************************************************************************/

void PostProcessor::resize(int width, int height) {
    m_frame_texture->setTextureSize(width, height);
    m_depth_texture->setTextureSize(width, height);
    for (int i = 0; i < 2; ++i)
    {
        m_color_textures[i]->setTextureSize(width, height);
    }

    m_camera->setViewport(0, 0, width, height);
    for (auto* layer : m_layers)
    {
        layer->setViewport(0, 0, width, height);
    }
}

/**************************************************************************************************/

void PostProcessor::attachInputCamera(osg::ref_ptr<osg::Camera> camera)
{
    camera->setRenderOrder(osg::Camera::PRE_RENDER, 0);
    camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    camera->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    camera->attach(osg::Camera::DEPTH_BUFFER, m_depth_texture.get());
    camera->attach(osg::Camera::COLOR_BUFFER0, m_frame_texture.get());

    this->resize(camera->getViewport()->width(),
                 camera->getViewport()->height());
}

/**************************************************************************************************/
