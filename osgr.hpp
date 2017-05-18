#ifndef AL_OMNI_STEREO_GRAPHICS_RENDERER1_H
#define AL_OMNI_STEREO_GRAPHICS_RENDERER1_H

#include "allocore/al_Allocore.hpp"
#include "allocore/io/al_Window.hpp"
#include "allocore/protocol/al_OSC.hpp"
#include "alloutil/al_FPS.hpp"
#include "alloutil/al_OmniStereo.hpp"
#include "alloutil/al_Simulator.hpp"  // DEVICE_SERVER_PORT_CONNECTION_US

/*
    A HACKED VERSION OF OmniStereoGraphicsRenderer.
    purpose is to use geometry shader with omni rendering.
*/

namespace al {

/*
   COPY of OmniStereoGrphicsRenderer.
   Parts that are only different are noted
*/
class OmniStereoGraphicsRenderer1 : public Window,
                                    public FPS,
                                    public OmniStereo::Drawable {
 public:
  OmniStereoGraphicsRenderer1();
  virtual ~OmniStereoGraphicsRenderer1();

  virtual void onDraw(Graphics& gl) {}
  virtual void onAnimate(al_sec dt) {}
  virtual bool onCreate();
  virtual bool onFrame();
  virtual void onDrawOmni(OmniStereo& omni);
  virtual std::string vertexCode();
  virtual std::string fragmentCode();
  virtual std::string geometryCode();

  void start();
  void initWindow(const Window::Dim& dims = Window::Dim(800, 400),
                  const std::string title = "OmniStereoGraphicsRenderer1",
                  double fps = 60,
                  Window::DisplayMode mode = Window::DEFAULT_BUF);
  void initOmni(std::string path = "");

  const Lens& lens() const { return mLens; }
  Lens& lens() { return mLens; }

  const Graphics& graphics() const { return mGraphics; }
  Graphics& graphics() { return mGraphics; }

  ShaderProgram& shader() { return mShader; }

  OmniStereo& omni() { return mOmni; }

  const std::string& hostName() const { return mHostName; }

  bool omniEnable() const { return bOmniEnable; }
  void omniEnable(bool b) { bOmniEnable = b; }

  osc::Send& oscSend() { return mOSCSend; }

 protected:
  const Nav& nav() const { return mNav; }
  Nav& nav() { return mNav; }

  OmniStereo mOmni;

  Lens mLens;
  Graphics mGraphics;

  osc::Send mOSCSend;
  Pose pose;

  ShaderProgram mShader;

  std::string mHostName;

  bool bOmniEnable;

  Nav mNav;
  NavInputControl mNavControl;
  StandardWindowKeyControls mStdControls;
};

inline void OmniStereoGraphicsRenderer1::start() {
  if (mOmni.activeStereo()) {
    Window::displayMode(Window::displayMode() | Window::STEREO_BUF);
  }

  create();

  if (mOmni.fullScreen()) {
    fullScreen(true);
    cursorHide(true);
  }

  Main::get().start();
}

inline OmniStereoGraphicsRenderer1::~OmniStereoGraphicsRenderer1() {}

inline OmniStereoGraphicsRenderer1::OmniStereoGraphicsRenderer1()
    : mNavControl(mNav), mOSCSend(12001), mOmni(2048, true) {

  bOmniEnable = true;
  mHostName = Socket::hostName();

  lens().near(0.01).far(40).eyeSep(0.03);
  nav().smooth(0.8);

  initWindow();
  initOmni();

  Window::append(mStdControls);
  Window::append(mNavControl);
}

inline void OmniStereoGraphicsRenderer1::initOmni(std::string path) {
  mOmni.configure(path, mHostName);
  if (mOmni.activeStereo()) {
    mOmni.mode(OmniStereo::ACTIVE).stereo(true);
  }
}

inline void OmniStereoGraphicsRenderer1::initWindow(const Window::Dim& dims,
                                                   const std::string title,
                                                   double fps,
                                                   Window::DisplayMode mode) {
  Window::dimensions(dims);
  Window::title(title);
  Window::fps(fps);
  Window::displayMode(mode);
}

inline bool OmniStereoGraphicsRenderer1::onCreate() {
  mOmni.onCreate();
  cout << "after mOmni.onCreate()" << endl;

  Shader shaderV, shaderF, shaderG;

  shaderV.source(vertexCode(), Shader::VERTEX).compile().printLog();
  shaderF.source(fragmentCode(), Shader::FRAGMENT).compile().printLog();
  shaderG.source(geometryCode(), Shader::GEOMETRY).compile().printLog();
  mShader.attach(shaderV);
  mShader.attach(shaderF);

  /*
      GEOMETRY SHADER POSSIBLE INPUTS and OUTPUTS
      input : GL_POINTS, GL_LINES, GL_LINES_ADJACENCY_EXT, GL_TRIANGLES,
              GL_TRIANGLES_ADJACENCY_EXT
      output: GL_POINTS, GL_LINE_STRIP, GL_TRIANGLE_STRIP
  */

  // modify the primitive types and numbers as needed
  mShader.setGeometryInputPrimitive(Graphics::POINTS);
  mShader.setGeometryOutputPrimitive(Graphics::POINTS);
  mShader.setGeometryOutputVertices(30);
  mShader.attach(shaderG);

  mShader.link(false).printLog();

  // mShader.begin();
  // mShader.uniform("lighting", 0.0);
  // mShader.uniform("texture", 0.0);
  // mShader.end();

  mShader.validate();
  mShader.printLog();

  return true;
}

inline bool OmniStereoGraphicsRenderer1::onFrame() {
  FPS::onFrame();
  nav().step();
  onAnimate(dt);

  Viewport vp(width(), height());
  if (bOmniEnable) {
    mOmni.onFrame(*this, lens(), pose, vp);
  } else {
    mOmni.onFrameFront(*this, lens(), pose, vp);
  }
  return true;
}

inline void OmniStereoGraphicsRenderer1::onDrawOmni(OmniStereo& omni) {
  graphics().error("start onDrawOmni");
  mShader.begin();
  mOmni.uniforms(mShader);
  graphics().error("start onDraw");
  onDraw(graphics());
  graphics().error("end onDraw");
  mShader.end();
  graphics().error("end onDrawOmni");
}

inline std::string OmniStereoGraphicsRenderer1::vertexCode() {
  return R"(

#version 120
#extension GL_EXT_gpu_shader4 : require

uniform sampler3D texSampler2;

uniform float animTime;
varying vec4 flux_v_to_g;
varying vec4 my_color;
varying float id_geo;
float TEX_WIDTH = 178.0;

void main(){
  float id = float(gl_VertexID);
  id_geo = id;
  vec3 coord = vec3(mod(id, TEX_WIDTH), floor(id/TEX_WIDTH), 0.);
  coord /= vec3(TEX_WIDTH);

  vec3 h = 0.5/vec3(TEX_WIDTH);
  coord += h;

  // select slice
  coord.z = animTime;
  // coord.z = 0.;

  vec4 data = texture3D(texSampler2, coord);

  vec4 pos = vec4(data.xy, 0., 1.);

  float theta = mod(id, TEX_WIDTH);
  float phi = id / TEX_WIDTH;

  theta  = (theta / TEX_WIDTH) * 2.0 * 3.14159265358979;
  phi  = (phi / TEX_WIDTH) * 3.14159265358979;

  float earthRad = 1.0;
  float xConv = (data.x)*(3.14/180.);
  float yConv = (data.y)*(3.14/180.);
  pos.x = -earthRad * cos(xConv) * cos(yConv);
  pos.y = earthRad * sin(xConv);
  pos.z = earthRad * cos(xConv) * sin(yConv);


  pos.x = -earthRad * cos(phi) * cos(theta);
  pos.y = earthRad * sin(phi);
  pos.z = earthRad * cos(phi) * sin(theta);

  flux_v_to_g = vec4(data.z,0.2,0.2,1.);
  
  my_color = 0.5 * (pos + 1.0);
  // Built-in varying that we will use in the fragment shader
  gl_TexCoord[0] = gl_MultiTexCoord0;

  // pass position to geometry shader
  // gl_Position = pos;
  // gl_Position = omni_render(gl_ModelViewMatrix * pos);

  float xy = mod(id, 31.0 * 31.0);
  float z = id / (31.0 * 31.0);
  float x = mod(xy, 31.0);
  float y = xy / (31.0);
  vec4 pos2 = vec4(x, y, z, 1.0);

  // gl_Position = omni_render(gl_ModelViewMatrix * pos2);
  gl_Position = pos2;
}
)";
}

