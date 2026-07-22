#!/usr/bin/env python3
"""Generate a Foxglove-friendly URDF from Menagerie's Shadow Hand MJCF."""

from __future__ import annotations

import argparse
import math
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path


DEFAULT_MJCF = (
    Path(__file__).resolve().parents[2]
    / "hakoniwa-mujoco-robots"
    / "thirdparty/mujoco_menagerie/shadow_hand/right_hand.xml"
)
DEFAULT_OUTPUT = Path("work/urdf/shadow_hand/shadow_hand_right.urdf")
DEFAULT_MESH_URL = "http://127.0.0.1:8767/assets"

EXPECTED_JOINTS = [
    "rh_WRJ2",
    "rh_WRJ1",
    "rh_THJ5",
    "rh_THJ4",
    "rh_THJ3",
    "rh_THJ2",
    "rh_THJ1",
    "rh_FFJ4",
    "rh_FFJ3",
    "rh_FFJ2",
    "rh_FFJ1",
    "rh_MFJ4",
    "rh_MFJ3",
    "rh_MFJ2",
    "rh_MFJ1",
    "rh_RFJ4",
    "rh_RFJ3",
    "rh_RFJ2",
    "rh_RFJ1",
    "rh_LFJ5",
    "rh_LFJ4",
    "rh_LFJ3",
    "rh_LFJ2",
    "rh_LFJ1",
]


@dataclass
class CompiledMesh:
    filename: str
    material: str


@dataclass
class CompiledVisual:
    filename: str
    material: str


def parse_vec(text: str | None, size: int, default: list[float]) -> list[float]:
    if not text:
        return list(default)
    values = [float(item) for item in text.split()]
    if len(values) != size:
        raise ValueError(f"expected {size} values, got {len(values)}: {text}")
    return values


def format_vec(values: list[float]) -> str:
    return " ".join(f"{value:.9g}" for value in values)


def normalize_quat(quat: list[float]) -> list[float]:
    norm = math.sqrt(sum(value * value for value in quat))
    if norm == 0:
        return [1.0, 0.0, 0.0, 0.0]
    return [value / norm for value in quat]


def quat_to_rpy(quat: list[float]) -> list[float]:
    w, x, y, z = normalize_quat(quat)

    sinr_cosp = 2 * (w * x + y * z)
    cosr_cosp = 1 - 2 * (x * x + y * y)
    roll = math.atan2(sinr_cosp, cosr_cosp)

    sinp = 2 * (w * y - z * x)
    if abs(sinp) >= 1:
        pitch = math.copysign(math.pi / 2, sinp)
    else:
        pitch = math.asin(sinp)

    siny_cosp = 2 * (w * z + x * y)
    cosy_cosp = 1 - 2 * (y * y + z * z)
    yaw = math.atan2(siny_cosp, cosy_cosp)

    return [roll, pitch, yaw]


def quat_mul(a: list[float], b: list[float]) -> list[float]:
    aw, ax, ay, az = normalize_quat(a)
    bw, bx, by, bz = normalize_quat(b)
    return normalize_quat(
        [
            aw * bw - ax * bx - ay * by - az * bz,
            aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
        ]
    )


def rotate_vec(quat: list[float], vec: list[float]) -> list[float]:
    w, x, y, z = normalize_quat(quat)
    vx, vy, vz = vec
    # Efficient q * v * q^-1 without normalizing the pure-vector quaternion.
    tx = 2.0 * (y * vz - z * vy)
    ty = 2.0 * (z * vx - x * vz)
    tz = 2.0 * (x * vy - y * vx)
    return [
        vx + w * tx + (y * tz - z * ty),
        vy + w * ty + (z * tx - x * tz),
        vz + w * tz + (x * ty - y * tx),
    ]


def compose_pose(
    pos_a: list[float],
    quat_a: list[float],
    pos_b: list[float],
    quat_b: list[float],
) -> tuple[list[float], list[float]]:
    rotated_b = rotate_vec(quat_a, pos_b)
    return (
        [pos_a[i] + rotated_b[i] for i in range(3)],
        quat_mul(quat_a, quat_b),
    )


