#pragma once

// Thin wrapper so callers outside src/audio never need to include
// miniaudio.h directly (it pulls in <windows.h>, which collides with
// raylib.h's symbols of the same name if both land in one translation unit).
const char* audio_backend_version();
