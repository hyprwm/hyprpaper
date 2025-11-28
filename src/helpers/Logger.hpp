#pragma once

#include <hyprutils/cli/Logger.hpp>
#include "Memory.hpp"

#define LOG_DEBUG Hyprutils::CLI::LOG_DEBUG
#define LOG_ERR   Hyprutils::CLI::LOG_ERR
#define LOG_WARN  Hyprutils::CLI::LOG_WARN
#define LOG_TRACE Hyprutils::CLI::LOG_TRACE
#define LOG_CRIT  Hyprutils::CLI::LOG_CRIT

inline UP<Hyprutils::CLI::CLogger> g_logger = makeUnique<Hyprutils::CLI::CLogger>();