def indent(element: ET.Element, level: int = 0) -> None:
    pad = "\n" + level * "  "
    child_pad = "\n" + (level + 1) * "  "
    if len(element):
        if not element.text or not element.text.strip():
            element.text = child_pad
        for child in element:
            indent(child, level + 1)
        if not child.tail or not child.tail.strip():
            child.tail = pad
    if level and (not element.tail or not element.tail.strip()):
        element.tail = pad


def add_origin(parent: ET.Element, pos: list[float], quat: list[float]) -> None:
    ET.SubElement(
        parent,
        "origin",
        {
            "xyz": format_vec(pos),
            "rpy": format_vec(quat_to_rpy(quat)),
        },
    )


def build_default_map(root: ET.Element) -> dict[str, dict[str, dict[str, str]]]:
    defaults: dict[str, dict[str, dict[str, str]]] = {}

    def merge(parent: dict[str, dict[str, str]], node: ET.Element) -> dict[str, dict[str, str]]:
        current = {kind: dict(attrs) for kind, attrs in parent.items()}
        for kind in ("mesh", "joint", "geom", "position"):
            child = node.find(kind)
            if child is not None:
                current.setdefault(kind, {}).update(child.attrib)
        return current

    def visit(node: ET.Element, inherited: dict[str, dict[str, str]]) -> None:
        current = merge(inherited, node)
        class_name = node.get("class")
        if class_name:
            defaults[class_name] = {kind: dict(attrs) for kind, attrs in current.items()}
        for child in node.findall("default"):
            visit(child, current)

    empty: dict[str, dict[str, str]] = {}
    for default in root.findall("default"):
        visit(default, empty)
    return defaults


def attrs_for(
    defaults: dict[str, dict[str, dict[str, str]]],
    element: ET.Element,
    kind: str,
    fallback_class: str | None,
) -> dict[str, str]:
    class_name = element.get("class") or fallback_class
    attrs: dict[str, str] = {}
    if class_name and class_name in defaults:
        attrs.update(defaults[class_name].get(kind, {}))
    attrs.update(element.attrib)
    return attrs


def material_map(root: ET.Element) -> dict[str, str]:
    result = {
        "black": "0.16355 0.16355 0.16355 1",
        "gray": "0.80848 0.80848 0.80848 1",
        "metallic": "0.9 0.9 0.9 1",
    }
    asset = root.find("asset")
    if asset is None:
        return result
    for material in asset.findall("material"):
        name = material.get("name")
        rgba = material.get("rgba")
        if name and rgba:
            result[name] = rgba
    return result


def material_name_by_id(root: ET.Element) -> dict[int, str]:
    asset = root.find("asset")
    if asset is None:
        return {}
    names: dict[int, str] = {}
    for index, material in enumerate(asset.findall("material")):
        name = material.get("name")
        if name:
            names[index] = name
    return names


def rotate_vertex(quat: list[float], vertex) -> list[float]:
    return rotate_vec(quat, [float(vertex[0]), float(vertex[1]), float(vertex[2])])


