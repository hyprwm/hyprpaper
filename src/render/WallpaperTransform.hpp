#pragma once
#include <cairo/cairo.h>
#include <hyprutils/math/Vector2D.hpp>

// Applies robust rotation, scaling, and centering for wallpaper rendering
// Always fills the monitor (cover mode), centers image, and handles all rotations correctly
inline void applyWallpaperTransform(cairo_t* cr, const Vector2D& imgSize, const Vector2D& monSize, int rotation) {
    double imgW = imgSize.x;
    double imgH = imgSize.y;
    double monW = monSize.x;
    double monH = monSize.y;
    
    // Normalize rotation
    int rot = rotation % 360;
    if (rot < 0) rot += 360;
    
    // For 90/270 degrees, the image dimensions are effectively swapped
    bool isRotated90or270 = (rot == 90 || rot == 270);
    double effectiveImgW = isRotated90or270 ? imgH : imgW;
    double effectiveImgH = isRotated90or270 ? imgW : imgH;
    
    // Calculate scale to cover the monitor (larger scale wins)
    double scaleX = monW / effectiveImgW;
    double scaleY = monH / effectiveImgH;
    double scale = std::max(scaleX, scaleY);
    
    // Debug output
    printf("DEBUG Transform: rotation=%d, imgSize=[%.1f,%.1f], monSize=[%.1f,%.1f]\n", 
           rot, imgW, imgH, monW, monH);
    printf("DEBUG effective=[%.1f,%.1f], scales=[%.3f,%.3f], final_scale=%.3f\n", 
           effectiveImgW, effectiveImgH, scaleX, scaleY, scale);
    printf("DEBUG transforms: translate=[%.1f,%.1f], rotate=%.1f°, scale=%.3f, img_center=[%.1f,%.1f]\n",
           monW/2.0, monH/2.0, (double)rot, scale, imgW/2.0, imgH/2.0);
    
    // Move to center of monitor
    cairo_translate(cr, monW / 2.0, monH / 2.0);
    
    // Rotate around center
    cairo_rotate(cr, rot * M_PI / 180.0);
    
    // Scale the image
    cairo_scale(cr, scale, scale);
    
    // Move to center of image (so image center aligns with rotation center)
    cairo_translate(cr, -imgW / 2.0, -imgH / 2.0);
}
