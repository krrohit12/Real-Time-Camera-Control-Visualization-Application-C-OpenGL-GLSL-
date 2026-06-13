#include "Application.h"
#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    // Allow overriding shader directory from command line
    std::string shaderDir = "shaders";
    if (argc > 1) shaderDir = argv[1];

    // Look for shaders relative to the executable or in a common install location
    if (!std::filesystem::exists(shaderDir)) {
        // Try relative to project root (common when running from build/)
        std::string alt = "../shaders";
        if (std::filesystem::exists(alt))
            shaderDir = alt;
        else
            std::cerr << "Warning: shader dir '" << shaderDir
                      << "' not found — effects may not load\n";
    }

    Application app;
    if (!app.init(1280, 720, "Camera App", shaderDir)) {
        std::cerr << "Application init failed\n";
        return 1;
    }

    app.run();
    return 0;
}
