#include "LayerSurface.hpp"

#include "../Hyprpaper.hpp"

CLayerSurface::CLayerSurface(SMonitor* pMonitor) {
    m_pMonitor = pMonitor;

    pSurface = wl_compositor_create_surface(g_pHyprpaper->m_sCompositor);

    if (!pSurface) {
        Debug::log(CRIT, "The compositor did not allow hyprpaper a surface!");
        exit(1);
    }

    const auto PINPUTREGION = wl_compositor_create_region(g_pHyprpaper->m_sCompositor);

    if (!PINPUTREGION) {
        Debug::log(CRIT, "The compositor did not allow hyprpaper a region!");
        exit(1);
    }

    wl_surface_set_input_region(pSurface, PINPUTREGION);

    pLayerSurface = zwlr_layer_shell_v1_get_layer_surface(g_pHyprpaper->m_sLayerShell, pSurface, pMonitor->output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "hyprpaper");

    if (!pLayerSurface) {
        Debug::log(CRIT, "The compositor did not allow hyprpaper a layersurface!");
        exit(1);
    }

    zwlr_layer_surface_v1_set_size(pLayerSurface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(pLayerSurface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    zwlr_layer_surface_v1_set_exclusive_zone(pLayerSurface, -1);
    zwlr_layer_surface_v1_add_listener(pLayerSurface, &Events::layersurfaceListener, this);
    wl_surface_commit(pSurface);

    wl_region_destroy(PINPUTREGION);

    wl_display_flush(g_pHyprpaper->m_sDisplay);
}

CLayerSurface::~CLayerSurface() {
    zwlr_layer_surface_v1_destroy(pLayerSurface);
    wl_surface_destroy(pSurface);

    wl_display_flush(g_pHyprpaper->m_sDisplay);
}
