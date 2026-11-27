#include "app/EditorApp.hpp"

#include <cstdlib>
#include <string>

int main(int argc, char **argv) {
    editor::EditorApp::Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--frame" && i + 1 < argc) {
            options.frame = argv[++i];
        } else if (argument == "--project" && i + 1 < argc) {
            options.project = argv[++i];
        } else if (argument == "--level" && i + 1 < argc) {
            options.level = argv[++i];
        } else if (argument == "--tab" && i + 1 < argc) {
            options.tab = std::atoi(argv[++i]);
        } else if (argument == "--demo") {
            options.demo = true;
        } else if (argument == "--play") {
            options.play = true;
        }
    }
    editor::EditorApp app;
    return app.run(options);
}
