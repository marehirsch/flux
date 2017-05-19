
#include "Gamma/Gamma.h"
#include "Gamma/SamplePlayer.h"
#include "allocore/io/al_App.hpp"
#include "Cuttlebone/Cuttlebone.hpp"
#include "alloutil/al_Simulator.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>

using namespace al;
using namespace std;

#include "particle_snow_common_3dtex.hpp"  // XXX


const int NUM_POINTS = 31561;
const int TEX_WIDTH = 178;    // round up sqrt(CHUNK)
const int TEX_SIZE = TEX_WIDTH*TEX_WIDTH*4;  // square texture rgba

struct SafeSamplePlayer : gam::SamplePlayer<> {

  float operator[](uint32_t i) {

    if (i >= size()) {

      return 0.;
    } else {

      return gam::SamplePlayer<>::operator[](i);
    }
  }
};

string fullPathOrDie(string fileName, string whereToLook = ".") {
  SearchPaths searchPaths;
  searchPaths.addSearchPath(whereToLook);
  string filePath = searchPaths.find(fileName).filepath();
  assert(filePath != "");
  return filePath;
}


struct AlloApp : App, InterfaceServerClient {
  ShaderProgram shader;
  Light light;
  Mesh m;
  Mesh dataMesh;

  SafeSamplePlayer samplePlayer;

  cuttlebone::Maker<State, CHUNKSIZE> maker;
  State state;

  //vector<vector<Texture>> texture;
  vector<vector<float>> data;

  float csvPull[NUM_POINTS];

	GLuint tex3d1; // the reference to your opengl 3d texture
  vector<vector<float>> src;
  vector<float> texArray;

  float animTime = 0.0;
  int frameNum = 0;
	double time=0;
  float geoCoords[TEX_SIZE];

