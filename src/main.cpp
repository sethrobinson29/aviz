#include "app/app.h"

#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <audio-file>\n", argv[0]);
        return 1;
    }
    return run_app(argv[1]);
}
