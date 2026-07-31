#include "miniaudio_backend.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

const char* audio_backend_version() {
    return ma_version_string();
}
