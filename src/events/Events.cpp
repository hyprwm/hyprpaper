#include "Events.hpp"
#include "../Hyprpaper.hpp"

void Events::geometry(void *data, wl_output *output, int32_t x, int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel, const char *make, const char *model, int32_t transform) {
    // ignored
}

void Events::mode(void *data, wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    // ignored
}

void Events::done(void *data, wl_output *wl_output) {
    const auto PMONITOR = (SMonitor*)data;

    PMONITOR->readyForLS = true;

    g_pHyprpaper->recheckAllMonitors();
}

void Events::scale(void *data, wl_output *wl_output, int32_t scale) {
    const auto PMONITOR = (SMonitor*)data;

    PMONITOR->scale = scale;
}

void Events::name(void *data, wl_output *wl_output, const char *name) {
    const auto PMONITOR = (SMonitor*)data;

    PMONITOR->name = name;
}

void Events::description(void *data, wl_output *wl_output, const char *description) {
    // i do not care
}

void Events::ls_configure(void *data, zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
    const auto PMONITOR = (SMonitor *)data;

    PMONITOR->size = Vector2D(width, height);
    PMONITOR->wantsReload = true;
    PMONITOR->configureSerial = serial;
    PMONITOR->wantsACK = true;
    PMONITOR->initialized = true;

    Debug::log(LOG, "configure for %s", PMONITOR->name.c_str());
}

void Events::handleGlobal(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        g_pHyprpaper->m_sCompositor = (wl_compositor *)wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        g_pHyprpaper->m_sSHM = (wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        g_pHyprpaper->m_mtTickMutex.lock();

        const auto PMONITOR = g_pHyprpaper->m_vMonitors.emplace_back(std::make_unique<SMonitor>()).get();
        PMONITOR->wayland_name = name;
        PMONITOR->name = "";
        PMONITOR->output = (wl_output *)wl_registry_bind(registry, name, &wl_output_interface, 4);
        wl_output_add_listener(PMONITOR->output, &Events::outputListener, PMONITOR);

        g_pHyprpaper->m_mtTickMutex.unlock();
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        g_pHyprpaper->m_sLayerShell = (zwlr_layer_shell_v1 *)wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    }
}

void Events::handleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name) {
    // todo
}

