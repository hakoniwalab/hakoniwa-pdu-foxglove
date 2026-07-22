#!/usr/bin/env python3

import argparse
import shutil
import re
from pathlib import Path


DEFAULT_SOURCE_URDF = (
    Path("work")
    / "shadow_hand_source_urdf"
    / "sr_common"
    / "sr_description"
    / "mujoco_models"
    / "urdfs"
    / "shadowhand_motor.urdf"
)
DEFAULT_SOURCE_SR_DESCRIPTION = (
    Path("work")
    / "shadow_hand_source_urdf"
    / "sr_common"
    / "sr_description"
)
DEFAULT_OUTPUT_DIR = Path("work") / "urdf" / "shadow_hand"
DEFAULT_OUTPUT_URDF = DEFAULT_OUTPUT_DIR / "shadow_hand_original.urdf"
SHADOW_HAND_MESH_PATTERN = re.compile(
    r"package://sr_description/meshes/hand/([^\"/]+)\.dae"
)
XML_DECL_PATTERN = re.compile(r"<\?xml[^>]*\?>\s*")


def fail(message: str) -> None:
    raise SystemExit(f"Error: {message}")


def copy_sr_description(source: Path, destination: Path) -> None:
    if not source.is_dir():
        fail(f"sr_description directory not found: {source}")

    if destination.exists() or destination.is_symlink():
        if destination.is_dir() and not destination.is_symlink():
            shutil.rmtree(destination)
        else:
            destination.unlink()

    ignore = shutil.ignore_patterns(".git", "test", "__pycache__")
    shutil.copytree(source, destination, ignore=ignore)


def rewrite_urdf(source_urdf: Path, output_urdf: Path, asset_base_url: str) -> None:
    if not source_urdf.is_file():
        fail(f"source URDF not found: {source_urdf}")

    text = source_urdf.read_text(encoding="utf-8")
    base_url = asset_base_url.rstrip("/")
    text = XML_DECL_PATTERN.sub("", text).lstrip()

    def replace_shadow_hand_mesh(match: re.Match[str]) -> str:
        mesh_name = match.group(1)
        return (
            f"{base_url}/sr_description/mujoco_models/meshes/"
            f"arm_and_hand_meshes/visual/{mesh_name}.stl"
        )

    text = SHADOW_HAND_MESH_PATTERN.sub(replace_shadow_hand_mesh, text)
    text = text.replace("package://sr_description/", base_url + "/sr_description/")

    output_urdf.parent.mkdir(parents=True, exist_ok=True)
    output_urdf.write_text(text, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Prepare a local work-only Shadow Hand URDF for Foxglove. "
            "The generated URDF rewrites package://sr_description mesh URLs "
            "to HTTP URLs served by tools/serve_static_cors.py."
        )
    )
    parser.add_argument(
        "--source-urdf",
        default=DEFAULT_SOURCE_URDF,
        type=Path,
        help=f"Input plain URDF. Default: {DEFAULT_SOURCE_URDF}",
    )
    parser.add_argument(
        "--source-sr-description",
        default=DEFAULT_SOURCE_SR_DESCRIPTION,
        type=Path,
        help=f"Input sr_description directory. Default: {DEFAULT_SOURCE_SR_DESCRIPTION}",
    )
    parser.add_argument(
        "--output-urdf",
        default=DEFAULT_OUTPUT_URDF,
        type=Path,
        help=f"Output Foxglove-ready URDF. Default: {DEFAULT_OUTPUT_URDF}",
    )
    parser.add_argument(
        "--asset-base-url",
        default="http://127.0.0.1:8767",
        help="Base URL used by Foxglove to fetch URDF mesh assets.",
    )
    args = parser.parse_args()

    output_dir = args.output_urdf.parent
    output_dir.mkdir(parents=True, exist_ok=True)

    copied_sr_description = output_dir / "sr_description"
    copy_sr_description(args.source_sr_description, copied_sr_description)
    rewrite_urdf(args.source_urdf, args.output_urdf, args.asset_base_url)

    print(f"Wrote {args.output_urdf}")
    print(f"Copied mesh assets to {copied_sr_description}")
    print(f"Serve with: python3 tools/serve_static_cors.py --directory {output_dir} --port 8767")
    print(f"Foxglove URDF URL: {args.asset_base_url.rstrip('/')}/{args.output_urdf.name}")


if __name__ == "__main__":
    main()
