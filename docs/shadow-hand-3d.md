# Shadow Hand 3D visualization with Foxglove

This document records the verified macOS arm64 workflow for visualizing the Hakoniwa Shadow Hand in Foxglove 3D.

The physics and visualization models intentionally have different responsibilities:

- **physics model:** MuJoCo Menagerie Shadow Hand
- **visualization model:** Shadow Robot `sr_hand.urdf.xacro`
- **joint motion:** Hakoniwa `sensor_msgs/msg/JointState`

No `/tf` publisher is required for this verified path. Foxglove's URDF layer computes link poses from the URDF and JointState data.

## Data path

```text
MuJoCo Shadow Hand
  -> Hakoniwa JointState PDU in callback SHM
  -> hakoniwa-pdu-bridge-core
  -> native Hakoniwa PDU over TCP
  -> Python JointState decode
  -> JointState CDR encode
  -> cdr_stdin_publisher
  -> FoxgloveComm
  -> Foxglove WebSocket
  -> Foxglove App
```

The bridge configuration lives under `config/shadow_hand_bridge/`.

The Foxglove JointState endpoint is `config/shadow_hand/endpoint_foxglove_jointstate.json` and publishes:

```text
/hakoniwa/ShadowHandAsset/joint_states
```

at:

```text
ws://127.0.0.1:8766
```

## Prepare the ROS 2 JointState schema

The Shadow Hand configuration references:

```text
work/schemas/ros2_jazzy/sensor_msgs/msg/JointState.bundle.msg
```

`work/` is intentionally gitignored. The schema bundle is a local staging artifact, not a source file owned by this repository.

With a sibling `hakoniwa-pdu-registry` checkout:

```bash
bash ../hakoniwa-pdu-registry/tools/generate_ros2msg_bundle.bash \
  sensor_msgs/JointState \
  -o ./work/schemas/ros2_jazzy/sensor_msgs/msg/JointState.bundle.msg
```

The registry tool prepares a self-contained Foxglove/MCAP `ros2msg` bundle from its ROS 2 Jazzy Docker environment. The host does not need a ROS 2 runtime or generated ROS 2 Python bindings for this step.

For custom message packages, the registry wrapper accepts repeatable `--search-path` arguments that point to host directories containing `<package>/msg/<Type>.msg` trees.

## Run the live path

The currently verified runtime launcher is macOS-oriented and uses local Python/shared-library paths.

Build the Endpoint shared library and Python binding used by the publisher process:

```bash
cd hakoniwa-pdu-endpoint
cmake -S . -B build-shared -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build build-shared --parallel
PATH=$HOME/.pyenv/shims:$PATH BUILD_DIR=build-shared bash build-python.bash
cd ..
```

Then run:

```bash
PYTHONPATH=../hakoniwa-mujoco-robots/thirdparty/hakoniwa-core-pro/launcher \
python3.12 -m hako_launch.hako_launcher \
  launch/shadow-hand-foxglove-smoke.launch.json
```

Expected evidence includes:

- `shadow_hand` reaches `WAIT START` and starts simulation
- `hakoniwa-pdu-web-bridge` initializes its SHM/TCP endpoints
- the Python publisher reports `sent JointState CDR`
- `cdr_stdin_publisher` reports published CDR frames

If Foxglove is not connected, a `No subscribers found` warning is expected.

## Prepare the visualization URDF

Keep upstream Shadow Robot inputs and generated visualization artifacts under `work/`; do not commit them.

Recommended layout:

```text
work/
├── schemas/
│   └── ros2_jazzy/
│       └── sensor_msgs/
│           └── msg/
│               └── JointState.bundle.msg
├── shadow_hand_source_urdf/
│   └── sr_common/
│       └── sr_description/
└── urdf/
    └── shadow_hand/
        ├── shadow_hand_right.urdf
        └── sr_description/
            └── meshes/
```

### 1. Fetch the Shadow Robot visualization source

```bash
mkdir -p work/shadow_hand_source_urdf
git clone \
  --depth 1 \
  --branch noetic-devel \
  https://github.com/shadow-robot/sr_common.git \
  work/shadow_hand_source_urdf/sr_common
```

### 2. Stage the mesh tree

```bash
mkdir -p work/urdf/shadow_hand
rm -rf work/urdf/shadow_hand/sr_description
cp -R \
  work/shadow_hand_source_urdf/sr_common/sr_description \
  work/urdf/shadow_hand/sr_description
```

### 3. Expand xacro without a ROS runtime on the host

Using a sibling `hakoniwa-mbody-registry` checkout:

```bash
python3 ../hakoniwa-mbody-registry/tools/xacro2urdf.py \
  work/shadow_hand_source_urdf/sr_common/sr_description/robots/sr_hand.urdf.xacro \
  -o work/urdf/shadow_hand/shadow_hand_right.urdf \
  --package sr_description=work/shadow_hand_source_urdf/sr_common/sr_description \
  --arg hand_version=E3M5 \
  --arg side=right \
  --arg fingers=all \
  --arg tip_sensors=pst
```

### 4. Prepare the URDF for Foxglove

```bash
python3 tools/prepare_urdf.py \
  work/urdf/shadow_hand/shadow_hand_right.urdf \
  --package sr_description \
  --root-joint rh_world_joint \
  --root-rpy "-1.5707963267948966 0 -1.5707963267948966" \
  --in-place
```

This rewrites selected `package://` mesh URIs to viewer-relative paths and applies only a visualization-side root orientation adjustment. It does not modify MuJoCo dynamics or the published JointState.

### 5. Serve the URDF and meshes

```bash
python3 tools/serve_static_cors.py \
  --directory work/urdf/shadow_hand \
  --port 8767
```

URDF URL:

```text
http://127.0.0.1:8767/shadow_hand_right.urdf
```

### 6. Configure Foxglove

Connect to:

```text
ws://127.0.0.1:8766
```

Then:

1. Add a **3D** panel.
2. Add a **URDF** custom layer.
3. Set the URDF URL to `http://127.0.0.1:8767/shadow_hand_right.urdf`.
4. Set URDF control mode to **Joint states**.
5. Set the joint-state topic to `/hakoniwa/ShadowHandAsset/joint_states`.
6. Enable **Ignore COLLADA `<up_axis>`** in the 3D scene settings.

The Shadow Robot URDF joint names match the observed JointState names, allowing Foxglove to animate the hand directly from `JointState.position[]`.

Troubleshooting:

- disconnected-looking mesh segments: check **Ignore COLLADA `<up_axis>`**
- whole model rotated relative to MuJoCo: check the `rh_world_joint` root RPY
- URDF visible but fingers do not move: confirm **Joint states** control mode and the exact JointState topic
- do not add `/tf` for this verified path

The earlier MJCF-to-URDF reverse-conversion experiment is not part of the recommended workflow. MuJoCo Menagerie remains the physics source of truth and the upstream Shadow Robot URDF remains the visualization source of truth.

The long-running browser launch config can start the static server together with the other processes after the URDF has been prepared:

```bash
PYTHONPATH=../hakoniwa-mujoco-robots/thirdparty/hakoniwa-core-pro/launcher \
python3.12 -m hako_launch.hako_launcher \
  launch/shadow-hand-foxglove-browser.launch.json
```
