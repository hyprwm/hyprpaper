#include "ImagePicker.hpp"
#include <filesystem>
#include <vector>
#include <algorithm>
#include <random>
#include <hyprgraphics/image/Image.hpp>

std::string getRandomImageFromDirectory(const std::string& dirPath, const std::string& ignore) {
    std::vector<std::filesystem::path> images;
    
    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        if (!entry.is_regular_file())
            continue;
            
        if (!ignore.empty() && entry.path().string() == ignore)
            continue;
        
        // Use hyprgraphics to validate image instead of extension checking
        try {
            Hyprgraphics::CImage testImage(entry.path().string());
            if (testImage.success())
                images.push_back(entry.path());
        } catch (...) {
            continue;
        }
    }

    if (images.empty()) {
        if (!ignore.empty() && std::filesystem::exists(ignore))
            return ignore;
        return "";
    }

    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    std::shuffle(images.begin(), images.end(), gen);
    return images.front().string();
}