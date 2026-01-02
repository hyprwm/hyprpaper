#include "ConfigManager.hpp"
#include <algorithm>
#include <filesystem>
#include <glob.h>
#include <hyprlang.hpp>
#include <hyprutils/path/Path.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <string>
#include "../helpers/Logger.hpp"
#include "WallpaperMatcher.hpp"

#include <magic.h>

using namespace std::string_literals;

[[nodiscard]] static bool isImage(const std::filesystem::path& path) {
    static constexpr std::array exts{".jpg", ".jpeg", ".png", ".bmp", ".webp", ".svg"};

    auto                        ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (std::ranges::any_of(exts, [&ext](const auto& e) { return ext == e; }))
        return true;

    magic_t magic = magic_open(MAGIC_MIME_TYPE);
    if (magic == nullptr)
        return false;

    Hyprutils::Utils::CScopeGuard guard{[&magic] { magic_close(magic); }};

    if (magic_load(magic, nullptr) != 0)
        return false;

    const auto* result = magic_file(magic, path.string().c_str());
    if (result == nullptr)
        return false;

    return std::string(result).starts_with("image/");
}

// Forward declaration for the source handler
static Hyprlang::CParseResult handleSource(const char* COMMAND, const char* VALUE);

static std::string            getMainConfigPath() {
    static const auto paths = Hyprutils::Path::findConfig("hyprpaper");

    return paths.first.value_or("");
}

CConfigManager::CConfigManager(const std::string& configPath) :
    m_config(configPath.empty() ? getMainConfigPath().c_str() : configPath.c_str(), Hyprlang::SConfigOptions{.throwAllErrors = true, .allowMissingConfig = true}) {
    m_currentConfigPath = configPath.empty() ? getMainConfigPath() : configPath;
}

void CConfigManager::init() {
    m_config.addConfigValue("splash", Hyprlang::INT{1});
    m_config.addConfigValue("splash_offset", Hyprlang::INT{20});
    m_config.addConfigValue("splash_opacity", Hyprlang::FLOAT{0.8});
    m_config.addConfigValue("ipc", Hyprlang::INT{1});

    m_config.addSpecialCategory("wallpaper", Hyprlang::SSpecialCategoryOptions{.key = "monitor"});
    m_config.addSpecialConfigValue("wallpaper", "monitor", Hyprlang::STRING{""});
    m_config.addSpecialConfigValue("wallpaper", "path", Hyprlang::STRING{""});
    m_config.addSpecialConfigValue("wallpaper", "fit_mode", Hyprlang::STRING{"cover"});
    m_config.addSpecialConfigValue("wallpaper", "timeout", Hyprlang::INT{0});

    m_config.registerHandler(&handleSource, "source", Hyprlang::SHandlerOptions{});

    m_config.commence();

    auto result = m_config.parse();

    if (result.error)
        g_logger->log(LOG_ERR, "Config has errors:\n{}\nProceeding ignoring faulty entries", result.getError());

    g_matcher->addStates(getSettings());
}

Hyprlang::CConfig* CConfigManager::hyprlang() {
    return &m_config;
}

const std::string& CConfigManager::getCurrentConfigPath() const {
    return m_currentConfigPath;
}

static std::expected<std::string, std::string> resolvePath(const std::string_view& sv) {
    std::error_code ec;
    const auto      CAN = std::filesystem::canonical(sv, ec);

    if (ec)
        return std::unexpected(std::format("invalid path: {}", ec.message()));

    return CAN;
}

static std::expected<std::string, std::string> getPath(const std::string_view& sv, const std::string& basePath = "") {
    if (sv.empty())
        return std::unexpected("empty path");

    std::string path{sv};

    if (sv[0] == '~') {
        static auto HOME = getenv("HOME");
        if (!HOME || HOME[0] == '\0')
            return std::unexpected("home path but no $HOME");

        path = std::string{HOME} + "/"s + std::string{sv.substr(1)};
    } else if (!std::filesystem::path(sv).is_absolute() && !basePath.empty()) {
        // Make relative paths relative to the base path's directory
        auto baseDir = std::filesystem::path(basePath).parent_path();
        path         = (baseDir / sv).string();
    }

    return resolvePath(path);
}

static std::expected<std::vector<std::string>, std::string> getFullPath(const std::string& sv) {
    if (sv.empty())
        return std::unexpected("empty path");

    static constexpr const size_t maxImagesCount{1024};

    std::vector<std::string>      result;

    const auto                    resolved = getPath(sv);
    if (!resolved)
        return std::unexpected(resolved.error());

    const auto resolvedPath = resolved.value();
    if (!std::filesystem::exists(resolvedPath))
        return std::unexpected(std::format("File '{}' does not exist", resolvedPath));

    if (std::filesystem::is_directory(resolvedPath))
        for (const auto& entry : std::filesystem::directory_iterator(resolvedPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && isImage(entry.path()))
                result.push_back(entry.path());

            if (result.size() >= maxImagesCount)
                break;
        }
    else if (isImage(resolvedPath))
        result.push_back(resolvedPath);
    else
        return std::unexpected(std::format("File '{}' is neither an image nor a directory", resolvedPath));

    return result;
}

