#include <iostream>
#include "defines.hpp"
#include "Hyprpaper.hpp"

int main(int argc, char** argv, char** envp) {
    Debug::log(LOG, "Welcome to hyprpaper!\nbuilt from commit %s (%s)", GIT_COMMIT_HASH, GIT_COMMIT_MESSAGE);

    // starts
    g_pHyprpaper = std::make_unique<CHyprpaper>();
    g_pHyprpaper->init();

    return 0;
}