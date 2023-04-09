#include "Events.hpp"
#include "../Hyprpaper.hpp"

void Events::geometry(void *data, wl_output *output, int32_t x, int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel, const char *make, const char *model, int32_t transform) {
    // ignored
}

void Events::mode(void *data, wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    const auto PMONITOR = (SMonitor*)data;

    PMONITOR->size = Vector2D(width, height);
}

void Events::done(void *data, wl_output *wl_output) {
    const auto PMONITOR = (SMonitor*)data;

    PMONITOR->readyForLS = true;

    g_pHyprpaper->tick(true);
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

void Events::handleCapabilities(void *data, wl_seat *wl_seat, uint32_t capabilities) {
    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        wl_pointer_add_listener(wl_seat_get_pointer(wl_seat), &pointerListener, wl_seat);
    }
}

void Events::handlePointerLeave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface) {
    // ignored
    wl_surface_commit(surface);

    g_pHyprpaper->m_pLastMonitor = nullptr;
}

void Events::handlePointerAxis(void *data, wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    // ignored
}

void Events::handlePointerMotion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    // ignored
    if (g_pHyprpaper->m_pLastMonitor) {
        wl_surface_commit(g_pHyprpaper->m_pLastMonitor->pCurrentLayerSurface->pSurface);
    }
}

void Events::handlePointerButton(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t button_state) {
    // ignored
}

void Events::handlePointerEnter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    for (auto& mon : g_pHyprpaper->m_vMonitors) {
        if (mon->pCurrentLayerSurface->pSurface == surface) {
            g_pHyprpaper->m_pLastMonitor = mon.get();

            wl_surface_set_buffer_scale(mon->pCurrentLayerSurface->pCursorSurface, mon->scale);
            wl_surface_attach(mon->pCurrentLayerSurface->pCursorSurface, wl_cursor_image_get_buffer(mon->pCurrentLayerSurface->pCursorImg), 0, 0);
            wl_pointer_set_cursor(wl_pointer, serial, mon->pCurrentLayerSurface->pCursorSurface, mon->pCurrentLayerSurface->pCursorImg->hotspot_x / mon->scale, mon->pCurrentLayerSurface->pCursorImg->hotspot_y / mon->scale);
            wl_surface_commit(mon->pCurrentLayerSurface->pCursorSurface);
        }
    }
}

void Events::ls_configure(void *data, zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
    const auto PLAYERSURFACE = (CLayerSurface*)data;

    PLAYERSURFACE->m_pMonitor->size = Vector2D(width, height);
    PLAYERSURFACE->m_pMonitor->wantsReload = true;
    PLAYERSURFACE->m_pMonitor->configureSerial = serial;
    PLAYERSURFACE->m_pMonitor->wantsACK = true;
    PLAYERSURFACE->m_pMonitor->initialized = true;

    Debug::log(LOG, "configure for %s", PLAYERSURFACE->m_pMonitor->name.c_str());
}

void Events::handleLSClosed(void *data, zwlr_layer_surface_v1 *zwlr_layer_surface_v1) {
    const auto PLAYERSURFACE = (CLayerSurface*)data;

    for (auto& m : g_pHyprpaper->m_vMonitors) {
        std::erase_if(m->layerSurfaces, [&](const auto& other) { return other.get() == PLAYERSURFACE; });
        if (m->pCurrentLayerSurface == PLAYERSURFACE) {
            if (m->layerSurfaces.empty()) {
                m->pCurrentLayerSurface = nullptr;
            } else {
                m->pCurrentLayerSurface = m->layerSurfaces.begin()->get();
                g_pHyprpaper->recheckMonitor(m.get());
            }
        }
    }
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
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        g_pHyprpaper->createSeat((wl_seat*)wl_registry_bind(registry, name, &wl_seat_interface, 1));
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        g_pHyprpaper->m_sLayerShell = (zwlr_layer_shell_v1*)wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0 && !g_pHyprpaper->m_bNoFractionalScale) {
        g_pHyprpaper->m_sFractionalScale = (wp_fractional_scale_manager_v1*)wl_registry_bind(registry, name, &wp_fractional_scale_manager_v1_interface, 1);
    } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
        g_pHyprpaper->m_sViewporter = (wp_viewporter*)wl_registry_bind(registry, name, &wp_viewporter_interface, 1);
    }
}

void Events::handleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name) {
    for (auto& m : g_pHyprpaper->m_vMonitors) {
        if (m->wayland_name == name) {
            Debug::log(LOG, "Destroying output %s", m->name.c_str());
            std::erase_if(g_pHyprpaper->m_vMonitors, [&](const auto& other) { return other->wayland_name == name; });
            return;
        }
    }
}

void Events::handlePreferredScale(void *data, wp_fractional_scale_v1* fractionalScaleInfo, uint32_t scale) {
    const double SCALE = scale / 120.0;

    CLayerSurface *const pLS = (CLayerSurface*)data;

    Debug::log(LOG, "handlePreferredScale: %.2lf for %lx", SCALE, pLS);

    if (pLS->fScale != SCALE) {
        pLS->fScale = SCALE;
        g_pHyprpaper->tick(true);
    }
}