inline std::string OmniStereoGraphicsRenderer1::fragmentCode() { return R"(
#version 120

varying vec4 flux_g_to_f;
varying vec4 my_color2;

void main(){
  // gl_FragColor = flux_g_to_f;
  gl_FragColor = mix(flux_g_to_f, my_color2, 0.8);
}
)";
}

inline std::string OmniStereoGraphicsRenderer1::geometryCode() { return R"(
#version 120
#extension GL_EXT_geometry_shader4 : enable // it is extension in 120

uniform float omni_near;
uniform float omni_far;
uniform float omni_eye;
uniform float omni_radius;
uniform int omni_face;
vec4 omni_render(in vec4 vertex) {
  float l = length(vertex.xz);
  vec3 vn = normalize(vertex.xyz);
  // Precise formula.
  float displacement = omni_eye *
    (omni_radius * omni_radius -
       sqrt(l * l * omni_radius * omni_radius +
            omni_eye * omni_eye * (omni_radius * omni_radius - l * l))) /
    (omni_radius * omni_radius - omni_eye * omni_eye);
  // Approximation, safe if omni_radius / omni_eye is very large, which is true for the allosphere.
  // float displacement = omni_eye * (1.0 - l / omni_radius);
  // Displace vertex.
  vertex.xz += vec2(displacement * vn.z, displacement * -vn.x);

  // convert eye-space into cubemap-space:
  // GL_TEXTURE_CUBE_MAP_POSITIVE_X
  if (omni_face == 0) {
    vertex.xyz = vec3(-vertex.z, -vertex.y, -vertex.x);
  }
  // GL_TEXTURE_CUBE_MAP_NEGATIVE_X
  else if (omni_face == 1) {
    vertex.xyz = vec3(vertex.z, -vertex.y, vertex.x);
  }
  // GL_TEXTURE_CUBE_MAP_POSITIVE_Y
  else if (omni_face == 2) {
    vertex.xyz = vec3(vertex.x, vertex.z, -vertex.y);
  }
  // GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
  else if (omni_face == 3) {
    vertex.xyz = vec3(vertex.x, -vertex.z, vertex.y);
  }
  // GL_TEXTURE_CUBE_MAP_POSITIVE_Z
  else if (omni_face == 4) {
    vertex.xyz = vec3(vertex.x, -vertex.y, -vertex.z);
  }
  // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
  else {
    vertex.xyz = vec3(-vertex.x, -vertex.y, vertex.z);
  }

  // convert into screen-space:
  // simplified perspective projection since fovy = 90 and aspect = 1
  vertex.zw = vec2(
    (vertex.z * (omni_far + omni_near) + vertex.w * omni_far * omni_near * 2.) / (omni_near - omni_far),
    -vertex.z
  );

  return vertex;
}

uniform float animTime;
varying in vec4 flux_v_to_g[];
varying out vec4 flux_g_to_f;

varying in float id_geo[];


varying in vec4 my_color[];
varying out vec4 my_color2;

float rand(vec2 co){
  return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main(){
  flux_g_to_f = flux_v_to_g[0];
  

  for(int i = 0; i < gl_VerticesIn; ++i){

    my_color2 = my_color[i];

    //get flux value
    float gflux = flux_v_to_g[i].x;

    for(int j=0; j<gflux*10; j++){
      //add displacement to each vertex (more vertices for higher gflux value)

      vec2 randCoord = vec2(id_geo[0]/31561.) + vec2(float(j)/20.);
      float randx = rand(randCoord + vec2(.01, .02)); // /50.;
      float randy = rand(randCoord + vec2(.02, .03)); // /50.;
      float randz = rand(randCoord + vec2(.03, .04)); // /50.;
      vec3 yayRandom = vec3(randx,randy,randz);

      vec4 posGeo = vec4(gl_PositionIn[0].xyz + yayRandom, 1.);
      gl_Position = omni_render(gl_ModelViewMatrix * posGeo);
      EmitVertex();
    }
  }

  EndPrimitive();
}
)";
}

}  // al

#endif
