#include "LayerSurface.hpp"

#include "../Hyprpaper.hpp"

CLayerSurface::CLayerSurface(SMonitor* pMonitor) {
    m_pMonitor = pMonitor;

    pSurface = makeShared<CCWlSurface>(g_pHyprpaper->m_pCompositor->sendCreateSurface());

    if (!pSurface) {
        Debug::log(CRIT, "The compositor did not allow hyprpaper a surface!");
        exit(1);
    }

    const auto PINPUTREGION = makeShared<CCWlRegion>(g_pHyprpaper->m_pCompositor->sendCreateRegion());

    if (!PINPUTREGION) {
        Debug::log(CRIT, "The compositor did not allow hyprpaper a region!");
        exit(1);
    }

    pSurface->sendSetInputRegion(PINPUTREGION.get());

    pLayerSurface = makeShared<CCZwlrLayerSurfaceV1>(
        g_pHyprpaper->m_pLayerShell->sendGetLayerSurface(pSurface->resource(), pMonitor->output->resource(), ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "hyprpaper"));

    if (!pLayerSurface) {
        Debug::log(CRIT, "The compositor did not allow hyprpaper a layersurface!");
        exit(1);
    }

    pLayerSurface->sendSetSize(0, 0);
    pLayerSurface->sendSetAnchor((zwlrLayerSurfaceV1Anchor)(ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                                            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT));
    pLayerSurface->sendSetExclusiveZone(-1);

    pLayerSurface->setConfigure([this](CCZwlrLayerSurfaceV1* r, uint32_t serial, uint32_t x, uint32_t y) {
        m_pMonitor->size            = Vector2D((double)x, (double)y);
        m_pMonitor->wantsReload     = true;
        m_pMonitor->configureSerial = serial;
        m_pMonitor->wantsACK        = true;
        m_pMonitor->initialized     = true;

        Debug::log(LOG, "configure for %s", m_pMonitor->name.c_str());
    });

    pLayerSurface->setClosed([this](CCZwlrLayerSurfaceV1* r) {
        for (auto& m : g_pHyprpaper->m_vMonitors) {
            std::erase_if(m->layerSurfaces, [&](const auto& other) { return other.get() == this; });
            if (m->pCurrentLayerSurface == this) {
                if (m->layerSurfaces.empty()) {
                    m->pCurrentLayerSurface = nullptr;
                } else {
                    m->pCurrentLayerSurface = m->layerSurfaces.begin()->get();
                    g_pHyprpaper->recheckMonitor(m.get());
                }
            }
        }
    });

    pSurface->sendCommit();

    // fractional scale, if supported by the compositor
    if (g_pHyprpaper->m_pFractionalScale && g_pHyprpaper->m_pViewporter) {
        pFractionalScaleInfo = makeShared<CCWpFractionalScaleV1>(g_pHyprpaper->m_pFractionalScale->sendGetFractionalScale(pSurface->resource()));
        pFractionalScaleInfo->setPreferredScale([this](CCWpFractionalScaleV1* r, uint32_t sc120) {
            const double SCALE = sc120 / 120.0;

            Debug::log(LOG, "handlePreferredScale: %.2lf for %lx", SCALE, this);

            if (fScale != SCALE) {
                fScale = SCALE;
                std::lock_guard<std::mutex> lg(g_pHyprpaper->m_mtTickMutex);
                m_pMonitor->wantsReload = true;
                g_pHyprpaper->tick(true);
            }
        });

        pViewport = makeShared<CCWpViewport>(g_pHyprpaper->m_pViewporter->sendGetViewport(pSurface->resource()));

        pSurface->sendCommit();
    } else
        Debug::log(ERR, "No fractional-scale-v1 / wp-viewporter support from the compositor! fractional scaling will not work.");

    wl_display_flush(g_pHyprpaper->m_sDisplay);
}

CLayerSurface::~CLayerSurface() {
    // hyprwayland-scanner will send the destructors automatically. Neat.
    pLayerSurface.reset();
    pFractionalScaleInfo.reset();
    pViewport.reset();
    pSurface.reset();

    wl_display_flush(g_pHyprpaper->m_sDisplay);
}
