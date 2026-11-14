from pathlib import Path
import json
import cv2
import numpy as np

root = Path(r"D:\Projects\Mystic-Turtle-Launcher\assets")

# Normalized eye centers for the supplied illustrations. OpenCV checks the
# local contrast so a flat/closed eye strip is automatically disabled.
eye_boxes = {
    "portrait-maid.png": None,
    "portrait-white-hoodie.png": ((0.455, 0.135), (0.545, 0.135)),
    "portrait-red.png": ((0.455, 0.170), (0.545, 0.170)),
    "portrait-pink.png": ((0.455, 0.138), (0.545, 0.138)),
    "portrait-blue-hat.png": ((0.450, 0.148), (0.550, 0.148)),
    "portrait-pink-teddy.png": ((0.455, 0.130), (0.545, 0.130)),
    "portrait-black-ponytail.png": ((0.465, 0.145), (0.535, 0.145)),
    "portrait-twintails.png": ((0.455, 0.165), (0.545, 0.165)),
    "portrait-black-dress.png": ((0.445, 0.135), (0.555, 0.135)),
    "portrait-white-hoodie-2.png": ((0.455, 0.135), (0.545, 0.135)),
    "portrait-pink-tee.png": ((0.455, 0.130), (0.545, 0.130)),
}

output = {}
for name, points in eye_boxes.items():
    image = cv2.imread(str(root / name), cv2.IMREAD_UNCHANGED)
    if image is None:
        continue
    gray = cv2.cvtColor(image[:, :, :3], cv2.COLOR_BGR2GRAY)
    h, w = gray.shape[:2]
    enabled = points is not None
    if enabled:
        (lx, ly), (rx, ry) = points
        x0 = int(w * min(lx, rx) - 60)
        x1 = int(w * max(lx, rx) + 60)
        y0 = int(h * min(ly, ry) - 45)
        y1 = int(h * max(ly, ry) + 45)
        strip = gray[max(0, y0):min(h, y1), max(0, x0):min(w, x1)]
        enabled = strip.size > 0 and float(np.std(strip)) > 8.0
    if enabled:
        (lx, ly), (rx, ry) = points
        output[name] = {
            "eyes": True,
            "left": [round(lx, 4), round(ly, 4)],
            "right": [round(rx, 4), round(ry, 4)],
            "radius": [0.05, 0.025],
        }
    else:
        output[name] = {"eyes": False}

(root / "portrait-eyes.json").write_text(
    json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")
print("wrote", root / "portrait-eyes.json")