std::vector<CConfigManager::SSetting> CConfigManager::getSettings() {
    std::vector<CConfigManager::SSetting> result;

    auto                                  keys = m_config.listKeysForSpecialCategory("wallpaper");
    result.reserve(keys.size());

    for (auto& key : keys) {
        std::string monitor, fitMode, path;
        int         timeout;

        try {
            monitor = std::any_cast<Hyprlang::STRING>(m_config.getSpecialConfigValue("wallpaper", "monitor", key.c_str()));
            fitMode = std::any_cast<Hyprlang::STRING>(m_config.getSpecialConfigValue("wallpaper", "fit_mode", key.c_str()));
            path    = std::any_cast<Hyprlang::STRING>(m_config.getSpecialConfigValue("wallpaper", "path", key.c_str()));
            timeout = std::any_cast<Hyprlang::INT>(m_config.getSpecialConfigValue("wallpaper", "timeout", key.c_str()));
        } catch (...) {
            g_logger->log(LOG_ERR, "Failed parsing wallpaper for key {}", key);
            continue;
        }

        const auto RESOLVE_PATH = getFullPath(path);

        if (!RESOLVE_PATH) {
            g_logger->log(LOG_ERR, "Failed to resolve path {}: {}", path, RESOLVE_PATH.error());
            continue;
        }

        if (RESOLVE_PATH.value().empty()) {
            g_logger->log(LOG_ERR, "Provided path(s) '{}' does not contain a valid image", path);
            continue;
        }

        result.emplace_back(SSetting{.monitor = std::move(monitor), .fitMode = std::move(fitMode), .paths = RESOLVE_PATH.value(), .timeout = timeout});
    }

    return result;
}

static Hyprlang::CParseResult handleSource(const char* COMMAND, const char* VALUE) {
    Hyprlang::CParseResult result;

    const auto             value = Hyprutils::String::trim(VALUE);

    if (value.empty()) {
        result.setError("source= requires a file path");
        return result;
    }

    const auto RESOLVED_PATH = getPath(value, g_config->getCurrentConfigPath());

    if (!RESOLVED_PATH) {
        result.setError(std::format("source= path error: {}", RESOLVED_PATH.error()).c_str());
        return result;
    }

    const auto& PATH = RESOLVED_PATH.value();

    g_logger->log(LOG_DEBUG, "source: including '{}'", PATH);

    // Support glob patterns
    glob_t                        globResult;
    Hyprutils::Utils::CScopeGuard scopeGuard([&globResult]() { globfree(&globResult); });

    int                           globStatus = glob(PATH.c_str(), GLOB_TILDE | GLOB_NOSORT, nullptr, &globResult);

    if (globStatus == GLOB_NOMATCH) {
        // No glob match - try as a literal path
        std::error_code ec;
        const auto      exists = std::filesystem::exists(PATH, ec);
        if (ec || !exists) {
            result.setError(std::format("source file '{}' not found", PATH).c_str());
            return result;
        }

        // Parse the single file
        g_logger->log(LOG_DEBUG, "source: parsing file '{}'", PATH);
        auto parseResult = g_config->hyprlang()->parseFile(PATH.c_str());
        if (parseResult.error)
            result.setError(std::format("error parsing '{}': {}", PATH, parseResult.getError()).c_str());
        return result;
    }

    if (globStatus != 0) {
        result.setError(std::format("glob error for pattern '{}'", PATH).c_str());
        return result;
    }

    // Process all matched files
    g_logger->log(LOG_DEBUG, "source: glob matched {} file(s)", globResult.gl_pathc);
    for (size_t i = 0; i < globResult.gl_pathc; i++) {
        const std::string matchedPath = globResult.gl_pathv[i];

        std::error_code   ec;
        const auto        isFile = std::filesystem::is_regular_file(matchedPath, ec);
        if (ec || !isFile) {
            g_logger->log(LOG_WARN, "source: skipping non-regular file '{}'", matchedPath);
            continue;
        }

        g_logger->log(LOG_DEBUG, "source: parsing file '{}'", matchedPath);
        auto parseResult = g_config->hyprlang()->parseFile(matchedPath.c_str());
        if (parseResult.error)
            g_logger->log(LOG_ERR, "error parsing '{}': {}", matchedPath, parseResult.getError());
    }

    return result;
}
