#pragma once
#include "Globals.h"
#include <string>

// One resolved 2D draw, in screen pixels. ModuleUI builds a list of these from the widget tree
// (already in back-to-front order) and UIPass turns them into SpriteBatch calls.
struct UIDrawItem {
    enum class Kind { Image, Text };

    Kind kind = Kind::Image;
    Vector2 position = Vector2::Zero;   // screen position of the pivot
    float rotation = 0.f;               // radians, clockwise
    Vector4 color = Vector4(1.f, 1.f, 1.f, 1.f);

    // Optional clip rectangle in screen pixels (x, y, w, h); nothing outside it is drawn.
    bool clip = false;
    Vector4 clipRect = Vector4::Zero;

    // Image: destination size, normalized pivot inside it, optional texel sub-rect. Empty texture = flat colour.
    std::string texture;
    Vector2 size = Vector2::Zero;
    Vector2 pivot = Vector2(0.5f, 0.5f);
    bool useSourceRect = false;
    Vector4 sourceRect = Vector4::Zero; // x, y, w, h in texels
    bool useSourceUV = false;
    Vector4 sourceUV = Vector4(0.f, 0.f, 1.f, 1.f); // x, y, w, h as 0..1 fractions of the texture

    // Text: `origin` is the pivot expressed in unscaled font pixels from the top-left of the text block.
    std::string text;
    std::string font;
    Vector2 origin = Vector2::Zero;
    float scale = 1.f;
};
