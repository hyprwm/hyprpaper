#pragma once
#include <random>

class CRandomGenerator {
public:
    static CRandomGenerator& get() {
        static CRandomGenerator instance;
        return instance;
    }

    // Generate random index for vector size
    size_t getRandomIndex(size_t size) {
        if (size == 0) return 0;
        std::uniform_int_distribution<size_t> dis(0, size - 1);
        return dis(m_Generator);
    }

    // Get raw generator if needed
    std::mt19937& getGenerator() {
        return m_Generator;
    }

private:
    CRandomGenerator() : m_Generator(m_RandomDevice()) {}
    
    std::random_device m_RandomDevice;
    std::mt19937 m_Generator;

    // Delete copy/move to ensure singleton
    CRandomGenerator(const CRandomGenerator&) = delete;
    CRandomGenerator& operator=(const CRandomGenerator&) = delete;
    CRandomGenerator(CRandomGenerator&&) = delete;
    CRandomGenerator& operator=(CRandomGenerator&&) = delete;
}; 