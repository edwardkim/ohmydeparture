# -*- coding: utf-8 -*-
"""Writes the R Departures icon into src/app.rdef.

A departures board standing upright on two legs, turned the way BeOS draws
things: its face runs down to the right, its thin top and right edge show.
On the face, a row of yellow flaps (the time), and below it an airline's
blue flap and white flaps, each split across the middle like a real flap.

The HVIF writer is the one the other R apps use (rwriteroom's
tools/make_icon.py, from rworldradio's).

Run from the project root:  python3 tools/make_icon.py
It rewrites the `resource vector_icon` block in src/app.rdef in place, and
writes the same bytes to assets/rdepartures.hvif.
"""

import io
import os
import re


# #pragma mark - HVIF

def _coord(v):
    b = int(round(v)) + 32
    if not 0 <= b <= 127:
        raise ValueError("coordinate %r outside the one-byte range" % v)
    return bytes([b])


def build_hvif(styles, paths, shapes):
    """styles: list of (r,g,b,a). paths: list of [(x,y), ...] polygons.
    shapes: list of (style_index, [path indices]) drawn in order."""
    out = bytearray(b"ncif")

    out.append(len(styles))
    for r, g, b, a in styles:
        out.append(0x01)                      # solid colour, RGBA
        out += bytes([r, g, b, a])

    out.append(len(paths))
    for points in paths:
        out.append(0x02 | 0x08)               # closed, no curves
        out.append(len(points))
        for x, y in points:
            out += _coord(x) + _coord(y)

    out.append(len(shapes))
    for style, path_indices in shapes:
        out.append(0x0A)                      # path source
        out.append(style)
        out.append(len(path_indices))
        for i in path_indices:
            out.append(i)
        out.append(0x00)                      # no transform, no flags

    return bytes(out)


def rdef(data):
    """The `vector_icon` alias rc accepts for resource(101, "BEOS:ICON")."""
    lines = ["resource vector_icon {"]
    hexstr = "".join("%02X" % b for b in data)
    for i in range(0, len(hexstr), 64):
        lines.append('\t$"%s"' % hexstr[i:i + 64])
    lines.append("};")
    return "\n".join(lines) + "\n"


# #pragma mark - the board

# The face: O is its top left corner, A runs along its width (down to the
# right, the BeOS angle) and H straight down its height. T is the board's
# thickness, going back up to the right, so the top and the right edge show.
O = (5.0, 12.0)
A = (44.0, 13.0)
H = (0.0, 31.0)
T = (8.0, -4.0)


def face(a, h):
    """(0,0) top left of the face, (1,1) bottom right."""
    return (O[0] + a * A[0] + h * H[0], O[1] + a * A[1] + h * H[1])


def quad(a0, h0, a1, h1):
    return [face(a0, h0), face(a1, h0), face(a1, h1), face(a0, h1)]


def plus(p, q, k=1.0):
    return (p[0] + q[0] * k, p[1] + q[1] * k)


BODY = (52, 54, 60, 255)
TOP = (96, 98, 106, 255)
SIDE = (34, 35, 40, 255)
FLAP = (24, 25, 28, 255)
SPLIT = (8, 8, 10, 255)
YELLOW = (255, 200, 40, 255)
WHITE = (236, 236, 228, 255)
BLUE = (30, 104, 214, 255)
LEG = (70, 72, 78, 255)


def build():
    styles = []
    colors = {}
    paths = []
    shapes = []

    def style_for(color):
        if color not in colors:
            colors[color] = len(styles)
            styles.append(color)
        return colors[color]

    def add(color, polys):
        indices = []
        for p in polys:
            indices.append(len(paths))
            paths.append(p)
        shapes.append((style_for(color), indices))

    # Legs first: the board stands in front of where they meet it.
    legs = []
    for a in (0.18, 0.82):
        top_left = face(a - 0.035, 0.9)
        top_right = face(a + 0.035, 0.9)
        legs.append([top_left, top_right, plus(top_right, (0, 9)),
                     plus(top_left, (0, 9))])
    add(LEG, legs)

    # The slab: top edge, right edge, face.
    add(TOP, [[face(0, 0), face(1, 0), plus(face(1, 0), T),
               plus(face(0, 0), T)]])
    add(SIDE, [[face(1, 0), plus(face(1, 0), T), plus(face(1, 1), T),
                face(1, 1)]])
    add(BODY, [quad(0, 0, 1, 1)])

    # Two rows of flaps.
    rows = [(0.12, 0.44), (0.56, 0.88)]
    first = [(0.07, 0.27), (0.29, 0.49), (0.51, 0.71), (0.73, 0.93)]
    second = [(0.07, 0.49), (0.51, 0.71), (0.73, 0.93)]
    flaps = []
    for (h0, h1), cells in zip(rows, (first, second)):
        for a0, a1 in cells:
            flaps.append(quad(a0, h0, a1, h1))
    add(FLAP, flaps)

    # What the flaps show: a glyph block on each yellow and white flap, the
    # blue airline flap filled.
    glyphs_yellow = []
    h0, h1 = rows[0]
    for a0, a1 in first:
        pad = (a1 - a0) * 0.28
        glyphs_yellow.append(quad(a0 + pad, h0 + 0.06, a1 - pad, h1 - 0.06))
    add(YELLOW, glyphs_yellow)

    h0, h1 = rows[1]
    add(BLUE, [quad(second[0][0], h0, second[0][1], h1)])
    glyphs_white = [quad(second[0][0] + 0.07, h0 + 0.11, second[0][1] - 0.07,
                         h1 - 0.11)]
    for a0, a1 in second[1:]:
        pad = (a1 - a0) * 0.28
        glyphs_white.append(quad(a0 + pad, h0 + 0.06, a1 - pad, h1 - 0.06))
    add(WHITE, glyphs_white)

    # The split across each row, over everything: what makes them flaps.
    splits = []
    for (h0, h1), cells in zip(rows, (first, second)):
        mid = (h0 + h1) / 2
        for a0, a1 in cells:
            splits.append(quad(a0, mid - 0.018, a1, mid + 0.018))
    add(SPLIT, splits)

    return build_hvif(styles, paths, shapes), len(paths)


BLOCK = re.compile(r"^resource vector_icon \{.*?^\};\n", re.S | re.M)


if __name__ == "__main__":
    data, count = build()
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    path = os.path.join(root, "src", "app.rdef")
    text = io.open(path, encoding="utf-8").read()
    if len(BLOCK.findall(text)) != 1:
        raise SystemExit("src/app.rdef: expected exactly one vector_icon block")
    io.open(path, "w", encoding="utf-8").write(
        BLOCK.sub(lambda m: rdef(data), text, count=1))
    assets = os.path.join(root, "assets")
    if not os.path.isdir(assets):
        os.makedirs(assets)
    io.open(os.path.join(assets, "rdepartures.hvif"), "wb").write(data)
    print("src/app.rdef: %d bytes, %d paths" % (len(data), count))
