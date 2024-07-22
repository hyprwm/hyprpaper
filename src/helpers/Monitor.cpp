#include "Monitor.hpp"
#include "../Hyprpaper.hpp"

#include "protocols/wayland.hpp"

void SMonitor::registerListeners() {
    output->setMode([this](CCWlOutput* r, uint32_t flags, int32_t width, int32_t height, int32_t refresh) { size = Vector2D(width, height); });

    output->setDone([this](CCWlOutput* r) {
        readyForLS = true;
        if (g_pConfigManager) // don't tick if this is the first roundtrip
            g_pHyprpaper->tick(true);
    });

    output->setScale([this](CCWlOutput* r, int32_t scale_) { scale = scale_; });

    output->setName([this](CCWlOutput* r, const char* name_) { name = name_; });

    output->setDescription([this](CCWlOutput* r, const char* desc_) {
        std::string desc = desc_;
        std::erase(desc, ',');

        description = desc;
    });
}