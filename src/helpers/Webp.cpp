#include "Webp.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

cairo_surface_t* WEBP::createSurfaceFromWEBP(const std::string& path) {

    if (!std::filesystem::exists(path)) {
        Debug::log(ERR, "createSurfaceFromWEBP: file doesn't exist??");
        exit(1);
    }

    void* imageRawData;

    struct stat fileInfo = {};

    const auto FD = open(path.c_str(), O_RDONLY);

    fstat(FD, &fileInfo);

    imageRawData = malloc(fileInfo.st_size);

    read(FD, imageRawData, fileInfo.st_size);

    close(FD);

    // now the WebP is in the memory

    WebPDecoderConfig config;
    if (!WebPInitDecoderConfig(&config)) {
        Debug::log(CRIT, "WebPInitDecoderConfig Failed");
        exit(1);
    }

    if (WebPGetFeatures((const unsigned char*)imageRawData, fileInfo.st_size, &config.input) != VP8_STATUS_OK) {
        Debug::log(ERR, "createSurfaceFromWEBP: file is not webp format");
        free(imageRawData);
        exit(1);
    }

    const auto height = config.input.height;
    const auto width = config.input.width;

    auto cairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(cairoSurface) != CAIRO_STATUS_SUCCESS) {
        Debug::log(CRIT, "createSurfaceFromWEBP: Cairo Failed (?)");
        cairo_surface_destroy(cairoSurface);
        exit(1);
    }


    if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
        config.output.colorspace = MODE_bgrA;
    } else {
        config.output.colorspace = MODE_Argb;
    }

    const auto CAIRODATA = cairo_image_surface_get_data(cairoSurface);
    const auto CAIROSTRIDE = cairo_image_surface_get_stride(cairoSurface);

    config.options.no_fancy_upsampling = 1;
    config.output.u.RGBA.rgba = CAIRODATA;
    config.output.u.RGBA.stride = CAIROSTRIDE;
    config.output.u.RGBA.size = CAIROSTRIDE * height;
    config.output.is_external_memory = 1;
    config.output.width = width;
    config.output.height = height;

    if (WebPDecode((const unsigned char*)imageRawData, fileInfo.st_size, &config) != VP8_STATUS_OK) {
        Debug::log(CRIT, "createSurfaceFromWEBP: WebP Decode Failed (?)");
        exit(1);
    }

    cairo_surface_mark_dirty(cairoSurface);
    cairo_surface_set_mime_data(cairoSurface, CAIRO_MIME_TYPE_PNG, (const unsigned char*)imageRawData, fileInfo.st_size, free, imageRawData);

	WebPFreeDecBuffer(&config.output);

    return cairoSurface;

}
