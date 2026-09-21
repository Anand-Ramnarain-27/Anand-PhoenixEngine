#!/usr/bin/env python3
"""Bake a TrueType font into a DirectXTK12 .spritefont (premultiplied RGBA8 atlas).

Drop-in replacement for the official MakeSpriteFont.exe for the common case, so the
UI system has no .NET / download dependency. Output is read by DirectX::SpriteFont.

    python tools/MakeSpriteFont.py <font.ttf> <out.spritefont> [--size 32] [--chars 32-126,161-255]

Binary layout (see SpriteFont.cpp, SpriteFont::Impl::Impl):
    "DXTKfont"
    u32 glyphCount, then glyphCount x { u32 char, i32 left, top, right, bottom, f32 xOffset, yOffset, xAdvance }
    f32 lineSpacing, u32 defaultChar
    u32 texWidth, u32 texHeight, u32 dxgiFormat, u32 stride, u32 rows, pixel data
Glyph layout rule in the runtime:  pen += xOffset; draw; pen += width + xAdvance.
"""
import argparse
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

DXGI_FORMAT_R8G8B8A8_UNORM = 28


def parse_ranges(spec):
    chars = []
    for part in spec.split(","):
        lo, _, hi = part.partition("-")
        chars.extend(range(int(lo), int(hi or lo) + 1))
    return sorted(set(chars))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("font")
    ap.add_argument("out")
    ap.add_argument("--size", type=int, default=32, help="pixel height of the baked font")
    ap.add_argument("--chars", default="32-126,161-255", help="codepoint ranges to bake")
    ap.add_argument("--default", type=int, default=ord("?"), help="codepoint drawn for missing glyphs")
    args = ap.parse_args()

    font = ImageFont.truetype(args.font, args.size)
    ascent, descent = font.getmetrics()
    line_spacing = float(ascent + descent)

    pad = 1
    glyphs = []
    for cp in parse_ranges(args.chars):
        ch = chr(cp)
        advance = font.getlength(ch)
        # bbox is relative to the baseline origin ('ls' anchor): left, top(<0 above baseline), right, bottom.
        l, t, r, b = font.getbbox(ch, anchor="ls")
        w, h = max(0, r - l), max(0, b - t)
        mask = None
        if w > 0 and h > 0:
            mask = Image.new("L", (w, h), 0)
            ImageDraw.Draw(mask).text((-l, -t), ch, font=font, fill=255, anchor="ls")
        glyphs.append({
            "cp": cp, "w": w, "h": h, "mask": mask,
            "xoff": float(l), "yoff": float(ascent + t),
            "xadv": float(advance - l - w),
        })

    if not any(g["cp"] == args.default for g in glyphs):
        sys.exit("default character 0x%X is not in the baked range" % args.default)

    # Shelf-pack into a power-of-two atlas, growing until everything fits.
    side = 128
    while True:
        x = y = row_h = 0
        ok = True
        for g in glyphs:
            if g["w"] == 0:
                g["x"] = g["y"] = 0
                continue
            if x + g["w"] + pad > side:
                x, y, row_h = 0, y + row_h + pad, 0
            if y + g["h"] + pad > side:
                ok = False
                break
            g["x"], g["y"] = x, y
            x += g["w"] + pad
            row_h = max(row_h, g["h"])
        if ok:
            break
        side *= 2

    atlas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    for g in glyphs:
        if g["mask"] is None:
            continue
        # White glyphs, premultiplied: rgb == alpha == coverage.
        px = Image.merge("RGBA", (g["mask"], g["mask"], g["mask"], g["mask"]))
        atlas.paste(px, (g["x"], g["y"]))

    with open(args.out, "wb") as f:
        f.write(b"DXTKfont")
        f.write(struct.pack("<I", len(glyphs)))
        for g in glyphs:
            f.write(struct.pack("<I4i3f", g["cp"], g["x"], g["y"], g["x"] + g["w"], g["y"] + g["h"],
                                g["xoff"], g["yoff"], g["xadv"]))
        f.write(struct.pack("<fI", line_spacing, args.default))
        f.write(struct.pack("<5I", side, side, DXGI_FORMAT_R8G8B8A8_UNORM, side * 4, side))
        f.write(atlas.tobytes())

    print("%s: %d glyphs, %dx%d atlas, line %.0fpx" % (args.out, len(glyphs), side, side, line_spacing))


if __name__ == "__main__":
    main()
