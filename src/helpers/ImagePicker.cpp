#include "ImagePicker.hpp"
#include <filesystem>
#include <vector>
#include <random>
#include <hyprgraphics/image/Image.hpp>

std::string getRandomImageFromDirectory(const std::string& dirPath, const std::string& ignore) {
    std::vector<std::filesystem::path> images;

    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        if (!entry.is_regular_file() || (!ignore.empty() && entry.path().string() == ignore))
            continue;

        if (Hyprgraphics::CImage::isImageFile(entry.path().string()))
            images.push_back(entry.path());
    }

    if (images.empty())
        return (!ignore.empty() && std::filesystem::exists(ignore)) ? ignore : "";

    static thread_local std::mt19937 gen{std::random_device{}()};
    return images[std::uniform_int_distribution<size_t>(0, images.size() - 1)(gen)].string();
}