"""Validate the Melody 3D avatar GLB is present and loadable."""

from __future__ import annotations

import json
import struct
from pathlib import Path


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    glb = root / "miara_mesh_melody.glb"
    src = root / "miara_mesh_melody.png"
    assert src.is_file(), f"missing source {src}"
    assert glb.is_file(), f"missing avatar {glb}"
    data = glb.read_bytes()
    magic, version, length = struct.unpack_from("<III", data, 0)
    assert magic == 0x46546C67, hex(magic)
    assert version == 2
    assert length == len(data)
    json_len, json_type = struct.unpack_from("<II", data, 12)
    assert json_type == 0x4E4F534A
    gltf = json.loads(data[20 : 20 + json_len].decode("utf-8"))
    prim = gltf["meshes"][0]["primitives"][0]
    nverts = gltf["accessors"][prim["attributes"]["POSITION"]]["count"]
    nidx = gltf["accessors"][prim["indices"]]["count"]
    assert nverts > 1000, nverts
    assert nidx > 3000, nidx
    assert gltf["images"][0]["mimeType"] == "image/png"
    print(f"ok {glb.name} verts={nverts} indices={nidx} bytes={len(data)}")


if __name__ == "__main__":
    main()
