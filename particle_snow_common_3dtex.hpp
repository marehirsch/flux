// MAT201B: Final Project
// Original authors: Karl Yerkes and Matt Wright (2012)
//
// Author: Mark Hirsch (2017)
//

#ifndef __COMMON_STUFF__
#define __COMMON_STUFF__

#define LOCATIONS (31561) // rows
#define SAMPLES (1) // columns
#define CHUNKSIZE (9000) // cuttlebone thing

// define stuff that's common to both the simulator and renderer: definition of
// State and M.
//
struct State {
  int frame;
  Pose pose;
  float animTime;
};



#endif
