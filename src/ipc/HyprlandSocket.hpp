#pragma once

#include <string_view>
#include <string>
#include <expected>

namespace HyprlandSocket {
    std::expected<std::string, std::string> getFromSocket(const std::string& cmd);
};