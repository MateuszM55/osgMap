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


static const char water_vert[] = R"(
#version 420 compatibility
varying vec4 ecp;                                                                          
varying vec3 normal;                                                                       
varying vec2 tcw;       
                                    
void main( )                                                           
{               

    vec4 vertex = gl_Vertex;
    normal = gl_Normal;                                                                      
    ecp = gl_ModelViewMatrix * vertex;                                                         
    tcw = vertex.xy * 0.001;                                                                   
                           
    gl_Position = gl_ModelViewProjectionMatrix * vertex;     
}
)";

static const char water_frag[] = R"(
#version 420 compatibility
in vec4 ecp;                                                                          
in vec2 tcw;                                                                          
in vec3 normal;                                                                       
uniform sampler2D sampler0;                                                                
uniform mat4 osg_ViewMatrix;                                                               
uniform float FresnelApproxPowFactor;                                                      
uniform float DynamicRange1;                                                               
uniform float DynamicRange2;                                                               
uniform float animacja;                                  
 
const vec4 WaterLight = vec4(0.2, 0.4, 1.0, 1.0);                                          
const vec4 WaterDark = vec4(0.1, 0.2, 0.4, 1.0);                                           
out vec4 fragColor;

vec3 rotateAroundAxis(vec3 v, vec3 axis, float angle) {
    float cosA = cos(angle);
    float sinA = sin(angle);
    return v * cosA + cross(axis, v) * sinA + axis * dot(axis, v) * (1.0 - cosA);
}

vec3 anyPerpendicular(vec3 n) {
    vec3 t = abs(n.z) < 0.99 ? vec3(0,0,1) : vec3(0,1,0);
    return normalize(cross(n, t));
}

vec3 orientedVector(vec3 axis, float tilt, float angle) {
    axis = normalize(axis);
    vec3 dir0 = anyPerpendicular(axis);
    vec3 tilted = rotateAroundAxis(axis, dir0, tilt);
    return rotateAroundAxis(tilted, axis, angle);
}

void DirectionalLight(in float glos,
                      in vec3 norm,
                      in vec3 ecPos,
                      inout vec4 ambient,
                      inout vec4 diffuse,
                      inout vec4 specular)
{
  float nDotVP;         // normal . light direction
  vec3 L = normalize(gl_LightSource[0].position.xyz);
  nDotVP = max(0.0, dot(norm, L));
  if (nDotVP > 0.0) {
    vec3 E = normalize(-ecPos);
    vec3 R = normalize(reflect(-L, norm));
    specular = pow(max(dot(R, E), 0.0), int(glos*127)+1.0) * gl_LightSource[0].specular; 
  }
  ambient  = gl_LightSource[0].ambient;
  diffuse  = gl_LightSource[0].diffuse * nDotVP;
}


void main (void)                                                 
{                                                                
  vec3 NH = texture2D(sampler0, tcw*0.5).xyz * vec3(2.0) - vec3(1.0);
vec3 N = normal;                                                                         
  vec3 T = normalize(cross(vec3(0, 1, 0), N));                                             
  mat3 vmo = mat3(osg_ViewMatrix[0].xyz, osg_ViewMatrix[1].xyz, osg_ViewMatrix[2].xyz);    
  N = normalize(vmo * N);                                                                  
  T = normalize(vmo * T);                                                                  
  vec3 B = normalize(cross(N, T));                                                         
  mat3 tbn = mat3(T, B, N); 
                  
float phase = length(gl_FragCoord.xy)*NH.y;
float cangle = 1.5*animacja + phase;
vec3 n = orientedVector(NH, 0.2 * cos(cangle), cangle);

                                             
  N = tbn * n;                                                                            
  
  vec4 ambiCol = vec4(0.0), diffCol = vec4(0.0), specCol = vec4(0.0);
  float specularGlos = 0.01;



  DirectionalLight(specularGlos, N, ecp.xyz, ambiCol, diffCol, specCol);

  vec3 ecPosNorm = normalize(ecp.xyz);                                                     
  float NdotE = dot(N, -ecPosNorm);                                                          
  float fres = pow(abs(1.0 + NdotE), -FresnelApproxPowFactor);                             
  fres *= DynamicRange1;
  float integralPart = floor(fres);                                                        
  float fractionPart = fract(fres);                                                        
  vec4 color1;                                                                             
  vec4 color2;                                                                             
  color1.a = fractionPart;                                                                 
  color2.a = integralPart * DynamicRange2;                                                 
  color1.rgb = vec3(NdotE);                                                                
  vec4 resval = vec4((color2.a) + (color1.a * 0.5));                                       
  resval.rgb += ambiCol.rgb + diffCol.rgb * 
    mix(WaterLight.rgb, WaterDark.rgb, color1.rgb) + specCol.rgb;
  resval.a = 1.0;
  fragColor = resval;                                                                        
}
)";

class MyUpdateCallback : public osg::NodeCallback 
{
public:
    osg::ref_ptr<osg::Uniform> _ctime; 
    MyUpdateCallback(osg::Uniform* ct): _ctime(ct) {} 

    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
    {
        
            static double firstTime = osg::Timer::instance()->time_s();
            double nowTime = osg::Timer::instance()->time_s();

        //how to update uniform animacja
            
        

        _ctime->set((float)osg::Timer::instance()->time_s());
        traverse(node, nv);
    }
};


osg::Node* process_water(osg::Matrixd& ltw, const std::string & file_path)
{
    std::string water_file_path = file_path + "/gis_osm_water_a_free_1.shp";
osg::ref_ptr<osg::Uniform> _ctime = new osg::Uniform("animacja", 0.f);
    // load the data
    osg::ref_ptr<osg::Node> water_model = osgDB::readRefNodeFile(water_file_path);
    if (!water_model)
    {
        std::cout << "Cannot load file " << water_file_path << std::endl;
        return nullptr;
    }

    ConvertFromGeoProjVisitor<true> cfgp;
    water_model->accept(cfgp);

    WorldToLocalVisitor ltwv(ltw, true);
    water_model->accept(ltwv);


    




    // GOOD LUCK!


    osg::ref_ptr<osg::Texture2D> texture = 
        new osg::Texture2D(osgDB::readImageFile("Images/pnoise0.tga"));
    water_model->getOrCreateStateSet()->setTextureAttributeAndModes(0, texture);
    texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);

    texture->setUseHardwareMipMapGeneration(true);
    texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);

water_model->setUpdateCallback(new MyUpdateCallback(_ctime.get()));

    osg::Shader* vshader = new osg::Shader(osg::Shader::VERTEX, water_vert);
    osg::Shader* fshader = new osg::Shader(osg::Shader::FRAGMENT, water_frag);
    osg::Program* program = new osg::Program;
    program->addShader(vshader);
    program->addShader(fshader);
    water_model->getOrCreateStateSet()->setAttribute(program);
    water_model->getOrCreateStateSet()->addUniform(new osg::Uniform("sampler0", 0));
    water_model->getOrCreateStateSet()->addUniform(new osg::Uniform("DynamicRange1", 0.125f));
    water_model->getOrCreateStateSet()->addUniform(new osg::Uniform("DynamicRange2", 0.025f));
    water_model->getOrCreateStateSet()->addUniform(new osg::Uniform("FresnelApproxPowFactor", 3.f));
    water_model->getOrCreateStateSet()->addUniform(_ctime);

    
    



    return water_model.release();
}
