#include "System.hpp"
#include "../defines.hpp"
#include <array>

// Execute a shell command and get the output
std::string execAndGet(const char* cmd) {
    std::array<char, 128>   buffer;
    std::string             result;
    const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        Debug::log(ERR, "execAndGet: failed in pipe");
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
