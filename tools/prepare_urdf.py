#!/usr/bin/env python3

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

PACKAGE_URI_PATTERN = re.compile(r"^package://([^/]+)/(.+)$")


def fail(message: str) -> None:
    print(f"Error: {message}", file=sys.stderr)
    raise SystemExit(1)


def parse_vec3(raw: str, option_name: str) -> tuple[float, float, float]:
    parts = raw.replace(",", " ").split()
    if len(parts) != 3:
        fail(f"{option_name} requires exactly 3 numbers, got: {raw!r}")
    try:
        return tuple(float(value) for value in parts)  # type: ignore[return-value]
    except ValueError:
        fail(f"{option_name} contains a non-numeric value: {raw!r}")


def format_vec3(values: tuple[float, float, float]) -> str:
    return " ".join(f"{value:.15g}" for value in values)


def rewrite_package_uris(
    root: ET.Element,
    packages: set[str],
) -> dict[str, int]:
    counts = {package: 0 for package in packages}

    for element in root.iter():
        for attr_name, attr_value in list(element.attrib.items()):
            match = PACKAGE_URI_PATTERN.match(attr_value)
            if match is None:
                continue

            package_name, relative_path = match.groups()
            if package_name not in packages:
                continue

            element.set(attr_name, f"{package_name}/{relative_path}")
            counts[package_name] += 1

    return counts


def set_root_pose(
    root: ET.Element,
    joint_name: str,
    rpy: tuple[float, float, float] | None,
    xyz: tuple[float, float, float] | None,
) -> None:
    target_joint = None

    for joint in root.findall(".//joint"):
        if joint.get("name") == joint_name:
            target_joint = joint
            break

    if target_joint is None:
        fail(f"Joint not found: {joint_name}")

    origin = target_joint.find("origin")
    if origin is None:
        origin = ET.Element("origin")
        target_joint.insert(0, origin)

    if xyz is not None:
        origin.set("xyz", format_vec3(xyz))
    elif origin.get("xyz") is None:
        origin.set("xyz", "0 0 0")

    if rpy is not None:
        origin.set("rpy", format_vec3(rpy))
    elif origin.get("rpy") is None:
        origin.set("rpy", "0 0 0")


def indent_xml(root: ET.Element) -> None:
    # Python 3.9+
    ET.indent(root, space="  ")


def build_output_path(input_path: Path, output_arg: str | None, in_place: bool) -> Path:
    if in_place and output_arg:
        fail("Use either --in-place or --output, not both.")

    if in_place:
        return input_path

    if output_arg:
        return Path(output_arg)

    return input_path.with_name(f"{input_path.stem}.prepared{input_path.suffix}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Prepare a plain URDF for downstream viewers by rewriting selected "
            "package:// URIs to relative paths and optionally adjusting a root joint pose."
        )
    )
    parser.add_argument("input", help="Input URDF file.")
    parser.add_argument(
        "-o",
        "--output",
        help="Output URDF path. Default: <input>.prepared.urdf",
    )
    parser.add_argument(
        "--in-place",
        action="store_true",
        help="Rewrite the input URDF in place.",
    )
    parser.add_argument(
        "--package",
        action="append",
        default=[],
        metavar="NAME",
        help=(
            "Rewrite package://NAME/path to NAME/path. "
            "Repeat for multiple packages."
        ),
    )
    parser.add_argument(
        "--root-joint",
        help="Joint whose <origin> should be adjusted.",
    )
    parser.add_argument(
        "--root-rpy",
        metavar='"ROLL PITCH YAW"',
        help='Root joint RPY in radians, e.g. "3.141592653589793 -1.5707963267948966 0".',
    )
    parser.add_argument(
        "--root-xyz",
        metavar='"X Y Z"',
        help='Optional root joint translation, e.g. "0 0 0".',
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.is_file():
        fail(f"Input URDF not found: {input_path}")

    if (args.root_rpy or args.root_xyz) and not args.root_joint:
        fail("--root-rpy/--root-xyz require --root-joint.")

    output_path = build_output_path(input_path, args.output, args.in_place)

    try:
        tree = ET.parse(input_path)
    except ET.ParseError as exc:
        fail(f"Failed to parse URDF XML: {exc}")

    root = tree.getroot()

    packages = set(args.package)
    counts = rewrite_package_uris(root, packages)

    if args.root_joint:
        rpy = parse_vec3(args.root_rpy, "--root-rpy") if args.root_rpy else None
        xyz = parse_vec3(args.root_xyz, "--root-xyz") if args.root_xyz else None
        set_root_pose(root, args.root_joint, rpy, xyz)

    indent_xml(root)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    tree.write(output_path, encoding="utf-8", xml_declaration=True)

    print(f"Prepared URDF: {input_path} -> {output_path}")

    for package in sorted(packages):
        print(f"  package://{package}/ -> {package}/ : {counts[package]} replacement(s)")

    if args.root_joint:
        print(f"  root joint: {args.root_joint}")
        if args.root_xyz:
            print(f"    xyz: {args.root_xyz}")
        if args.root_rpy:
            print(f"    rpy: {args.root_rpy}")


if __name__ == "__main__":
    main()