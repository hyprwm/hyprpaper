#pragma once
#include <cairo/cairo.h>
#include <hyprutils/math/Vector2D.hpp>

enum class WallpaperMode {
    COVER,   // Fill monitor, may crop image (default)
    CONTAIN, // Fit entire image, may leave empty space
    TILE     // Use 1:1 scale with tiling
};

/**
 * Applies wallpaper transformation with rotation, scaling, and centering
 * 
 * @param cr Cairo context for rendering
 * @param imgSize Original image dimensions
 * @param monSize Monitor dimensions  
 * @param rotation Rotation angle in degrees (0, 90, 180, 270)
 * @param contain Use contain mode (fit entire image)
 * @param tile Use tile mode (1:1 scale)
 */
inline void applyWallpaperTransform(cairo_t* cr, const Vector2D& imgSize, const Vector2D& monSize, 
                                   int rotation, bool contain = false, bool tile = false) {
    const double imgW = imgSize.x;
    const double imgH = imgSize.y;
    const double monW = monSize.x;
    const double monH = monSize.y;
    
    // Normalize rotation to 0-359 range
    const int normalizedRotation = ((rotation % 360) + 360) % 360;
    
    // Calculate effective image dimensions after rotation
    const bool isDimensionSwapped = (normalizedRotation == 90 || normalizedRotation == 270);
    const double effectiveImgW = isDimensionSwapped ? imgH : imgW;
    const double effectiveImgH = isDimensionSwapped ? imgW : imgH;
    
    // Calculate scale factor based on wallpaper mode
    const double scaleX = monW / effectiveImgW;
    const double scaleY = monH / effectiveImgH;
    
    double scale;
    if (contain) {
        scale = std::min(scaleX, scaleY);  // Fit entire image
    } else if (tile) {
        scale = 1.0;                       // Original size
    } else {
        scale = std::max(scaleX, scaleY);  // Fill monitor (cover mode)
    }
    
    // Apply Cairo transformations
    cairo_translate(cr, monW / 2.0, monH / 2.0);                    // Move to monitor center
    cairo_rotate(cr, normalizedRotation * M_PI / 180.0);            // Rotate around center
    cairo_scale(cr, scale, scale);                                  // Scale image
    cairo_translate(cr, -imgW / 2.0, -imgH / 2.0);                 // Center image on rotation point
}
