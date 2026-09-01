"""Build a textured glTF 2.0 avatar (GLB) from a character render on black."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

import numpy as np
from PIL import Image


GLB_MAGIC = 0x46546C67
GLB_VERSION = 2
JSON_CHUNK = 0x4E4F534A
BIN_CHUNK = 0x004E4942


def _solid_mask(rgb: np.ndarray) -> np.ndarray:
    return rgb.max(axis=2) > 14


def _chamfer_distance(mask: np.ndarray) -> np.ndarray:
    h, w = mask.shape
    dist = np.full((h, w), 1.0e6, dtype=np.float32)
    dist[mask] = 0.0
    for y in range(h):
        row = dist[y]
        if y > 0:
            np.minimum(row, dist[y - 1] + 1.0, out=row)
        for x in range(1, w):
            if row[x] > row[x - 1] + 1.0:
                row[x] = row[x - 1] + 1.0
    for y in range(h - 1, -1, -1):
        row = dist[y]
        if y + 1 < h:
            np.minimum(row, dist[y + 1] + 1.0, out=row)
        for x in range(w - 2, -1, -1):
            if row[x] > row[x + 1] + 1.0:
                row[x] = row[x + 1] + 1.0
    return dist


def _emissive_map(rgb: np.ndarray, mask: np.ndarray) -> np.ndarray:
    r = rgb[:, :, 0].astype(np.float32)
    g = rgb[:, :, 1].astype(np.float32)
    b = rgb[:, :, 2].astype(np.float32)
    mx = np.maximum(np.maximum(r, g), b)
    mn = np.minimum(np.minimum(r, g), b)
    sat = np.divide(mx - mn, mx, out=np.zeros_like(mx), where=mx > 8.0)
    warm = ((r > 140) & (g > 40) & (b < 140) & (r > b + 20)).astype(np.float32)
    glow = np.clip(sat * (mx / 255.0) * 1.6 + warm * 0.85, 0.0, 1.0)
    glow *= mask.astype(np.float32)
    emi = np.zeros_like(rgb)
    emi[:, :, 0] = np.clip(r * glow, 0, 255)
    emi[:, :, 1] = np.clip(g * glow * 0.55, 0, 255)
    emi[:, :, 2] = np.clip(b * glow * 0.35, 0, 255)
    return emi.astype(np.uint8)


def build_mesh(rgb: np.ndarray, columns: int = 140):
    h, w, _ = rgb.shape
    mask = _solid_mask(rgb)
    ys, xs = np.nonzero(mask)
    if ys.size == 0:
        raise ValueError("source image has no non-black silhouette")

    y0, y1 = int(ys.min()), int(ys.max())
    x0, x1 = int(xs.min()), int(xs.max())
    bw = max(1, x1 - x0)
    bh = max(1, y1 - y0)
    step = max(2, int(round(bw / columns)))
    gx = list(range(x0, x1 + 1, step))
    gy = list(range(y0, y1 + 1, step))
    if gx[-1] != x1:
        gx.append(x1)
    if gy[-1] != y1:
        gy.append(y1)

    small_mask = mask[y0 : y1 + 1 : step, x0 : x1 + 1 : step]
    edge = _chamfer_distance(~small_mask)
    max_edge = float(edge[small_mask].max()) if small_mask.any() else 1.0
    thickness = (max(bw, bh) / float(w)) * 0.22

    index_of: dict[tuple[int, int], int] = {}
    positions: list[float] = []
    uvs: list[float] = []
    colors: list[float] = []

    aspect = h / float(w)
    height_world = 1.70
    width_world = height_world / aspect

    def add_vert(ix: int, iy: int, sign: float) -> int:
        key = (ix, iy, int(sign))
        cached = index_of.get(key)
        if cached is not None:
            return cached
        px, py = gx[ix], gy[iy]
        u = (px + 0.5) / w
        v = 1.0 - (py + 0.5) / h
        sx = min(small_mask.shape[1] - 1, max(0, int(round((px - x0) / step))))
        sy = min(small_mask.shape[0] - 1, max(0, int(round((py - y0) / step))))
        depth_n = float(edge[sy, sx]) / max(max_edge, 1.0)
        luma = float(rgb[py, px].max()) / 255.0
        z = sign * thickness * (0.12 + 0.88 * depth_n) * (0.65 + 0.35 * luma)
        x = ((px - x0) / bw - 0.5) * width_world
        y = (1.0 - (py - y0) / bh) * height_world
        idx = len(positions) // 3
        positions.extend((x, y, z))
        uvs.extend((u, v))
        c = rgb[py, px].astype(np.float32) / 255.0
        colors.extend((float(c[0]), float(c[1]), float(c[2]), 1.0))
        index_of[key] = idx
        return idx

    indices: list[int] = []
    ny, nx = len(gy), len(gx)
    for j in range(ny - 1):
        for i in range(nx - 1):
            corners = ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))
            if not all(mask[gy[cj], gx[ci]] for ci, cj in corners):
                continue
            for sign in (1.0, -1.0):
                a = add_vert(corners[0][0], corners[0][1], sign)
                b = add_vert(corners[1][0], corners[1][1], sign)
                c = add_vert(corners[2][0], corners[2][1], sign)
                d = add_vert(corners[3][0], corners[3][1], sign)
                if sign > 0:
                    indices.extend((a, b, c, a, c, d))
                else:
                    indices.extend((a, c, b, a, d, c))

    if not indices:
        raise ValueError("silhouette produced no triangles")

    pos = np.asarray(positions, dtype=np.float32).reshape(-1, 3)
    nrm = np.zeros_like(pos)
    idx = np.asarray(indices, dtype=np.uint32).reshape(-1, 3)
    v0 = pos[idx[:, 0]]
    v1 = pos[idx[:, 1]]
    v2 = pos[idx[:, 2]]
    face_n = np.cross(v1 - v0, v2 - v0)
    for k in range(3):
        np.add.at(nrm, idx[:, k], face_n)
    lengths = np.linalg.norm(nrm, axis=1)
    lengths[lengths == 0] = 1.0
    nrm /= lengths[:, None]

    return {
        "positions": pos.reshape(-1),
        "normals": nrm.reshape(-1).astype(np.float32),
        "uvs": np.asarray(uvs, dtype=np.float32),
        "colors": np.asarray(colors, dtype=np.float32),
        "indices": np.asarray(indices, dtype=np.uint32),
    }


def _pad4(data: bytes, pad: bytes) -> bytes:
    rem = len(data) % 4
    return data if rem == 0 else data + pad * (4 - rem)


def write_glb(path: Path, mesh: dict, albedo_png: bytes, emissive_png: bytes, name: str) -> None:
    pos = mesh["positions"].tobytes()
    nrm = mesh["normals"].tobytes()
    uvs = mesh["uvs"].tobytes()
    cols = mesh["colors"].tobytes()
    idx = mesh["indices"].tobytes()

    blobs = [pos, nrm, uvs, cols, idx, albedo_png, emissive_png]
    offsets = []
    cursor = 0
    packed = b""
    for blob in blobs:
        blob = _pad4(blob, b"\x00")
        offsets.append((cursor, len(blob)))
        packed += blob
        cursor += len(blob)

    pos_a = mesh["positions"].reshape(-1, 3)
    nrm_a = mesh["normals"].reshape(-1, 3)
    uv_a = mesh["uvs"].reshape(-1, 2)
    col_a = mesh["colors"].reshape(-1, 4)
    nverts = pos_a.shape[0]
    nidx = int(mesh["indices"].size)

    def acc_minmax(arr):
        return arr.min(axis=0).tolist(), arr.max(axis=0).tolist()

    pmin, pmax = acc_minmax(pos_a)
    nmin, nmax = acc_minmax(nrm_a)
    umin, umax = acc_minmax(uv_a)
    cmin, cmax = acc_minmax(col_a)

    gltf = {
        "asset": {"version": "2.0", "generator": "avamad/tools/image_to_avatar.py"},
        "scene": 0,
        "scenes": [{"name": name, "nodes": [0]}],
        "nodes": [
            {
                "name": name,
                "mesh": 0,
                "translation": [0.0, 0.0, 0.0],
            }
        ],
        "meshes": [
            {
                "name": name,
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": 0,
                            "NORMAL": 1,
                            "TEXCOORD_0": 2,
                            "COLOR_0": 3,
                        },
                        "indices": 4,
                        "material": 0,
                    }
                ],
            }
        ],
        "materials": [
            {
                "name": f"{name}Skin",
                "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": 0},
                    "metallicFactor": 0.18,
                    "roughnessFactor": 0.42,
                },
                "emissiveFactor": [1.0, 1.0, 1.0],
                "emissiveTexture": {"index": 1},
                "alphaMode": "MASK",
                "alphaCutoff": 0.08,
                "doubleSided": True,
            }
        ],
        "textures": [{"source": 0}, {"source": 1}],
        "images": [
            {"mimeType": "image/png", "bufferView": 5, "name": "albedo"},
            {"mimeType": "image/png", "bufferView": 6, "name": "emissive"},
        ],
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 33071, "wrapT": 33071}],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": nverts,
                "type": "VEC3",
                "min": pmin,
                "max": pmax,
            },
            {
                "bufferView": 1,
                "componentType": 5126,
                "count": nverts,
                "type": "VEC3",
                "min": nmin,
                "max": nmax,
            },
            {
                "bufferView": 2,
                "componentType": 5126,
                "count": nverts,
                "type": "VEC2",
                "min": umin,
                "max": umax,
            },
            {
                "bufferView": 3,
                "componentType": 5126,
                "count": nverts,
                "type": "VEC4",
                "min": cmin,
                "max": cmax,
            },
            {
                "bufferView": 4,
                "componentType": 5125,
                "count": nidx,
                "type": "SCALAR",
            },
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": offsets[0][0], "byteLength": len(pos), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[1][0], "byteLength": len(nrm), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[2][0], "byteLength": len(uvs), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[3][0], "byteLength": len(cols), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[4][0], "byteLength": len(idx), "target": 34963},
            {"buffer": 0, "byteOffset": offsets[5][0], "byteLength": len(albedo_png)},
            {"buffer": 0, "byteOffset": offsets[6][0], "byteLength": len(emissive_png)},
        ],
        "buffers": [{"byteLength": len(packed)}],
    }

    json_bytes = _pad4(json.dumps(gltf, separators=(",", ":")).encode("utf-8"), b" ")
    bin_bytes = _pad4(packed, b"\x00")
    total = 12 + 8 + len(json_bytes) + 8 + len(bin_bytes)
    header = struct.pack("<III", GLB_MAGIC, GLB_VERSION, total)
    json_hdr = struct.pack("<II", len(json_bytes), JSON_CHUNK)
    bin_hdr = struct.pack("<II", len(bin_bytes), BIN_CHUNK)
    path.write_bytes(header + json_hdr + json_bytes + bin_hdr + bin_bytes)


def convert(src: Path, dst: Path, columns: int) -> dict:
    image = Image.open(src).convert("RGB")
    rgb = np.asarray(image, dtype=np.uint8)
    mesh = build_mesh(rgb, columns=columns)
    mask = _solid_mask(rgb)
    emissive = Image.fromarray(_emissive_map(rgb, mask), mode="RGB")
    albedo_buf = __import__("io").BytesIO()
    emissive_buf = __import__("io").BytesIO()
    image.save(albedo_buf, format="PNG")
    emissive.save(emissive_buf, format="PNG")
    write_glb(dst, mesh, albedo_buf.getvalue(), emissive_buf.getvalue(), src.stem)
    return {
        "source": str(src),
        "output": str(dst),
        "vertices": int(mesh["positions"].size // 3),
        "triangles": int(mesh["indices"].size // 3),
        "bytes": dst.stat().st_size,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    parser.add_argument("--columns", type=int, default=140)
    args = parser.parse_args()
    src = args.source.resolve()
    dst = (args.output or src.with_suffix(".glb")).resolve()
    info = convert(src, dst, args.columns)
    print(json.dumps(info, indent=2))


if __name__ == "__main__":
    main()
