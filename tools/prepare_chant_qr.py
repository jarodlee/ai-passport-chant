#!/usr/bin/env python3
"""Prepare a scan-verified I1 QR asset (requires opencv-python).

Run with a local source image. The decoded destination is never logged. Re-encode
it without a logo, retain a four-module quiet zone, and use integer pixel scaling.
"""
from pathlib import Path
import argparse
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main():
    import cv2
    import numpy as np

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--public", action="store_true",
                        help="Write tracked assets; use only with owner publication permission")
    args = parser.parse_args()
    original = cv2.imread(str(args.image))
    if original is None:
        raise SystemExit("Cannot read source image")
    detector = cv2.QRCodeDetector()
    payload, _, _ = detector.detectAndDecode(original)
    if not payload:
        raise SystemExit("Source QR cannot be decoded")
    params = cv2.QRCodeEncoder_Params()
    params.correction_level = cv2.QRCodeEncoder_CORRECT_LEVEL_H
    encoded = cv2.QRCodeEncoder_create(params).encode(payload)
    ys, xs = np.where(encoded == 0)
    modules = encoded[ys.min():ys.max()+1, xs.min():xs.max()+1]
    if modules.shape[0] != modules.shape[1]:
        raise SystemExit("Expected a square QR matrix")
    modules = np.pad(modules, 4, constant_values=255)
    scale = 224 // modules.shape[0]
    if scale < 3:
        raise SystemExit("QR too dense for this display")
    pixels = np.repeat(np.repeat(modules, scale, axis=0), scale, axis=1)
    width = pixels.shape[0]
    stride = (width+7)//8
    data = bytearray([0, 0, 0, 255, 255, 255, 255, 255])
    data.extend(np.packbits(pixels > 127, axis=1, bitorder="big").tobytes())
    assert len(data) == 8+stride*width
    # Verify the bytes actually consumed by LVGL, including stride/palette.
    readback = np.unpackbits(np.frombuffer(data[8:], dtype=np.uint8).reshape(width, stride),
                            axis=1, bitorder="big")[:, :width]*255
    result, _, _ = detector.detectAndDecode(readback)
    if result != payload:
        raise SystemExit("Display-sized QR failed scan verification")
    output = ROOT/("assets/images/chant-qr.h" if args.public else "main/chant_qr_local.h")
    comment = "Owner-approved public QR bitmap." if args.public else "Private local QR bitmap; do not publish."
    output.write_text(f"// {comment}\n#pragma once\n" +
        f"#define CHANT_QR_WIDTH {width}\n#define CHANT_QR_HEIGHT {width}\n" +
        f"#define CHANT_QR_STRIDE {stride}\n" +
        "static const uint8_t chant_qr_pixels[] = {\n" +
        ",\n".join(",".join(f"0x{v:02x}" for v in data[i:i+24]) for i in range(0,len(data),24)) +
        "\n};\n")
    images = ROOT/"assets/images"
    stem = "chant-qr" if args.public else "chant-qr-local"
    local_source = images/(stem+args.image.suffix.lower())
    if args.image.resolve() != local_source.resolve():
        shutil.copyfile(args.image, local_source)
    cv2.imwrite(str(images/(stem+"-preview.png")),readback)
    print(f"QR: PASS ({width}x{width}, {scale} pixels/module, {len(data)} bytes, decoded match)")
    print("Public assets prepared" if args.public else "Private source, preview and header remain Git-ignored")


if __name__ == "__main__":
    main()
