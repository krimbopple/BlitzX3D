#ifndef STD_H
#define STD_H

#include "..//fmod375/include/fmod.h"

#include "../config/config.h"
#include "../stdutil/stdutil.h"
#include "../bbruntime/constants.h"
#include "debug_log.h"

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
#ifndef DX9
#include <ddraw.h> // Why are we still here? Just to suffer?
#include <d3d.h>
#else
#include <d3d9.h> // suicide
#endif

#endif