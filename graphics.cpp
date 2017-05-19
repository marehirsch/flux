// MAT201B, final project renderer
// Winter 2016
// Author: Mark Hirsch
// code developed from 'shader_point_cloud' example by Karl Yerkes


#include "Gamma/Gamma.h"
#include "Gamma/SamplePlayer.h"
#include "allocore/io/al_App.hpp"
#include "Cuttlebone/Cuttlebone.hpp"
#include "alloutil/al_OmniStereoGraphicsRenderer.hpp"
#include "osgr.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>
#include <algorithm>


using namespace al;
using namespace std;

const int NUM_POINTS = 31561;
const int TEX_WIDTH = 178;    // round up sqrt(CHUNK)
const int TEX_SIZE = TEX_WIDTH*TEX_WIDTH*4;  // square texture rgba

#include "particle_snow_common_3dtex.hpp"  // XXX

string fullPathOrDie(string fileName, string whereToLook = ".") {
  SearchPaths searchPaths;
  searchPaths.addSearchPath(whereToLook);
  string filePath = searchPaths.find(fileName).filepath();
  assert(filePath != "");
  return filePath;
}


struct AlloApp : OmniStereoGraphicsRenderer1 {
//struct AlloApp : App {
  Light light;
  Mesh m;
  Mesh dataMesh;

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

  cuttlebone::Taker<State, CHUNKSIZE> taker;
  State* state;

  AlloApp() {
    state = new State;
    memset(state, 0, sizeof(State));
    cout << "State is " << sizeof(State) << " bytes or " << sizeof(State)/1000000.0 << " MB" << endl;
    initWindow();
    pose.pos(0, 0, 0);


    SearchPaths searchPaths;
    searchPaths.addAppPaths();
    searchPaths.addSearchPath("..");


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
            src0 = (90.0f + src0) / 180.0f;
            texArray.push_back(src0); // normalize before sending

            float src1(data[row][1]);
            src1 = (180.0f + src1) / 360.0f;
            texArray.push_back(src1);

            float srcColumn(data[row][column]);
            srcColumn = std::min(std::max(0.0f, srcColumn), 20.0f) / 20.0f;
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

}


  virtual void onAnimate(double dt) {
    int popCount = taker.get(*state);
    //int popCount = taker.get(state);
    pose = state->pose;

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

    animTime = state->animTime;
    // nav(state->pose);


    if (! popCount) return;
  }

  virtual void onDraw(Graphics& g) {
    omni().clearColor() = Color(0, 0, 0);
    // omni().clearColor() = Color(0.0f, omni().face() / 5.0f, (5.0f - omni().face()) / 5.0f);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, tex3d1);

    shader().uniform("animTime", animTime);
    shader().uniform("texSampler2", 0);

    g.blending(false);

    g.pointSize(10);
    g.draw(dataMesh);

    glDisable(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, 0);
  }
};

int main() {
  AlloApp* app = new AlloApp;
  app->taker.start();
  app->start();
  // AlloApp app;
  // app.taker.start(); // XXX
  // app.start();
}
