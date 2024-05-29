#include "Bmp.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class BmpHeader {
public:
    unsigned char format[2];
    uint32_t sizeOfFile;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t dataOffset;
    uint32_t sizeOfBitmapHeader;
    uint32_t width;
    uint32_t height;
    uint16_t numberOfColors;
    uint16_t numberOfBitPerPixel;
    uint32_t compressionMethod;
    uint32_t imageSize;
    uint32_t horizontalResolutionPPM;
    uint32_t verticalResolutionPPM;
    uint32_t numberOfCollors;
    uint32_t numberOfImportantCollors;

    BmpHeader(std::ifstream& file) {
        file.read(reinterpret_cast<char*>(&format), sizeof(format));
        if (!(format[0] == 66 and format[1] == 77)) {
            Debug::log(ERR, "Unable to parse bitmap header: wrong bmp file type");
            exit(1);
        }
        file.read(reinterpret_cast<char*>(&sizeOfFile), sizeof(sizeOfFile));
        file.read(reinterpret_cast<char*>(&reserved1), sizeof(reserved1));
        file.read(reinterpret_cast<char*>(&reserved2), sizeof(reserved2));
        file.read(reinterpret_cast<char*>(&dataOffset), sizeof(dataOffset));
        file.read(reinterpret_cast<char*>(&sizeOfBitmapHeader), sizeof(sizeOfBitmapHeader));
        file.read(reinterpret_cast<char*>(&width), sizeof(width));
        file.read(reinterpret_cast<char*>(&height), sizeof(height));
        file.read(reinterpret_cast<char*>(&numberOfColors), sizeof(numberOfColors));
        file.read(reinterpret_cast<char*>(&numberOfBitPerPixel), sizeof(numberOfBitPerPixel));
        file.read(reinterpret_cast<char*>(&compressionMethod), sizeof(compressionMethod));
        file.read(reinterpret_cast<char*>(&imageSize), sizeof(imageSize));
        file.read(reinterpret_cast<char*>(&horizontalResolutionPPM), sizeof(horizontalResolutionPPM));
        file.read(reinterpret_cast<char*>(&verticalResolutionPPM), sizeof(verticalResolutionPPM));
        file.read(reinterpret_cast<char*>(&numberOfCollors), sizeof(numberOfCollors));
        file.read(reinterpret_cast<char*>(&numberOfImportantCollors), sizeof(numberOfImportantCollors));
        if (imageSize == 0) {
            imageSize = sizeOfFile - dataOffset;
        }
        file.seekg(dataOffset);
    };
};

void reflectImage(unsigned char* image, uint32_t numberOfRows, int stride) {
    int rowStart = 0;
    int rowEnd = numberOfRows - 1;
    unsigned char* temp = (unsigned char*) malloc(stride);
    while (rowStart < rowEnd) {
        memcpy(temp, &image[rowStart * stride], stride);
        memcpy(&image[rowStart * stride], &image[rowEnd * stride], stride);
        memcpy(&image[rowEnd * stride], temp, stride);
        rowStart++;
        rowEnd--;
    }
    free(temp);
};

void convertRgbToArgb(std::ifstream& imageStream, unsigned char* outputImage, uint32_t newImageSize) {
    uint8_t forthBitCounter = 0;
    unsigned long imgCursor = 0;
    while (imgCursor < newImageSize) {
        imageStream.read(reinterpret_cast<char*>(&outputImage[imgCursor]), 1);
        imgCursor++;
        forthBitCounter++;
        if (forthBitCounter == 3) {
            outputImage[imgCursor] = 0;
            imgCursor++;
            forthBitCounter = 0;
        }
    }
};

cairo_surface_t* BMP::createSurfaceFromBMP(const std::string& path) {

    if (!std::filesystem::exists(path)) {
        Debug::log(ERR, "createSurfaceFromBMP: file doesn't exist??");
        exit(1);
    }

    std::ifstream bitmapImageStream(path);
    BmpHeader bitmapHeader(bitmapImageStream);
    unsigned char* imageData;

    cairo_format_t format = CAIRO_FORMAT_ARGB32;
    int stride = cairo_format_stride_for_width (format, bitmapHeader.width);
    imageData = (unsigned char*) malloc(bitmapHeader.height * stride);

    if (bitmapHeader.numberOfBitPerPixel == 24) {
        convertRgbToArgb(bitmapImageStream, imageData, bitmapHeader.height * stride);
    } else if (bitmapHeader.numberOfBitPerPixel == 32) {
        bitmapImageStream.read(reinterpret_cast<char*>(&imageData), bitmapHeader.imageSize);
    } else {
        Debug::log(ERR, "createSurfaceFromBMP: unsupported bmp format");
        bitmapImageStream.close();
        exit(1);
    }
    bitmapImageStream.close();
    reflectImage(imageData, bitmapHeader.height, stride);
    return cairo_image_surface_create_for_data (imageData, format, bitmapHeader.width, bitmapHeader.height, stride);
}