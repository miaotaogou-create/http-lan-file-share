# 将 app_icon.svg 渲成多尺寸 app_icon.ico（本地生成资源用）
from __future__ import annotations

import os
import sys

from PIL import Image
from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QGuiApplication, QImage, QPainter
from PySide6.QtSvg import QSvgRenderer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SVG = os.path.join(ROOT, "resources", "icons", "app_icon.svg")
ICO = os.path.join(ROOT, "resources", "icons", "app_icon.ico")
SIZES = (16, 24, 32, 48, 64, 128, 256)


def main() -> int:
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    app = QGuiApplication(sys.argv)

    renderer = QSvgRenderer(SVG)
    if not renderer.isValid():
        print("invalid svg", SVG, file=sys.stderr)
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
        pil = Image.frombuffer("RGBA", (s, s), raw, "raw", "BGRA", 0, 1).copy()
        images.append(pil)

    # PIL ICO：主图 + append_images；sizes 声明各层尺寸
    images[-1].save(
        ICO,
        format="ICO",
        sizes=[(im.width, im.height) for im in images],
        append_images=images[:-1],
    )
    print(f"wrote {ICO} ({os.path.getsize(ICO)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
