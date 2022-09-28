#include <iostream>
#include "defines.hpp"
#include "Hyprpaper.hpp"

int main(int argc, char** argv, char** envp) {
    Debug::log(LOG, "Welcome to hyprpaper!\nbuilt from commit %s (%s)", GIT_COMMIT_HASH, GIT_COMMIT_MESSAGE);

    // parse some args
    std::string configPath;
    for (int i = 1; i < argc; ++i) {
        if ((!strcmp(argv[i], "-c") || !strcmp(argv[i], "--config")) && argc >= i + 2) {
            configPath = std::string(argv[++i]);
            Debug::log(LOG, "Using config location %s.", configPath.c_str());
        } else {
            std::cout << "Hyprpaper usage: hyprpaper [arg [...]].\n\nArguments:\n" <<
                "--help         -h | Show this help message\n" <<
                "--config       -c | Specify config file to use\n";
            return 1;
        }
    }

    // starts
    g_pHyprpaper = std::make_unique<CHyprpaper>();
    g_pHyprpaper->m_szExplicitConfigPath = configPath;
    g_pHyprpaper->init();

    return 0;
}