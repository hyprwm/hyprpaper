#pragma once
#include <string>

// Returns a random image path from the given directory.
// If 'ignore' is provided, that file (if present) will be excluded from the selection.
// Returns an empty string if no valid image is found.
std::string getRandomImageFromDirectory(const std::string &dirPath, const std::string &ignore = "");
