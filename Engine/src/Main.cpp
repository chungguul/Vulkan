#include "GameApp.hpp"
#include <iostream>
#include <exception>

int main() {
    try {
        GameApp app{};

        app.loadSceneFromJSON("../scenes/stage1.json");

        app.run();
    } catch (const std::exception &e) {
        std::cerr << "\n[치명적 에러 발생] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}