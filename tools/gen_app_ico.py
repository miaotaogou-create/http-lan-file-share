# 从 app_icon.svg 生成 UI 用 SVG（去掉 Qt 不稳的 filter）与多尺寸 BMP-ICO
from __future__ import annotations

import os
import re
import struct
import sys
from pathlib import Path

from PIL import Image
from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QGuiApplication, QImage, QPainter
from PySide6.QtSvg import QSvgRenderer

ROOT = Path(__file__).resolve().parents[1]
SRC_SVG = ROOT / "resources" / "icons" / "app_icon.svg"
UI_SVG = ROOT / "resources" / "icons" / "app_icon_ui.svg"
ICO = ROOT / "resources" / "icons" / "app_icon.ico"
SIZES = (16, 24, 32, 48, 64, 128, 256)


def write_bmp_ico(images: list[Image.Image], path: Path) -> None:
    blobs: list[bytes] = []
    entries: list[tuple[int, int, int]] = []
    for im in images:
        w, h = im.size
        rgba = im.convert("RGBA")
        pixels = bytearray()
        for y in range(h - 1, -1, -1):
            for x in range(w):
                r, g, b, a = rgba.getpixel((x, y))
                pixels += bytes((b, g, r, a))
        row_bytes = ((w + 31) // 32) * 4
        mask = bytes(row_bytes * h)
        dib = struct.pack("<IIIHHIIIIII", 40, w, h * 2, 1, 32, 0, len(pixels), 0, 0, 0, 0)
        blob = dib + pixels + mask
        blobs.append(blob)
        entries.append((0 if w >= 256 else w, 0 if h >= 256 else h, len(blob)))

    offset = 6 + 16 * len(images)
    out = bytearray(struct.pack("<HHH", 0, 1, len(images)))
    for (w, h, size), blob in zip(entries, blobs):
        out += struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, size, offset)
        offset += size
    for blob in blobs:
        out += blob
    path.write_bytes(out)


def main() -> int:
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    src = SRC_SVG.read_text(encoding="utf-8")
    safe = re.sub(r"<filter[\s\S]*?</filter>\s*", "", src)
    safe = safe.replace(' filter="url(#glow)"', "")
    UI_SVG.write_text(safe, encoding="utf-8")

    app = QGuiApplication(sys.argv)
    renderer = QSvgRenderer(str(UI_SVG))
    if not renderer.isValid():
        print("invalid ui svg", file=sys.stderr)
        return 1

    images: list[Image.Image] = []
    for s in SIZES:
        qimg = QImage(s, s, QImage.Format.Format_ARGB32_Premultiplied)
        qimg.fill(Qt.GlobalColor.transparent)
        painter = QPainter(qimg)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
        painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform, True)
        renderer.render(painter, QRectF(0, 0, s, s))
        painter.end()
        raw = bytes(qimg.constBits()[: qimg.sizeInBytes()])
        images.append(Image.frombuffer("RGBA", (s, s), raw, "raw", "BGRA", 0, 1).copy())

    write_bmp_ico(images, ICO)
    print(f"wrote {UI_SVG.name} + {ICO.name} ({ICO.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
