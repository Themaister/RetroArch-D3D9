#ifndef COMMON_H__
#define COMMON_H__

#define UNICODE
#define DIRECTINPUT_VERSION 0x0800

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "keysym.h"
#include "ssnes_video.h"
#include "D3DVideo.h"
#include "DirectInput.h"

#ifdef __GNUC__
#if (__GNUC__ == 4) && (__GNUC_MINOR__ < 6)
#define nullptr 0
#elif (__GNUC__ < 4)
#error "Cannot build with such an old compiler. At least get GCC 4.5."
#endif
#endif

#endif

