"""Generate sized game maps from the original Substance Painter exports."""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def resize_map(path, size, normal, srgb):
    with Image.open(path) as image:
        rgba = image.convert("RGBA")
        if rgba.getextrema()[3] != (255, 255):
            raise ValueError(f"Opaque RGB export expected: {path}")
        data = np.array(rgba.convert("RGB"), dtype=np.float32) / 255.0
    if normal:
        data = data * 2.0 - 1.0
    elif srgb:
        data = np.where(data <= 0.04045, data / 12.92, ((data + 0.055) / 1.055) ** 2.4)
    filtered = np.stack([
        np.asarray(Image.fromarray(data[:, :, channel]).resize((size, size), Image.Resampling.LANCZOS))
        for channel in range(3)
    ], axis=-1)
    if normal:
        length = np.linalg.norm(filtered, axis=-1, keepdims=True)
        filtered = filtered / np.maximum(length, 1e-4) * 0.5 + 0.5
    elif srgb:
        filtered = np.clip(filtered, 0, 1)
        filtered = np.where(filtered <= 0.0031308, filtered * 12.92, 1.055 * filtered ** (1 / 2.4) - 0.055)
    return Image.fromarray(np.rint(np.clip(filtered, 0, 1) * 255).astype(np.uint8))


def build(profile):
    target_root = profile.parent.resolve()
    authoring_root = target_root.parent
    budget = json.loads(profile.read_text(encoding="utf-8"))
    for entry in budget["maps"]:
        source = (authoring_root / entry["file"]).resolve()
        target = (target_root / entry["file"]).resolve()
        if not source.is_relative_to(authoring_root) or not target.is_relative_to(target_root):
            raise ValueError(f"Map path outside texture profile: {entry['file']}")
        if source == target:
            raise ValueError(f"Map output overlaps its source: {source}")
        target.parent.mkdir(parents=True, exist_ok=True)
        resize_map(source, entry["size"], entry["channel"] == "Normal", entry["srgb"]).save(target)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", type=Path)
    build(parser.parse_args().profile)