def write_compiled_visuals(
    mjcf: Path,
    output_assets: Path,
    material_names: dict[int, str],
) -> dict[str, list[CompiledVisual]]:
    try:
        import mujoco
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "MuJoCo Python is required for Foxglove-correct Shadow Hand URDF generation. "
            "Run with the Python environment that has mujoco installed, for example "
            "/Users/tmori/.pyenv/shims/python3.12."
        ) from exc

    model = mujoco.MjModel.from_xml_path(str(mjcf))
    output_assets.mkdir(parents=True, exist_ok=True)
    visuals: dict[str, list[CompiledVisual]] = {}

    for geom_id in range(model.ngeom):
        if int(model.geom_group[geom_id]) != 2:
            continue
        mesh_id = int(model.geom_dataid[geom_id])
        if mesh_id < 0:
            continue
        mesh_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_MESH, mesh_id)
        body_id = int(model.geom_bodyid[geom_id])
        body_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, body_id)
        if not mesh_name or not body_name:
            continue

        vert_adr = int(model.mesh_vertadr[mesh_id])
        vert_num = int(model.mesh_vertnum[mesh_id])
        face_adr = int(model.mesh_faceadr[mesh_id])
        face_num = int(model.mesh_facenum[mesh_id])
        vertices = model.mesh_vert[vert_adr : vert_adr + vert_num]
        faces = model.mesh_face[face_adr : face_adr + face_num]

        geom_pos = [float(value) for value in model.geom_pos[geom_id]]
        geom_quat = [float(value) for value in model.geom_quat[geom_id]]
        mat_id = int(model.geom_matid[geom_id])
        material = material_names.get(mat_id, "black")

        filename = f"{body_name}_{mesh_name}_{geom_id}.compiled.obj"
        with (output_assets / filename).open("w", encoding="utf-8") as fp:
            fp.write(f"# Body-local compiled MuJoCo geom for {body_name}/{mesh_name}\n")
            for vertex in vertices:
                rotated = rotate_vertex(geom_quat, vertex)
                x = geom_pos[0] + rotated[0]
                y = geom_pos[1] + rotated[1]
                z = geom_pos[2] + rotated[2]
                fp.write(f"v {x:.9g} {y:.9g} {z:.9g}\n")
            for a, b, c in faces:
                fp.write(f"f {int(a) + 1} {int(b) + 1} {int(c) + 1}\n")

        visuals.setdefault(body_name, []).append(
            CompiledVisual(filename=filename, material=material)
        )

    return visuals


def matrix_to_quat(matrix) -> list[float]:
    m00, m01, m02 = float(matrix[0][0]), float(matrix[0][1]), float(matrix[0][2])
    m10, m11, m12 = float(matrix[1][0]), float(matrix[1][1]), float(matrix[1][2])
    m20, m21, m22 = float(matrix[2][0]), float(matrix[2][1]), float(matrix[2][2])
    trace = m00 + m11 + m22
    if trace > 0.0:
        s = math.sqrt(trace + 1.0) * 2.0
        return normalize_quat([0.25 * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s])
    if m00 > m11 and m00 > m22:
        s = math.sqrt(1.0 + m00 - m11 - m22) * 2.0
        return normalize_quat([(m21 - m12) / s, 0.25 * s, (m01 + m10) / s, (m02 + m20) / s])
    if m11 > m22:
        s = math.sqrt(1.0 + m11 - m00 - m22) * 2.0
        return normalize_quat([(m02 - m20) / s, (m01 + m10) / s, 0.25 * s, (m12 + m21) / s])
    s = math.sqrt(1.0 + m22 - m00 - m11) * 2.0
    return normalize_quat([(m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25 * s])


def mat_rotate_vec(matrix, vertex) -> list[float]:
    return [
        float(matrix[0][0]) * float(vertex[0]) + float(matrix[0][1]) * float(vertex[1]) + float(matrix[0][2]) * float(vertex[2]),
        float(matrix[1][0]) * float(vertex[0]) + float(matrix[1][1]) * float(vertex[1]) + float(matrix[1][2]) * float(vertex[2]),
        float(matrix[2][0]) * float(vertex[0]) + float(matrix[2][1]) * float(vertex[1]) + float(matrix[2][2]) * float(vertex[2]),
    ]


def write_baked_visuals(
    mjcf: Path,
    output_assets: Path,
    material_names: dict[int, str],
) -> list[CompiledVisual]:
    try:
        import mujoco
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "MuJoCo Python is required for Foxglove-correct Shadow Hand URDF generation. "
            "Run with the Python environment that has mujoco installed, for example "
            "/Users/tmori/.pyenv/shims/python3.12."
        ) from exc

    model = mujoco.MjModel.from_xml_path(str(mjcf))
    data = mujoco.MjData(model)
    mujoco.mj_forward(model, data)
    output_assets.mkdir(parents=True, exist_ok=True)
    visuals: list[CompiledVisual] = []

    for geom_id in range(model.ngeom):
        if int(model.geom_group[geom_id]) != 2:
            continue
        mesh_id = int(model.geom_dataid[geom_id])
        if mesh_id < 0:
            continue
        mesh_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_MESH, mesh_id)
        body_id = int(model.geom_bodyid[geom_id])
        body_name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_BODY, body_id)
        if not mesh_name or not body_name:
            continue

        vert_adr = int(model.mesh_vertadr[mesh_id])
        vert_num = int(model.mesh_vertnum[mesh_id])
        face_adr = int(model.mesh_faceadr[mesh_id])
        face_num = int(model.mesh_facenum[mesh_id])
        vertices = model.mesh_vert[vert_adr : vert_adr + vert_num]
        faces = model.mesh_face[face_adr : face_adr + face_num]
        geom_xpos = [float(value) for value in data.geom_xpos[geom_id]]
        geom_xmat = data.geom_xmat[geom_id].reshape(3, 3)
        mat_id = int(model.geom_matid[geom_id])
        material = material_names.get(mat_id, "black")

        filename = f"baked_{body_name}_{mesh_name}_{geom_id}.compiled.obj"
        with (output_assets / filename).open("w", encoding="utf-8") as fp:
            fp.write(f"# World-baked MuJoCo geom for {body_name}/{mesh_name}\n")
            for vertex in vertices:
                rotated = mat_rotate_vec(geom_xmat, vertex)
                x = geom_xpos[0] + rotated[0]
                y = geom_xpos[1] + rotated[1]
                z = geom_xpos[2] + rotated[2]
                fp.write(f"v {x:.9g} {y:.9g} {z:.9g}\n")
            for a, b, c in faces:
                fp.write(f"f {int(a) + 1} {int(b) + 1} {int(c) + 1}\n")
        visuals.append(CompiledVisual(filename=filename, material=material))

    return visuals


