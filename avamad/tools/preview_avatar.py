"""Rasterize three views of a generated avatar mesh for visual verification."""

from __future__ import annotations

import io
import json
import struct
from pathlib import Path

import numpy as np
from PIL import Image


def _parse_glb_mesh(path: Path):
    data = path.read_bytes()
    json_len = struct.unpack_from("<I", data, 12)[0]
    gltf = json.loads(data[20 : 20 + json_len].decode("utf-8"))
    bin_off = 20 + json_len + 8
    blob = data[bin_off:]
    views = gltf["bufferViews"]
    acc = gltf["accessors"]

    def slurp(accessor_index, dtype, cols):
        a = acc[accessor_index]
        v = views[a["bufferView"]]
        raw = blob[v["byteOffset"] : v["byteOffset"] + v["byteLength"]]
        return np.frombuffer(raw, dtype=dtype).reshape(-1, cols).copy()

    prim = gltf["meshes"][0]["primitives"][0]
    pos = slurp(prim["attributes"]["POSITION"], np.float32, 3)
    uv = slurp(prim["attributes"]["TEXCOORD_0"], np.float32, 2)
    idx = slurp(prim["indices"], np.uint32, 1).reshape(-1, 3)
    img_view = views[gltf["images"][0]["bufferView"]]
    albedo = Image.open(io.BytesIO(blob[img_view["byteOffset"] : img_view["byteOffset"] + img_view["byteLength"]])).convert("RGB")
    return pos, uv, idx, np.asarray(albedo, dtype=np.uint8)


def _rotate_y(pos: np.ndarray, degrees: float) -> np.ndarray:
    rad = np.deg2rad(degrees)
    c, s = np.cos(rad), np.sin(rad)
    r = pos.copy()
    x, z = pos[:, 0], pos[:, 2]
    r[:, 0] = c * x + s * z
    r[:, 2] = -s * x + c * z
    return r


def _splat(pos, uv, albedo, width=420, height=640) -> Image.Image:
    canvas = np.zeros((height, width, 3), dtype=np.uint8)
    order = np.argsort(pos[:, 2])
    min_xy = pos[:, :2].min(axis=0)
    max_xy = pos[:, :2].max(axis=0)
    span = np.maximum(max_xy - min_xy, 1e-5)
    scale = float(min(0.92 * width / span[0], 0.92 * height / span[1]))
    origin = (np.array([width, height], dtype=np.float32) - (max_xy + min_xy) * scale) * 0.5
    p2 = pos[:, :2] * scale + origin
    xs = np.clip(p2[:, 0].astype(np.int32), 0, width - 1)
    ys = np.clip(height - 1 - p2[:, 1].astype(np.int32), 0, height - 1)
    ah, aw = albedo.shape[:2]
    tx = np.clip((uv[:, 0] * aw).astype(np.int32), 0, aw - 1)
    ty = np.clip(((1.0 - uv[:, 1]) * ah).astype(np.int32), 0, ah - 1)
    color = albedo[ty, tx]
    for radius in (2, 1, 0):
        for dx in range(-radius, radius + 1):
            for dy in range(-radius, radius + 1):
                canvas[np.clip(ys[order] + dy, 0, height - 1), np.clip(xs[order] + dx, 0, width - 1)] = color[order]
    return Image.fromarray(canvas, mode="RGB")


def main() -> None:
    glb = Path(__file__).resolve().parents[1] / "miara_mesh_melody.glb"
    out = glb.with_name("miara_mesh_melody.preview.png")
    pos, uv, idx, albedo = _parse_glb_mesh(glb)
    views = []
    for angle in (0.0, 32.0, 78.0):
        views.append(_splat(_rotate_y(pos, angle), uv, albedo))
    sheet = Image.new("RGB", (views[0].width * 3, views[0].height), (8, 8, 8))
    for i, view in enumerate(views):
        sheet.paste(view, (i * view.width, 0))
    sheet.save(out)
    print(out, sheet.size)


if __name__ == "__main__":
    main()
