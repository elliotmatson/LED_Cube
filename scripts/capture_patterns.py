#!/usr/bin/env python3
"""Record animations of the cube's patterns from a running cube.

Switches the cube to each pattern through the REST API, grabs frames from
GET /api/v1/frame (raw RGB888, 192 x 64, the same frame the panels show), and
draws them as the cube looks from its shared corner: every LED placed by the
isometric projection the firmware uses (cube::projectXf / projectYf), top
face up.

Needs Pillow:
    uv run --with pillow scripts/capture_patterns.py --host cube.local

Writes docs/patterns/<pattern id>.webp (animated WebP: a fifth the size of a
GIF here, where the fine LED texture defeats GIF compression; --format gif
if you need one). Leaves the cube on the pattern it was on.
"""

import argparse
import json
import time
import urllib.request
from pathlib import Path

from PIL import Image, ImageDraw

WIDTH, HEIGHT = 192, 64
FRAME_BYTES = WIDTH * HEIGHT * 3

# Per pattern: seconds to let it settle, seconds to record. Some need longer
# to show what they do (a Rubik's scramble, sand piling up).
TIMING = {
    "rubiks_cube": (1.0, 12.0),
    "falling_sand": (8.0, 6.0),
    "clock": (2.0, 4.0),
    "spotify": (6.0, 6.0),
    "game_of_life": (2.0, 5.0),
}
DEFAULT_TIMING = (2.0, 5.0)


def project(x: int, y: int) -> tuple[float, float]:
    """cube::projectXf and projectYf, from lib/cube_geometry."""
    if x < 64:
        return 0.8660254 * (x - y), (130 - x - y) * 0.5
    if x < 128:
        return 111.7173 - 0.8660254 * x, y - x * 0.5
    return 109.1192 - 0.8660254 * x, x * 0.5 + y - 128


def api(host: str, path: str, method: str = "GET") -> bytes:
    req = urllib.request.Request(f"http://{host}/api{path}", method=method)
    with urllib.request.urlopen(req, timeout=10) as resp:
        return resp.read()


class Renderer:
    """Draws a raw frame as LEDs in the isometric view."""

    # Drawn this many times larger, then scaled down: at GIF size the LEDs
    # are ~2.6 px apart, and rounding each to whole pixels leaves stripes.
    SUPERSAMPLE = 4

    def __init__(self, scale: float):
        self.scale = scale
        ss = scale * self.SUPERSAMPLE
        pts = [project(x, y) for y in range(HEIGHT) for x in range(WIDTH)]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        margin = 3
        self.min_x, self.max_y = min(xs) - margin, max(ys) + margin
        self.size = (
            int((max(xs) - min(xs) + 2 * margin) * scale),
            int((max(ys) - min(ys) + 2 * margin) * scale),
        )
        self.big = (self.size[0] * self.SUPERSAMPLE, self.size[1] * self.SUPERSAMPLE)
        r = 0.5 * ss
        # Screen boxes for every LED, row-major like the frame.
        self.boxes = []
        for (px, py) in pts:
            cx = (px - self.min_x) * ss
            cy = (self.max_y - py) * ss  # top face (larger Y) at the top
            self.boxes.append((cx - r, cy - r, cx + r, cy + r))

    def render(self, frame: bytes) -> Image.Image:
        img = Image.new("RGB", self.big, (0, 0, 0))
        draw = ImageDraw.Draw(img)
        for i, box in enumerate(self.boxes):
            r, g, b = frame[i * 3], frame[i * 3 + 1], frame[i * 3 + 2]
            if r or g or b:
                draw.ellipse(box, fill=(r, g, b))
        return img.resize(self.size, Image.Resampling.LANCZOS)


def record(host: str, pattern: str, renderer: Renderer, fps: float, out: Path, fmt: str) -> int:
    settle, seconds = TIMING.get(pattern, DEFAULT_TIMING)
    api(host, f"/v1/patterns?id={pattern}", "POST")
    time.sleep(settle)
    frames, stamps = [], []
    end = time.monotonic() + seconds
    interval = 1.0 / fps
    while time.monotonic() < end:
        start = time.monotonic()
        data = api(host, "/v1/frame")
        if len(data) == FRAME_BYTES:
            frames.append(renderer.render(data))
            stamps.append(start)
        time.sleep(max(0.0, interval - (time.monotonic() - start)))
    if not frames:
        raise RuntimeError(f"no frames from {pattern}")
    # Each frame shown for as long as it actually lasted.
    durations = [max(20, int((b - a) * 1000)) for a, b in zip(stamps, stamps[1:])]
    durations.append(durations[-1] if durations else 100)
    out.parent.mkdir(parents=True, exist_ok=True)
    if fmt == "webp":
        frames[0].save(out, save_all=True, append_images=frames[1:], duration=durations, loop=0, quality=85, method=6)
    else:
        frames[0].save(out, save_all=True, append_images=frames[1:], duration=durations, loop=0, optimize=True, disposal=1)
    return len(frames)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="cube.local")
    ap.add_argument("--out", default="docs/patterns", type=Path)
    ap.add_argument("--format", default="webp", choices=["webp", "gif"])
    ap.add_argument("--fps", default=10.0, type=float)
    ap.add_argument("--scale", default=3.0, type=float, help="pixels per LED spacing")
    ap.add_argument("patterns", nargs="*", help="pattern ids (default: all)")
    args = ap.parse_args()

    listing = json.loads(api(args.host, "/v1/patterns"))
    original = listing["current"]
    ids = args.patterns or [p["id"] for p in listing["patterns"]]
    renderer = Renderer(args.scale)
    try:
        for pattern in ids:
            path = args.out / f"{pattern}.{args.format}"
            n = record(args.host, pattern, renderer, args.fps, path, args.format)
            print(f"{pattern}: {n} frames, {path.stat().st_size // 1024} KB -> {path}")
    finally:
        api(args.host, f"/v1/patterns?id={original}", "POST")


if __name__ == "__main__":
    main()
