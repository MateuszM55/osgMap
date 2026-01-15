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

// GOOD LUCK! // Thanks, I'll need it

const char* vert_source = R"(
#version 330

layout (location = 0) in vec3 i_position;

out vec2 tex_coord;

void main() {
    gl_Position = vec4(i_position, 1.0);
    tex_coord = vec2(
        (i_position.x + 1.0) / 2.0,
        (i_position.y + 1.0) / 2.0
    );
}

)";

const char* frag_source = R"(
#version 330

uniform sampler2D color_texture;
uniform sampler2D depth_texture;

in vec2 tex_coord;

void main() {
    gl_FragColor = vec4(texture(depth_texture, tex_coord).r);
}

)";

/**************************************************************************************************/

PostProcessor::PostProcessor(
    osg::ref_ptr<osg::Group> parent
) : m_render_camera(new osg::Camera),
    m_render_plane(
    osg::createTexturedQuadGeometry(
        osg::Vec3(-1.0f, -1.0f, 0.0f),
        osg::Vec3( 2.0f,  0.0f, 0.0f),
        osg::Vec3( 0.0f,  2.0f, 0.0f))
    ),
    m_color_texture(new osg::Texture2D),
    m_depth_texture(new osg::Texture2D)
{
    parent->addChild(m_render_camera);

    osg::ref_ptr<osg::Geode> render_plane_geode = new osg::Geode;
    render_plane_geode->addDrawable(m_render_plane.get());

    osg::ref_ptr<osg::Program> program = new osg::Program;
    program->addShader(new osg::Shader(osg::Shader::VERTEX, vert_source));
    program->addShader(new osg::Shader(osg::Shader::FRAGMENT, frag_source));

    osg::StateSet* state_set = render_plane_geode->getOrCreateStateSet();
    state_set->setAttributeAndModes(program.get());

    state_set->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    state_set->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

    state_set->setTextureAttributeAndModes(0, m_color_texture.get());
    state_set->setTextureAttributeAndModes(1, m_depth_texture.get());
    state_set->addUniform(new osg::Uniform("color_texture", 0));
    state_set->addUniform(new osg::Uniform("depth_texture", 1));

    m_render_camera->setRenderOrder(osg::Camera::POST_RENDER);
    m_render_camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    m_render_camera->setViewMatrix(osg::Matrix::identity());
    m_render_camera->setProjectionMatrix(osg::Matrix::ortho2D(-1, 1, -1, 1));
    m_render_camera->setClearMask(0);
    m_render_camera->addChild(render_plane_geode.get());

    m_color_texture->setInternalFormat(GL_RGBA);
    m_color_texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::NEAREST);
    m_color_texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::NEAREST);

    m_depth_texture->setInternalFormat(GL_DEPTH_COMPONENT24);
    m_depth_texture->setSourceFormat(GL_DEPTH_COMPONENT);
    m_depth_texture->setSourceType(GL_FLOAT);
    m_depth_texture->setShadowComparison(false);
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

void PostProcessor::attachInputCamera(osg::ref_ptr<osg::Camera> camera) 
{
    camera->setRenderOrder(osg::Camera::PRE_RENDER);
    camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    camera->attach(osg::Camera::COLOR_BUFFER0, m_color_texture.get());
    camera->attach(osg::Camera::DEPTH_BUFFER, m_depth_texture.get());

    camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    camera->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    m_color_texture->setTextureWidth(camera->getViewport()->width());
    m_depth_texture->setTextureWidth(camera->getViewport()->width());
    m_color_texture->setTextureHeight(camera->getViewport()->height());
    m_depth_texture->setTextureHeight(camera->getViewport()->height());

    m_render_camera->setViewport(0, 0, camera->getViewport()->width(),
                                 camera->getViewport()->height());
}

/**************************************************************************************************/