def add_link_visuals(
    urdf_link: ET.Element,
    body_name: str,
    mesh_base_url: str,
    compiled_visuals: dict[str, list[CompiledVisual]],
) -> int:
    count = 0
    for compiled_visual in compiled_visuals.get(body_name, []):
        visual = ET.SubElement(urdf_link, "visual")
        add_origin(
            visual,
            [0.0, 0.0, 0.0],
            [1.0, 0.0, 0.0, 0.0],
        )
        geometry = ET.SubElement(visual, "geometry")
        ET.SubElement(
            geometry,
            "mesh",
            {
                "filename": f"{mesh_base_url.rstrip('/')}/{compiled_visual.filename}",
                "scale": "1 1 1",
            },
        )
        ET.SubElement(visual, "material", {"name": compiled_visual.material})
        count += 1
    return count


def add_joint(
    robot: ET.Element,
    parent_link: str,
    child_link: str,
    body: ET.Element,
    defaults: dict[str, dict[str, dict[str, str]]],
    default_class: str | None,
) -> tuple[int, str | None]:
    joint = body.find("joint")
    pos = parse_vec(body.get("pos"), 3, [0.0, 0.0, 0.0])
    quat = parse_vec(body.get("quat"), 4, [1.0, 0.0, 0.0, 0.0])

    if joint is None:
        urdf_joint = ET.SubElement(
            robot,
            "joint",
            {
                "name": f"{parent_link}_to_{child_link}",
                "type": "fixed",
            },
        )
        add_origin(urdf_joint, pos, quat)
        ET.SubElement(urdf_joint, "parent", {"link": parent_link})
        ET.SubElement(urdf_joint, "child", {"link": child_link})
        return 0, None

    attrs = attrs_for(defaults, joint, "joint", default_class)
    joint_name = attrs["name"]
    urdf_joint = ET.SubElement(robot, "joint", {"name": joint_name, "type": "revolute"})
    add_origin(urdf_joint, pos, quat)
    ET.SubElement(urdf_joint, "parent", {"link": parent_link})
    ET.SubElement(urdf_joint, "child", {"link": child_link})
    ET.SubElement(urdf_joint, "axis", {"xyz": attrs.get("axis", "1 0 0")})

    lower, upper = "-3.14159", "3.14159"
    if attrs.get("range"):
        lower, upper = attrs["range"].split()
    ET.SubElement(
        urdf_joint,
        "limit",
        {
            "lower": lower,
            "upper": upper,
            "effort": "10",
            "velocity": "10",
        },
    )
    return 1, joint_name


