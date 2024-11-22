#include "JpegXL.hpp"

#include <filesystem>
#include <fstream>
#include <jxl/decode_cxx.h>
#include <jxl/resizable_parallel_runner_cxx.h>

cairo_surface_t* JXL::createSurfaceFromJXL(const std::string& path) {

    if (!std::filesystem::exists(path)) {
        Debug::log(ERR, "createSurfaceFromJXL: file doesn't exist??");
        exit(1);
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit | std::ifstream::eofbit);
    std::vector<uint8_t> bytes(file.tellg());
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());

    JxlSignature signature = JxlSignatureCheck(bytes.data(), bytes.size());
    if (signature != JXL_SIG_CODESTREAM && signature != JXL_SIG_CONTAINER) {
        Debug::log(ERR, "createSurfaceFromJXL: file is not JXL format");
        exit(1);
    }

    auto dec    = JxlDecoderMake(nullptr);
    auto runner = JxlResizableParallelRunnerMake(nullptr);
    if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec.get(), JxlResizableParallelRunner, runner.get())) {
        Debug::log(ERR, "createSurfaceFromJXL: JxlDecoderSetParallelRunner failed");
        exit(1);
    }

    if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE)) {
        Debug::log(ERR, "createSurfaceFromJXL: JxlDecoderSubscribeEvents failed");
        exit(1);
    }

    JxlDecoderSetInput(dec.get(), bytes.data(), bytes.size());
    JxlDecoderCloseInput(dec.get());
    if (JXL_DEC_BASIC_INFO != JxlDecoderProcessInput(dec.get())) {
        Debug::log(ERR, "createSurfaceFromJXL: JxlDecoderProcessInput failed");
        exit(1);
    }

    JxlBasicInfo basic_info;
    if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &basic_info)) {
        Debug::log(ERR, "createSurfaceFromJXL: JxlDecoderGetBasicInfo failed");
        exit(1);
    }

    auto cairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, basic_info.xsize, basic_info.ysize);
    if (cairo_surface_status(cairoSurface) != CAIRO_STATUS_SUCCESS) {
        Debug::log(ERR, "createSurfaceFromJXL: Cairo Failed (?)");
        cairo_surface_destroy(cairoSurface);
        exit(1);
    }

    const auto     CAIRODATA = cairo_image_surface_get_data(cairoSurface);

    JxlPixelFormat format = {
        .num_channels = 4,
        .data_type    = JXL_TYPE_UINT8,
        .endianness   = JXL_BIG_ENDIAN,
        .align        = cairo_image_surface_get_stride(cairoSurface),
    };

    const auto output_size = basic_info.xsize * basic_info.ysize * format.num_channels;

    for (;;) {
        JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
        if (status == JXL_DEC_ERROR) {
            Debug::log(ERR, "createSurfaceFromJXL: JxlDecoderProcessInput failed");
            cairo_surface_destroy(cairoSurface);
            exit(1);
        } else if (status == JXL_DEC_NEED_MORE_INPUT) {
            Debug::log(ERR, "createSurfaceFromJXL: JxlDecoderProcessInput expected more input");
            cairo_surface_destroy(cairoSurface);
            exit(1);
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            JxlResizableParallelRunnerSetThreads(runner.get(), JxlResizableParallelRunnerSuggestThreads(basic_info.xsize, basic_info.ysize));
            size_t buffer_size;
            if (JXL_DEC_SUCCESS != JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size)) {
                Debug::log(ERR, "createSurfaceFromJXL: JxlDecoderImageOutBufferSize failed");
                cairo_surface_destroy(cairoSurface);
                exit(1);
            }
            if (buffer_size != output_size) {
                Debug::log(ERR, "createSurfaceFromJXL: invalid output buffer size");
                cairo_surface_destroy(cairoSurface);
                exit(1);
            }
            if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(), &format, CAIRODATA, buffer_size)) {
                Debug::log(ERR, "createSurfaceFromJXL: JxlDecoderSetImageOutBuffer failed");
                cairo_surface_destroy(cairoSurface);
                exit(1);
            }
        } else if (status == JXL_DEC_FULL_IMAGE) {
            for (size_t i = 0; i < output_size - 2; i += format.num_channels) {
                std::swap(CAIRODATA[i + 0], CAIRODATA[i + 2]);
            }
            cairo_surface_mark_dirty(cairoSurface);
            cairo_surface_set_mime_data(cairoSurface, "image/jxl", bytes.data(), bytes.size(), nullptr, nullptr);
            return cairoSurface;
        }
    }
}
