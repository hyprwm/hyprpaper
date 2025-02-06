#include "ImagePicker.hpp"
#include "RandomGenerator.hpp"
#include <filesystem>
#include <vector>
#include <algorithm>

std::string getRandomImageFromDirectory(const std::string &dirPath, const std::string &ignore) {
    std::vector<std::filesystem::path> images;
    for (const auto &entry : std::filesystem::directory_iterator(dirPath)) {
        if (!entry.is_regular_file())
            continue;
            
        auto ext = entry.path().extension().string();
        if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" &&
            ext != ".webp" && ext != ".jxl")
            continue;
            
        if (!ignore.empty() && entry.path().string() == ignore)
            continue;
            
        images.push_back(entry.path());
    }

    if (images.empty()) {
        // If no alternative was found but the ignored file exists, return it.
        if (!ignore.empty() && std::filesystem::exists(ignore))
            return ignore;
        return "";
    }

    std::shuffle(images.begin(), images.end(), CRandomGenerator::get().getGenerator());
    return images.front().string();
}
