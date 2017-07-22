#pragma once

#include "../basics/Pixel3DSet.h"
#include "experiments.h"
#include "../basics/locv3.h"


#ifdef HASGL

//takes two peices frames
//prints out the translations between each experiment
/*
//use w,a,s & d to find the translation. - to decrease the velocity, + to increase it and f - to finish the curration

*/
void currate(ll_pix3d::Pix3D a, ll_pix3d::Pix3D b, std::string distance);

//currate-2 eg. currate("trans.5cminc.office1", 0, 1, "5cm");
void currate(std::string datasetname, int index1, int index2, std::string distance);

#endif