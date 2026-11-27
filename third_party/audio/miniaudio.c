// stb_vorbis only needs its declarations here; the implementation lives in
// stb_vorbis.c, which is compiled as its own translation unit. miniaudio keys
// its built-in Ogg Vorbis backend off STB_VORBIS_INCLUDE_STB_VORBIS_H.
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
