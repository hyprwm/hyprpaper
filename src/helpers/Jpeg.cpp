#include "Jpeg.hpp"

#include <filesystem>
#include <fstream>

cairo_surface_t* JPEG::createSurfaceFromJPEG(const std::string& path) {

    if (!std::filesystem::exists(path)) {
        Debug::log(ERR, "createSurfaceFromJPEG: file doesn't exist??");
        exit(1);
    }

    if (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__) {
        Debug::log(CRIT, "tried to load a jpeg on a big endian system! ping vaxry he is lazy.");
        exit(1);
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    std::vector<uint8_t> bytes(file.tellg());
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());

    // now the JPEG is in the memory

    jpeg_decompress_struct decompressStruct = {};
    jpeg_error_mgr         errorManager     = {};

    decompressStruct.err = jpeg_std_error(&errorManager);
    jpeg_create_decompress(&decompressStruct);
    jpeg_mem_src(&decompressStruct, bytes.data(), bytes.size());
    jpeg_read_header(&decompressStruct, true);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    decompressStruct.out_color_space = JCS_EXT_BGRA;
#else
    decompressStruct.out_color_space = JCS_EXT_ARGB;
#endif

    // decompress
    jpeg_start_decompress(&decompressStruct);

    auto cairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, decompressStruct.output_width, decompressStruct.output_height);

    if (cairo_surface_status(cairoSurface) != CAIRO_STATUS_SUCCESS) {
        Debug::log(ERR, "createSurfaceFromJPEG: Cairo Failed (?)");
        exit(1);
    }

    const auto CAIRODATA   = cairo_image_surface_get_data(cairoSurface);
    const auto CAIROSTRIDE = cairo_image_surface_get_stride(cairoSurface);
    JSAMPROW   rowRead;

    while (decompressStruct.output_scanline < decompressStruct.output_height) {
        const auto PROW = CAIRODATA + (decompressStruct.output_scanline * CAIROSTRIDE);
        rowRead         = PROW;
        jpeg_read_scanlines(&decompressStruct, &rowRead, 1);
    }

    cairo_surface_mark_dirty(cairoSurface);
    cairo_surface_set_mime_data(cairoSurface, CAIRO_MIME_TYPE_JPEG, bytes.data(), bytes.size(), nullptr, nullptr);
    jpeg_finish_decompress(&decompressStruct);
    jpeg_destroy_decompress(&decompressStruct);

    return cairoSurface;
}
