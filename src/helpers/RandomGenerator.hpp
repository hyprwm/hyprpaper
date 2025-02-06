#pragma once
#include <random>
#include <memory>

class CRandomGenerator {
public:
    CRandomGenerator() : mGenerator(mRandomDevice()) {}
    
    // Generate random index for vector size
    size_t getRandomIndex(size_t size) {
        if (size == 0) return 0;
        std::uniform_int_distribution<size_t> distribution(0, size - 1);
        return distribution(mGenerator);
    }

    // Get raw generator if needed
    std::mt19937& getGenerator() {
        return mGenerator;
    }

private:
    std::random_device mRandomDevice;
    std::mt19937 mGenerator;
};

// Global pointer to random generator
inline std::unique_ptr<CRandomGenerator> gRandomGenerator; 