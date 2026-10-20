<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

The chant player uses [Noto Sans CJK SC Regular](https://github.com/notofonts/noto-cjk)
under the SIL Open Font License (local copy: `fonts/OFL.txt`).
`fonts/NotoSansCJKsc-Regular.otf` is the source; `fonts/chant_font_18.c` and
`fonts/chant_font_28.c` are uncompressed 4bpp LVGL subsets containing ASCII and
the body inventory in `fonts/chant-glyphs.txt` (18 px) and the title-only
inventory in `fonts/chant-title-glyphs.txt` (28 px). Generate with
`python3 tools/generate_chant_fonts.py` (Pillow/FreeType required). The generator
verifies bitmap pixel readback; the static gate checks actual application glyph coverage.
Large-font headings and home labels are collected from application source, not
from a separate manual title list. The scan-page Wi-Fi icon is drawn as arcs
and a dot on its own widget; it needs no image or additional icon font.

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

The chant QR display accepts an owner-supplied local image. Keep its original
and verified preview as `images/chant-qr-local*` (Git-ignored), with no public
redistribution license assumed. `tools/prepare_chant_qr.py` uses OpenCV to decode
and re-encode the same destination, retaining a four-module quiet zone and integer
scaling. It generates ignored `main/chant_qr_local.h`, a flash-resident I1 bitmap
with an eight-byte black/white palette. The supplied image produces a 196 square
pixel asset (4,908 bytes). The QR, its destination and paired firmware are local
deployment material and must not enter public commits.

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

The offline chant player uses the first 8:30 of
[this YouTube recording](https://www.youtube.com/watch?v=HQR_jVqWE_Y).
`music/dabei.mp3` is the retained 64 kbps mono excerpt with short fades at both
ends. No explicit redistribution license was found; these materials are used
locally for the owner's requested device, and are not committed, uploaded or
published.
`music/dabei.adpcm` is embedded into the application image by `main/CMakeLists.txt`.
It uses 11.025 kHz mono IMA ADPCM, high nibble first, initial predictor/index zero,
decoded into 16-bit PCM in 512-sample chunks; metadata and hashes are in
`music/dabei-metadata.json`. The runtime never allocates the entire recording.
Recreate it with `python3 tools/encode_chant_audio.py` (FFmpeg and Python 3.9
audioop required); the static gate compares the decoder with an independent oracle.

The v2 playlist adds two complete recordings from the
[official CTTB audio index](https://www.cttbusa.org/sitemap.asp.html):
[Guanyin Bodhisattva recitation](https://www.cttbusa.org/recitation/gwanyinchant.mp3)
(`music/guanyin.mp3`, 37.329 s),
[Disaster Eradicating Auspicious Spirit Mantra](https://www.cttbusa.org/recitation/auspicious_mantra.mp3)
(`music/auspicious.mp3`, 76.643 s). Each corresponding `.adpcm` is embedded,
with its own `*-metadata.json` hashes. A fourth track is the
[Heart Sutra](https://www.youtube.com/watch?v=j6YKyMu2Kfc) (`music/xinjing.mp3`,
305.850 s). The encoder regenerates all five tracks and
`main/chant_playlist.c` at 11.025 kHz; the two CTTB recordings and the Heart
Sutra and a 4:50 excerpt of a third YouTube Heart Sutra recitation
(`music/xinjinglong.mp3`, 290.000 s) are also embedded. The Great Compassion
source is limited to the 8:30 excerpt that fits alongside them. Fonts now
collect fixed text from every `main/chant_*.c` and generate a runtime inventory.
These added recordings have the same local-only, no-publication restriction.

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