  AlloApp()
    : maker(Simulator::defaultBroadcastIP()),
      InterfaceServerClient(Simulator::defaultInterfaceServerIP()){
    state.frame = 0;

    printf("Tex width = %d, num pixels = %d, num elements = %d\n", TEX_WIDTH, TEX_WIDTH*TEX_WIDTH, TEX_SIZE);

    nav().pos(0, 0, 0);
    // light.pos(0, 0, 10);

    //addSphereWithTexcoords(m);
    // m.generateNormals(); // don't need normals if we don't use lights

    initAudio();
    initWindow();

    // set interface server nav/lens to App's nav/lens
    InterfaceServerClient::setNav(nav());
    InterfaceServerClient::setLens(lens());

    SearchPaths searchPaths;
    searchPaths.addAppPaths();
    searchPaths.addSearchPath("..");

    const char* soundFileName = "particle_snow_audio.aif";

    string soundFilePath = searchPaths.find(soundFileName).filepath();

    if (soundFilePath == "") {
      cerr << "FAIL: your sound file " << soundFileName
           << " was not found in the file path." << endl;
      exit(1);
    }
    cout << "Full path to your sound file is " << soundFilePath << endl;

    if (!samplePlayer.load(soundFilePath.c_str())) {
      cerr << "FAIL: your sound file did not load." << endl;
      exit(1);
    }

    // find and open the data file and die unless we have it open
    // ifstream f(fullPathOrDie("diatoms_fulltime-fixed-unfucked3.csv", ".."), ios::in);
    ifstream f(fullPathOrDie("diatoms_fulltime-fixed_old.csv", ".."), ios::in);
    if (!f.is_open()) {
      cout << "file not open" << endl;
      exit(1);
    }

    // for each line in the file
    string line;
    while (getline(f, line)) {
      // push an empty "row" on our vector of vectors
      data.push_back(vector<float>());

      // for each item in the line
      stringstream stream(line);
      float v;
      while (stream >> v) {
        // push each value onto the current row
        data.back().push_back(v);

        if (stream.peek() == ',') stream.ignore();
      }
    }

    // close the file - we're done
    f.close();

    if (data.size() != 31561) {
      cerr << "FAIL" << endl;
      cerr << data.size() << endl;
      exit(1);
    }

		//load subset of CSV into an array
    cout << "loading csv into texArray..." << endl;

		// our texture expects 178x178 or whatever, so let's make sure each slice is that big
		int filler_points = (TEX_WIDTH*TEX_WIDTH) - NUM_POINTS;

		//populate texArray with lat, long, flux values
		for(int column=2; column < 14; column++){
		      for (int row = 0; row < NUM_POINTS; row++) {
		        float src0(data[row][0]);
		        texArray.push_back(src0);
		        float src1(data[row][1]);
		        texArray.push_back(src1);
		        float srcColumn(data[row][column]);
		        texArray.push_back(srcColumn);
		        // Add alpha channel
		        texArray.push_back(1.0);

		        // if we're on the last entry, fill it up with more pixels
		        if (row == NUM_POINTS-1) {
		            for (int i = 0; i < filler_points; i++) {
		                for (int j = 0; j < 4; j++) {
		                    texArray.push_back(-1.0);
		                }
		            }
		        }
		    }
		}
		cout << "texArray size = " << texArray.size() << endl;
		cout << "TEX_WIDTH * TEX_WIDTH * 4 * 12 = " << TEX_WIDTH * TEX_WIDTH * 4 * 12<< endl;
    cout << "loaded csv into texArray!" << endl;


    // Create vertices for data
    for (int i = 0; i < TEX_WIDTH*TEX_WIDTH; i++) {
      dataMesh.vertex(0,0,0);
    }

    //Geometry inputs/outputs must be specified BEFORE compiling shader
    shader.setGeometryInputPrimitive(Graphics::POINTS);
    shader.setGeometryOutputPrimitive(Graphics::POINTS);
    shader.setGeometryOutputVertices(30);

    // Compile the vertex and fragment shaders to display the texture
    shader.compile(
    R"(
      #extension GL_EXT_gpu_shader4 : require
      uniform sampler3D texSampler2;
      // uniform sampler2D texSampler;

      uniform float animTime;
			varying vec4 flux_v_to_g;
      varying vec4 pos_v_to_g;
      // varying float id_geo;
      float TEX_WIDTH = 178.0;

      void main(){
        float id = float(gl_VertexID);
        // id_geo = id;
        vec3 coord = vec3(mod(id, TEX_WIDTH), floor(id/TEX_WIDTH), 0.);
        coord /= vec3(TEX_WIDTH);

        vec3 h = 0.5/vec3(TEX_WIDTH);
        coord += h;

        // select slice
        coord.z = animTime;
        // coord.z = 0.;

        vec4 data = texture3D(texSampler2, coord);
        // vec4 data = texture2D(texSampler, coord.xy);

        vec4 pos = vec4(data.xy, 0., 1.);

        float earthRad = 1.0;
        float xConv = (data.x)*(3.14/180.);
        float yConv = (data.y)*(3.14/180.);
        pos.x = -earthRad * cos(xConv) * cos(yConv);
        pos.y = earthRad * sin(xConv);
        pos.z = earthRad * cos(xConv) * sin(yConv);
        flux_v_to_g = vec4(data.z,0.2,0.2,1.);
        vec4 pos_v_to_g = pos;

        // Built-in varying that we will use in the fragment shader
        gl_TexCoord[0] = gl_MultiTexCoord0;

        // Set position as you normally do
        //gl_Position = gl_ModelViewProjectionMatrix * pos;
        gl_Position = pos;

      }
    )", R"(
			// A "sampler" is used to fetch texels from the texture
		  //uniform sampler3D texSampler;
			varying vec4 flux_g_to_f;

			void main(){
				vec4 flux = flux_g_to_f;
        //vec4 flux = vec4(1.,1.,1.,1.);
				//gl_FragColor = texture3D(texSampler, gl_TexCoord[0].xy);
				vec4 color = flux;
				gl_FragColor = color;

			}
		)", R"(
      #version 120
      #extension GL_EXT_geometry_shader4 : enable
      uniform float animTime;
      varying in vec4 flux_v_to_g[];
      varying out vec4 flux_g_to_f;
      // varying in float id_geo[];
      // vec2 randCoord;
      // float randx, randy, randz;
      float rand(vec2 co){
        return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
      }

      void main(){
        flux_g_to_f = flux_v_to_g[0];

        // for(int i = 0; i < gl_VerticesIn; ++i){
        //   //get flux value
        //   float gflux = flux_v_to_g[i].x;

        //   for(int j=0; j<gflux*10; j++){
        //     //add displacement to each vertex (more vertices for higher gflux value)
        //     randCoord = vec2(id_geo[0]/31561.) + vec2(float(j)/20.);
        //     randx = rand(randCoord + vec2(.01, .02))/50.;
        //     randy = rand(randCoord + vec2(.02, .03))/50.;
        //     randz = rand(randCoord + vec2(.03, .04))/50.;
        //     vec3 yayRandom = vec3(randx,randy,randz);
        //     // yayRandom = (yayRandom * vec3(2.))-vec3(1.);
        //     // yayRandom * .01;

        //     vec4 posGeo = vec4(gl_PositionIn[0].xyz + yayRandom, 1.);
        //     //vec4 posGeo = vec4(gl_PositionIn[0].xyz, 1.);
        //     //posGeo.xyz += yayRandom;


        //     // if(posGeo.x > gl_PositionIn[i].x + gCoords[j].x + 0.05 || posGeo.x < gl_PositionIn[i].x + gCoords[j].x - 0.05 ||
        //     //    posGeo.y > gl_PositionIn[i].y + gCoords[j].y + 0.05 || posGeo.y < gl_PositionIn[i].y + gCoords[j].y - 0.05 ||
        //     //    posGeo.z > gl_PositionIn[i].z + gCoords[j].z + 0.05 || posGeo.z < gl_PositionIn[i].z + gCoords[j].z - 0.05 ){
        //     //    posGeo = vec4(gl_PositionIn[i].xyz + gCoords[j], 1.);
        //     //    }
        //     gl_Position = gl_ModelViewProjectionMatrix * posGeo;
        //     EmitVertex();
        //   }
        // }

        vec4 posGeo = vec4(gl_PositionIn[0].xyz, 1.);
        gl_Position = gl_ModelViewProjectionMatrix * posGeo;
        EmitVertex();

        EndPrimitive();
      }
    )"
    );
  }

  virtual void onDraw(Graphics& g, const Viewpoint& v) {
			g.clearColor(0.1,0.1,0.1,1);
			g.clear(Graphics::COLOR_BUFFER_BIT);
			g.rotate(time*5, 0,1,0);
      g.pointSize(10);
      shader.begin();

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_3D, tex3d1);
				shader.uniform("animTime", animTime);
        shader.uniform("texSampler2", 0);
	      g.draw(dataMesh);

				glBindTexture(GL_TEXTURE_3D, 0);

      shader.end();

  }

  virtual void onAnimate(double dt) {
    while (InterfaceServerClient::oscRecv().recv()) {
    }

		if (frameNum == 0) {

			// Create 3d texture and load it with data
			glGenTextures(1, &tex3d1);
			glBindTexture(GL_TEXTURE_3D, tex3d1);
			// Set texture wrapping to GL_REPEAT
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			// Set texture filtering
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			// Load 3d texture with data
			glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, TEX_WIDTH, TEX_WIDTH, 12, 0, GL_RGBA, GL_FLOAT, texArray.data());
			glBindTexture(GL_TEXTURE_3D, 0);
			cout << "Created 3d texture!" << endl;
		}

    frameNum++;

    //XXX cuddlebone stuff
    //XXX THIS IS WHAT I'M SENDING TO STATE
    int index = 0;
    index++;

    state.pose = nav();
    maker.set(state);
    state.frame++;


    //animation timing
		if(animTime < 1.){
			animTime += 0.003;
		} else{
			animTime = 0.;
		}
    state.animTime = animTime;

		time += dt;
		if( time == 360){
			time = 0;
		}
  }

  // onMouseMove is when the mouse moves
  virtual void onMouseMove(const ViewpointWindow& w, const Mouse& m) {
    // normalize mouse position from -1.0 to 1.0
    float x = float(m.x()) / w.width() * 2.f - 1.f;
    float y = float(m.y()) / w.height();

    //animTime = y;
    // cout << animTime << endl;
  }

  void onKeyDown(const Keyboard& k) {
  }

  virtual void onSound(AudioIOData& io) {

    gam::Sync::master().spu(audioIO().fps());
    while (io()) {
      float s = samplePlayer();

      io.out(0) = io.out(1) = s;
    }
  }
  // other mouse callbacks
  //
  virtual void onMouseDown(const ViewpointWindow& w, const Mouse& m) {}
  virtual void onMouseUp(const ViewpointWindow& w, const Mouse& m) {}
  virtual void onMouseDrag(const ViewpointWindow& w, const Mouse& m) {}
};

int main(int argc, char* argv[]) {
  AlloApp* app = new AlloApp;
  app->maker.start();
  app->start();
}
