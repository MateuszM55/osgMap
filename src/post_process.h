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
	
	Layer(void) = default;
    virtual ~Layer(void) = default;
	
private:

};

/**************************************************************************************************/

class DOF : public Layer {

};

/**************************************************************************************************/

class Bloom : public Layer {

};

/**************************************************************************************************/

class FXAA : public Layer {

};

/**************************************************************************************************/

class PostProcessor {
public:

	PostProcessor(osg::ref_ptr<osg::Group> parent);
	~PostProcessor(void);

	template <typename T> void pushLayer(void);
    template <typename T> T* getLayer(void);

	void attachInputCamera(osg::ref_ptr<osg::Camera> camera);

private:

    osg::ref_ptr<osg::Camera> m_render_camera;
	osg::ref_ptr<osg::Geometry> m_render_plane;
    osg::ref_ptr<osg::Texture2D> m_color_texture;
    osg::ref_ptr<osg::Texture2D> m_depth_texture;
	
	std::vector<Layer*> m_layers;

};

/**************************************************************************************************/

template <typename T> 
void PostProcessor::pushLayer(void) {
	static_assert(std::is_base_of<Layer, T>::value,
		"T must be derived from osgMap::postfx::Layer");

    m_layers.push_back(new T);
}

/**************************************************************************************************/

template <typename T>
T* PostProcessor::getLayer(void) {
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