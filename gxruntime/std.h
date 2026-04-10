#ifndef STD_H
#define STD_H

// openAL
#include "../openAL/include/AL/al.h" // just let me do <AL/al.h> you wank
#include "../openAL/include/AL/alc.h"
#include "../openAL/include/dr_wav.h"
#include "../openAL/include/dr_mp3.h"

#include "../config/config.h"
#include "../stdutil/stdutil.h"
#include "../bbruntime/constants.h"

#pragma warning( disable:4786 )

#define DIRECTSOUND_VERSION 0x700
#define NOMINMAX // stupid microsoft

#include <set>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include <math.h>
#include <Windows.h>
#include <ddraw.h> // Why are we still here? Just to suffer?
#include <d3d.h>

#endif