#include "MonitorLayout.hpp"
#include "Logger.hpp"
#include "../ipc/HyprlandSocket.hpp"

#include <memory>
#include <json/json.h>

std::optional<std::vector<MonitorLayout::SMonitor>> MonitorLayout::query() {
    const auto REPLY = HyprlandSocket::getFromSocket("j/monitors");
    if (!REPLY) {
        g_logger->log(LOG_WARN, "span: couldn't query monitor layout: {}", REPLY.error());
        return std::nullopt;
    }

    const std::string&                DATA = REPLY.value();
    Json::CharReaderBuilder           builder;
    Json::Value                       root;
    std::string                       errs;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    if (!reader->parse(DATA.c_str(), DATA.c_str() + DATA.size(), &root, &errs)) {
        g_logger->log(LOG_ERR, "span: failed to parse monitor JSON: {}", errs);
        return std::nullopt;
    }

    if (!root.isArray()) {
        g_logger->log(LOG_ERR, "span: unexpected monitor JSON (not an array)");
        return std::nullopt;
    }

    std::vector<SMonitor> result;
    result.reserve(root.size());

    for (const auto& m : root) {
        if (m.get("disabled", false).asBool())
            continue;

        SMonitor mon;
        mon.name      = m.get("name", "").asString();
        mon.x         = m.get("x", 0).asDouble();
        mon.y         = m.get("y", 0).asDouble();
        mon.scale     = m.get("scale", 1.0).asDouble();
        mon.transform = m.get("transform", 0).asInt();

        if (mon.scale <= 0)
            mon.scale = 1.0;

        const double W = m.get("width", 0).asDouble();
        const double H = m.get("height", 0).asDouble();

        // wl_output transforms 90/270 (and their flipped variants) swap the logical footprint.
        const bool ROTATED = (mon.transform % 2) != 0;
        mon.w              = (ROTATED ? H : W) / mon.scale;
        mon.h              = (ROTATED ? W : H) / mon.scale;

        if (mon.name.empty() || mon.w <= 0 || mon.h <= 0)
            continue;

        result.emplace_back(std::move(mon));
    }

    return result;
}
