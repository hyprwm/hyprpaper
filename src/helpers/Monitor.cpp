#include "Monitor.hpp"
#include "../Hyprpaper.hpp"

void SMonitor::registerListeners() {
    output->setMode([this](CCWlOutput* r, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
        size               = Vector2D(width, height);
        newModeForGeometry = true;
    });

    output->setDone([this](CCWlOutput* r) {
        readyForLS = true;
        std::lock_guard<std::mutex> lg(g_pHyprpaper->m_mtTickMutex);
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

    output->setGeometry([this](CCWlOutput* r, int32_t x, int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel, const char* make, const char* model,
                               int32_t transform_) { //
        transform = (wl_output_transform)transform_;

        //if no new mode has been sent before a geometry request, ignore it.
        if (!newModeForGeometry)
            return;
        newModeForGeometry = false;

        //see https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_output-enum-transform
        if ((transform % 4) == 1 || (transform % 4) == 3)
            std::swap(size.x, size.y);
    });
}