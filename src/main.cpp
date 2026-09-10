#include "app/app.h"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace {
void print_usage(const char* program) {
    std::fprintf(stderr, "usage: %s <audio-file> [-n <count>]\n", program);
}
}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::optional<std::string> audio_path;
    std::optional<int> generate_count;

    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-n") {
            if (i + 1 >= args.size()) {
                print_usage(argv[0]);
                return 1;
            }
            ++i;
            char* end = nullptr;
            long value = std::strtol(args[i].c_str(), &end, 10);
            if (end == args[i].c_str() || *end != '\0' || value < 1) {
                std::fprintf(stderr, "error: -n requires a positive integer count\n");
                return 1;
            }
            generate_count = static_cast<int>(value);
        } else if (!audio_path.has_value()) {
            audio_path = args[i];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!audio_path.has_value()) {
        print_usage(argv[0]);
        return 1;
    }

    return run_app(audio_path->c_str(), generate_count);
}