def convert(mjcf: Path, output: Path, mesh_base_url: str) -> None:
    source_root = ET.parse(mjcf).getroot()
    defaults = build_default_map(source_root)
    materials = material_map(source_root)
    material_names = material_name_by_id(source_root)
    output.parent.mkdir(parents=True, exist_ok=True)
    compiled_visuals = write_compiled_visuals(mjcf, output.parent / "assets", material_names)

    robot = ET.Element("robot", {"name": "shadow_hand_right"})
    for name, rgba in materials.items():
        material = ET.SubElement(robot, "material", {"name": name})
        ET.SubElement(material, "color", {"rgba": rgba})
    ET.SubElement(robot, "link", {"name": "world"})

    link_count = 1
    revolute_count = 0
    visual_count = 0
    joint_names: list[str] = []

    def visit_body(body: ET.Element, parent_link: str, inherited_class: str | None) -> None:
        nonlocal link_count, revolute_count, visual_count
        body_name = body.get("name")
        if not body_name:
            raise ValueError("body without name is not supported")
        default_class = body.get("childclass") or inherited_class

        urdf_link = ET.SubElement(robot, "link", {"name": body_name})
        link_count += 1
        visual_count += add_link_visuals(
            urdf_link,
            body_name,
            mesh_base_url,
            compiled_visuals,
        )

        added, joint_name = add_joint(robot, parent_link, body_name, body, defaults, default_class)
        revolute_count += added
        if joint_name:
            joint_names.append(joint_name)

        for child_body in body.findall("body"):
            visit_body(child_body, body_name, default_class)

    worldbody = source_root.find("worldbody")
    if worldbody is None:
        raise ValueError("MJCF has no worldbody")
    for body in worldbody.findall("body"):
        visit_body(body, "world", None)

    missing = [name for name in EXPECTED_JOINTS if name not in joint_names]
    extra = [name for name in joint_names if name not in EXPECTED_JOINTS]
    if missing or extra:
        raise ValueError(f"unexpected joint contract: missing={missing} extra={extra}")

    indent(robot)
    ET.ElementTree(robot).write(output, encoding="utf-8", xml_declaration=True)

    print(f"generated {output}")
    print(f"links={link_count} revolute_joints={revolute_count} visuals={visual_count}")
    print("joint_order=" + ",".join(joint_names))


def convert_baked(mjcf: Path, output: Path, mesh_base_url: str) -> None:
    source_root = ET.parse(mjcf).getroot()
    materials = material_map(source_root)
    material_names = material_name_by_id(source_root)
    output.parent.mkdir(parents=True, exist_ok=True)
    visuals = write_baked_visuals(mjcf, output.parent / "assets", material_names)

    robot = ET.Element("robot", {"name": "shadow_hand_right_baked"})
    for name, rgba in materials.items():
        material = ET.SubElement(robot, "material", {"name": name})
        ET.SubElement(material, "color", {"rgba": rgba})
    world_link = ET.SubElement(robot, "link", {"name": "world"})
    for compiled_visual in visuals:
        visual = ET.SubElement(world_link, "visual")
        add_origin(visual, [0.0, 0.0, 0.0], [1.0, 0.0, 0.0, 0.0])
        geometry = ET.SubElement(visual, "geometry")
        ET.SubElement(
            geometry,
            "mesh",
            {
                "filename": f"{mesh_base_url.rstrip('/')}/{compiled_visual.filename}",
                "scale": "1 1 1",
            },
        )
        ET.SubElement(visual, "material", {"name": compiled_visual.material})

    indent(robot)
    ET.ElementTree(robot).write(output, encoding="utf-8", xml_declaration=True)
    print(f"generated {output}")
    print(f"links=1 fixed_visuals={len(visuals)}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mjcf", type=Path, default=DEFAULT_MJCF)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--mesh-base-url", default=DEFAULT_MESH_URL)
    parser.add_argument("--mode", choices=["jointed", "baked"], default="jointed")
    args = parser.parse_args()

    try:
        if args.mode == "baked":
            output = args.output
            if output == DEFAULT_OUTPUT:
                output = output.with_name("shadow_hand_right_baked.urdf")
            convert_baked(args.mjcf.resolve(), output, args.mesh_base_url)
        else:
            convert(
                args.mjcf.resolve(),
                args.output,
                args.mesh_base_url,
            )
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
