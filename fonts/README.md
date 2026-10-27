# Board fonts

The Linux board uses **D-DIN Condensed Bold** for Latin text and digits. CJK
characters keep the system CJK font, preferring Noto Sans CJK KR. On Linux,
Hangul uses 85% horizontal stretch while retaining its height, weight and
two-cell layout. The same measurement and rendering apply to Korean labels
and project badges. Japanese and Chinese retain their original width.
TeX Gyre Heros Cn Bold remains bundled as a
fallback. No system font settings are changed and builds do not download fonts.

## D-DIN

Unmodified upstream files from
[amcchord/datto-d-din](https://github.com/amcchord/datto-d-din/tree/e199c8441e758d6e492cd01ef52c3c67ba4bae26),
commit `e199c8441e758d6e492cd01ef52c3c67ba4bae26` (D-DIN 1.0).
Copyright 2017 Datto Inc.; design by Charles Nix (Monotype). Licensed under
SIL OFL 1.1 with reserved font names. `OFL-1.1.txt` and `FONTLOG.txt` are retained
verbatim with a `D-DIN-` filename prefix and included in installed licenses.

| Local file | SHA-256 |
| --- | --- |
| `D-DINCondensed-Bold.otf` | `872f4fde8b21d5105a83ba13988aa60224eae251b1708dec3062160b72d30736` |
| `D-DIN-OFL-1.1.txt` | `59e505e11e6da3ac3020eb6b4163f4d0382524183db660bf52a08234c764b2e5` |
| `D-DIN-FONTLOG.txt` | `e1e2e114db6aed0269e7f907bc0b2bd0da18c05b6339a8f712d3645d5d7e043f` |

## TeX Gyre

The original project bundled the unmodified `texgyreheroscn-bold.otf`. Its
[GUST Font License](GUST-FONT-LICENSE.txt) is retained and installed alongside the
D-DIN license.
