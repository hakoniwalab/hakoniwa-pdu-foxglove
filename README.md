# hakoniwa-pdu-foxglove

[![Endpoint and Bridge Package Contract](https://github.com/hakoniwalab/hakoniwa-pdu-foxglove/actions/workflows/endpoint-package-contract.yml/badge.svg)](https://github.com/hakoniwalab/hakoniwa-pdu-foxglove/actions/workflows/endpoint-package-contract.yml)
[![Validate Windows x64](https://github.com/hakoniwalab/hakoniwa-pdu-foxglove/actions/workflows/windows-x64.yml/badge.svg)](https://github.com/hakoniwalab/hakoniwa-pdu-foxglove/actions/workflows/windows-x64.yml)

`hakoniwa-pdu-foxglove` is an output adapter that connects
[`hakoniwa-pdu-endpoint`](https://github.com/hakoniwalab/hakoniwa-pdu-endpoint)
to Foxglove live visualization.

Version 0.1 publishes **caller-provided, Foxglove-compatible CDR payloads**
through the official Foxglove SDK WebSocket server. The adapter forwards the
CDR bytes without deserializing or re-serializing them.

> Status: version 0.1, output-only.

## Verified scope

Build/package integration and runtime E2E are intentionally tracked separately.

| Scope | Ubuntu x64 | Linux ARM64 | macOS arm64 | Windows x64 |
|---|---:|---:|---:|---:|
| Foxglove against installed Core-free Endpoint | verified | verified | verified | verified |
| Core + Endpoint `core_callback` + Bridge web app build | verified | verified | verified | verified |
| Foxglove automated tests | verified | verified | verified | verified |
| Windows publisher smoke | — | — | — | verified |
| Full Shadow Hand → Bridge → Foxglove runtime E2E | not yet verified | not yet verified | verified | not yet verified |
| Shadow Hand URDF + JointState 3D visualization | not yet verified | not yet verified | verified | not yet verified |

The cross-platform CI verifies the repository's **build and package contract**.
It does not claim that the current Shadow Hand launcher is portable to all four
platforms. The current live Shadow Hand launcher still contains macOS-oriented
Python/shared-library paths.

## Goal

Provide a small, reusable component that connects Hakoniwa's binary-oriented
Endpoint abstraction to Foxglove while keeping Hakoniwa Core, transport bridging,
and type conversion outside the Foxglove adapter itself.

Version 0.1:

- implements an output-only `FoxgloveComm` derived from `hakoniwa::pdu::PduComm`;
- uses the official Foxglove C++ SDK `WebSocketServer` and `RawChannel` APIs;
- publishes CDR payloads with explicitly configured schemas;
- preserves the exact payload bytes supplied by the caller;
- uses `hakoniwa-pdu-registry` as the source of PDU types, CDR codecs, and schema information;
- is injected with `Endpoint::set_comm()` rather than being registered in the Endpoint protocol factory.

## Repository integration architecture

Foxglove itself consumes the **Core-free base Endpoint target**:

```text
Application / codec layer
        ↓ complete CDR payload
hakoniwa_pdu_endpoint::hakoniwa_pdu_endpoint
        ↓
FoxgloveComm
        ↓
Foxglove SDK / WebSocket
        ↓
Foxglove App
```

The repository's Shadow Hand integration uses Bridge on the Hakoniwa side:

```text
MuJoCo Shadow Hand
  ↓ callback SHM
Hakoniwa Core Pro
  ↓
hakoniwa_pdu_endpoint::core_callback
  ↓
hakoniwa-pdu-web-bridge
  ↓ native Hakoniwa PDU over TCP
Python decode / CDR encode
  ↓
cdr_stdin_publisher
  ↓
FoxgloveComm
  ↓
Foxglove WebSocket
```

This split is deliberate:

- **Foxglove adapter:** Core-free Endpoint consumer and Foxglove transport.
- **Bridge:** callback/SHM-facing Hakoniwa integration and TCP process boundary.
- **Registry / codec layer:** native PDU decoding, CDR encoding, and schema knowledge.

## Important data contract

`PduComm` is a binary transport boundary. It does not guarantee that bytes
passed to `send()` are CDR.

```text
FoxgloveComm::send(...) input = complete Foxglove-compatible CDR payload
```

The caller, typed wrapper, or bridge/codec layer is responsible for converting a
Hakoniwa typed/native PDU into CDR before calling the Endpoint. Generated CDR
converters from `hakoniwa-pdu-registry` should be used for that conversion.

`FoxgloveComm` does not guess the input representation and does not silently
convert Hakoniwa native PDU binary into CDR.

## Dependencies

Required for the core adapter:

- C++20 compiler
- CMake 3.20 or later
- installed `hakoniwa-pdu-endpoint` CMake package, preferred
- official Foxglove C++ SDK, pinned by version and SHA256 unless a system SDK is requested

Optional/development dependencies:

- bundled `hakoniwa-pdu-endpoint` submodule as a compatibility/development fallback
- `hakoniwa-pdu-registry` for bundled examples, PDU/CDR converters, and schema generation
- Fast-CDR for examples
- GoogleTest for tests
- `hakoniwa-pdu-bridge-core` for the verified Shadow Hand integration path

## Endpoint package contract

The preferred build consumes the installed Endpoint package target:

```cmake
find_package(hakoniwa_pdu_endpoint CONFIG REQUIRED)

target_link_libraries(hakoniwa_pdu_foxglove
  PUBLIC
    hakoniwa_pdu_endpoint::hakoniwa_pdu_endpoint
)
```

The Foxglove adapter intentionally uses the Core-free base target.

For repository development, the bundled Endpoint submodule remains available as
a fallback. `HAKO_PDU_FOXGLOVE_USE_BUNDLED_ENDPOINT` can be used to select that
path explicitly.

## Build and test

For the existing repository-development flow, initialize submodules:

```bash
git submodule update --init --recursive
```

Build:

```bash
./build.bash
```

Run tests:

```bash
./test.bash
```

The build can produce:

- `hakoniwa_pdu_foxglove`
- `cdr_publisher_example`
- `cdr_stdin_publisher`
- `hakoniwa_pdu_foxglove_tests`

## Cross-platform package/integration CI

`.github/workflows/endpoint-package-contract.yml` validates, on Ubuntu x64,
native Linux ARM64, macOS arm64, and Windows x64:

1. build/install Core-free Endpoint;
2. build/test Foxglove against the installed Endpoint package;
3. build/install Hakoniwa Core Pro;
4. build/install the Core-enabled Endpoint package, including base, callback, and polling variants;
5. build `hakoniwa-pdu-web-bridge` against `core_callback`.

This is the same dependency boundary used by the repository's Shadow Hand
integration, without pretending the full launcher/runtime recipe is already
portable across all platforms.

`.github/workflows/windows-x64.yml` is intentionally kept separately because it
also builds the examples and runs the Windows-specific publisher smoke test.

Both workflows cancel superseded runs for the same PR/ref, and documentation-only
changes do not trigger the heavy build workflows.

## Foxglove SDK pin

The CMake integration downloads an official Foxglove C++ SDK release archive
and verifies it with SHA256.

Pinned release:

```text
sdk/v0.25.2
```

Supported prebuilt targets in the current integration:

- macOS arm64
- macOS x86_64
- Linux aarch64
- Linux x86_64
- Windows x86_64 / MSVC

Set:

```text
HAKO_PDU_FOXGLOVE_USE_SYSTEM_FOXGLOVE_SDK=ON
```

to use an already installed `foxglove-sdk` CMake package.

## Basic `SimTime` CDR publisher

The smallest smoke example publishes `hako_msgs/SimTime`.

- PDU definition: `config/sample/pdudef.json`
- Foxglove communication config: `config/sample/comm_foxglove.json`
- registry schema: `hakoniwa-pdu-registry/idl/hako_msgs/msg/SimTime.msg`
- schema encoding: `ros2msg`
- topic: `/hakoniwa/FoxgloveDemo/sim_time`

Run:

```bash
./build/cdr_publisher_example config/sample/endpoint_foxglove.json
```

For a short smoke:

```bash
./build/cdr_publisher_example config/sample/endpoint_foxglove.json 3
```

Connect Foxglove to:

```text
ws://127.0.0.1:8765
```

Then confirm `/hakoniwa/FoxgloveDemo/sim_time` in Raw Messages or Plot.

## Generic stdin CDR publisher

`cdr_stdin_publisher` is a type-independent process boundary. A type-specific
producer creates a complete CDR payload and writes it to stdin; the C++ process
publishes it through a `FoxgloveComm`-backed Endpoint.

Default framing:

```text
<u32 payload_size little-endian><CDR payload>
```

A multiplexed mode is also supported for configurations with multiple channels.
See `examples/cdr-stdin-publisher/README.md` for the framing contract.

## Shadow Hand Foxglove workflow

The verified Shadow Hand path reuses existing Hakoniwa components rather than
putting Hakoniwa shared-memory and type-conversion responsibility into the
Foxglove adapter.

### Build the local demo binaries

The Shadow Hand launcher intentionally runs repository-local application
binaries. This prevents an older Bridge, Endpoint, Foxglove publisher, or MuJoCo
asset installed elsewhere on the machine from being mixed into the demo.

Endpoint and Bridge use their manifest-driven build entrypoints. The manifests
describe platform-neutral capability intent; `tools/hako.py` resolves compiler,
architecture, shared-library, dependency, and CMake details for the current
host. Raw CMake commands are not the primary Endpoint/Bridge build contract.

The integration-owned manifests are:

```text
config/manifests/endpoint-core-free.yaml
config/manifests/endpoint-core-callback.yaml
config/manifests/bridge-shadow-hand.yaml
```

Hakoniwa Core keeps its normal installed prefix. It defaults to
`/usr/local/hakoniwa` and can be changed with `HAKONIWA_CORE_ROOT`. Two Endpoint
packages built specifically for this demo are installed under the ignored local
`work/install` directory so the Core-free and callback contracts cannot be
mixed through one CMake cache.

From the `hakoniwa-pdu-foxglove` repository root, initialize the submodules and
define the build prefixes:

```bash
git submodule update --init --recursive

export WORKSPACE_DIR="$(cd .. && pwd -P)"
export HAKONIWA_CORE_ROOT="${HAKONIWA_CORE_ROOT:-/usr/local/hakoniwa}"
export HAKO_PYTHON="${HAKO_PYTHON:-python3.12}"
export HAKONIWA_ENDPOINT_CORE_FREE_PREFIX="${HAKONIWA_ENDPOINT_CORE_FREE_PREFIX:-$PWD/work/install/endpoint-core-free}"
export HAKONIWA_ENDPOINT_CORE_PREFIX="${HAKONIWA_ENDPOINT_CORE_PREFIX:-$PWD/work/install/endpoint-core}"
```

Python 3.12 is the common Hakoniwa Core/runtime prerequisite defined by the
Hakoniwa Runtime Primer; this demo only selects that interpreter consistently
for manifest resolution and Python bindings.

When the sibling Business Pack repository is available, run the common runtime
preflight before building:

```bash
(
  cd "$WORKSPACE_DIR/hakoniwa-business-pack"
  bash tools/doctor.bash
)
```

Resolve, build, test, and locally install the Core-free Endpoint package
consumed by Foxglove:

```bash
"$HAKO_PYTHON" hakoniwa-pdu-endpoint/tools/hako.py doctor \
  --config config/manifests/endpoint-core-free.yaml
"$HAKO_PYTHON" hakoniwa-pdu-endpoint/tools/hako.py configure --dry-run \
  --config config/manifests/endpoint-core-free.yaml
"$HAKO_PYTHON" hakoniwa-pdu-endpoint/tools/hako.py build \
  --config config/manifests/endpoint-core-free.yaml
"$HAKO_PYTHON" hakoniwa-pdu-endpoint/tools/hako.py test \
  --config config/manifests/endpoint-core-free.yaml
cmake --install work/build/manifest/endpoint-core-free --config Release \
  --prefix "$HAKONIWA_ENDPOINT_CORE_FREE_PREFIX"
```

Build and locally install the separate Core-enabled Endpoint package used by
Bridge and the Python Shadow Hand sender. `BUILD_SHARED_LIBS=ON` is required
because the sender loads the explicit `core_callback` library at runtime; the
manifest expresses this as `build.shared: true` and `bindings.python: true`.

```bash
HAKONIWA_CORE_ROOT="$HAKONIWA_CORE_ROOT" \
"$HAKO_PYTHON" hakoniwa-pdu-endpoint/tools/hako.py doctor \
  --config config/manifests/endpoint-core-callback.yaml
HAKONIWA_CORE_ROOT="$HAKONIWA_CORE_ROOT" \
"$HAKO_PYTHON" hakoniwa-pdu-endpoint/tools/hako.py configure --dry-run \
  --config config/manifests/endpoint-core-callback.yaml
HAKONIWA_CORE_ROOT="$HAKONIWA_CORE_ROOT" \
"$HAKO_PYTHON" hakoniwa-pdu-endpoint/tools/hako.py build \
  --config config/manifests/endpoint-core-callback.yaml
HAKONIWA_CORE_ROOT="$HAKONIWA_CORE_ROOT" \
"$HAKO_PYTHON" hakoniwa-pdu-endpoint/tools/hako.py test \
  --config config/manifests/endpoint-core-callback.yaml
cmake --install work/build/manifest/endpoint-core-callback --config Release \
  --prefix "$HAKONIWA_ENDPOINT_CORE_PREFIX"
```

Each manifest run writes the resolved host configuration and generated CMake
arguments under `hakoniwa-pdu-endpoint/.hako/`. Preserve those files when
reporting a platform-specific build problem.

Build Foxglove against the installed Core-free base Endpoint target:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HAKONIWA_ENDPOINT_CORE_FREE_PREFIX" \
  -DHAKO_PDU_FOXGLOVE_BUILD_EXAMPLES=ON \
  -DHAKO_PDU_FOXGLOVE_BUILD_TESTS=ON \
  -DHAKO_PDU_FOXGLOVE_USE_BUNDLED_ENDPOINT=OFF
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

Resolve and build the sibling Bridge web application against the installed
`core_callback` Endpoint target:

```bash
(
  cd "$WORKSPACE_DIR/hakoniwa-pdu-bridge-core"
  export HAKO_PDU_ENDPOINT_ROOT="$HAKONIWA_ENDPOINT_CORE_PREFIX"
  export HAKONIWA_CORE_ROOT
  "$HAKO_PYTHON" tools/hako.py doctor \
    --config ../hakoniwa-pdu-foxglove/config/manifests/bridge-shadow-hand.yaml
  "$HAKO_PYTHON" tools/hako.py configure --dry-run \
    --config ../hakoniwa-pdu-foxglove/config/manifests/bridge-shadow-hand.yaml
  "$HAKO_PYTHON" tools/hako.py build \
    --config ../hakoniwa-pdu-foxglove/config/manifests/bridge-shadow-hand.yaml
  "$HAKO_PYTHON" tools/hako.py test \
    --config ../hakoniwa-pdu-foxglove/config/manifests/bridge-shadow-hand.yaml
)
```

The resolved Bridge configuration is recorded under
`hakoniwa-pdu-bridge-core/.hako/`.

Build the local Shadow Hand MuJoCo asset against the same Core and Endpoint
prefixes. `hakoniwa-mujoco-robots` does not currently provide the same
`hakoniwa-build.yaml` interface, so this step uses that repository's supported
build wrapper. If its existing build directory was configured with different
Core or Endpoint prefixes, run `bash build.bash clean` once before rebuilding;
CMake package locations are cached.

```bash
(
  cd "$WORKSPACE_DIR/hakoniwa-mujoco-robots"
  HAKONIWA_CORE_ROOT="$HAKONIWA_CORE_ROOT" \
  HAKONIWA_PDU_ENDPOINT_ROOT="$HAKONIWA_ENDPOINT_CORE_PREFIX" \
  bash build.bash
)
```

When testing a local Core build beside the system installation with the mmap
backend, also set `HAKO_CONFIG_PATH` to a Core config whose `core_mmap_path`
points to an isolated local directory. Do not reuse mmap files created by a
different Core build.

The resulting launcher contract is:

| Runtime role | Local artifact | Endpoint linkage |
|---|---|---|
| MuJoCo Shadow Hand | `hakoniwa-mujoco-robots/src/cmake-build/.../shadow-hand-hakoniwa-asset` | local Endpoint package |
| Foxglove publisher | `hakoniwa-pdu-foxglove/build/cdr_stdin_publisher` | Core-free base target |
| SHM-to-TCP Bridge | `hakoniwa-pdu-bridge-core/build-shadow-hand/hakoniwa-pdu-web-bridge` | `core_callback` |
| Python sender | local Endpoint Python source/FFI plus `work/install/endpoint-core/lib/libhakoniwa_pdu_endpoint_core_callback.*` | `core_callback` |

Use separate manifests and build directories when changing Endpoint variants.
Reconfiguring one build directory between Core-free and Core-enabled modes can
retain an old `find_library()` result in the CMake cache.

`tools/launch.bash` checks these local artifacts before starting any process.
Set `HAKONIWA_ENDPOINT_CORE_FREE_PREFIX` and
`HAKONIWA_ENDPOINT_CORE_PREFIX` when using different local install directories.
`HAKONIWA_DEMO_PREFIX` remains a compatibility override for the callback
prefix. Set `HAKO_PDU_ENDPOINT_SHARED_LIB` to select an explicit callback shared
library directly. The launcher also selects the manifest-built local Python FFI
through `HAKO_PDU_ENDPOINT_PYTHON_BUILD_DIR`, so an older globally installed
Endpoint Python package cannot silently replace the demo build. Both the Core
and local Endpoint library directories are propagated to launcher-managed
processes.

### Launch

After preparing the local JointState schema and Shadow Hand visualization URDF
described in the detailed guide, start the current browser-oriented composition
from the repository root:

```bash
bash tools/launch.bash launch/shadow-hand-foxglove-browser.launch.json
```

Then open:

```text
https://app.foxglove.dev
```

and connect Foxglove to:

```text
ws://localhost:8766
```

`tools/launch.bash` configures the sibling Hakoniwa repository paths, verifies
the locally built demo artifacts, selects the locally installed
`core_callback` Endpoint shared library for the Python sender, and starts the
launch file through the launcher provided by the installed `hakoniwa-pdu`
Python package. Following the common Hakoniwa runtime contract, the wrapper
uses Python 3.12 and verifies that the launcher is importable before starting
any asset. Set `HAKO_PYTHON` or `PYTHON_CMD` only when the prepared Python 3.12
interpreter is not available as `python3.12`.

The browser launch composition starts the Shadow Hand plant, the local URDF
server, the JointState-to-CDR publisher, the SHM-to-TCP bridge, and the bounded
hand-motion sender. The sender is configured for 120 seconds. Stop the
foreground launcher with Ctrl-C; it terminates the assets it started. Confirm
that the listeners on ports 8766 and 8767 have closed before starting another
run.

The Shadow-Hand-specific bridge configuration lives under:

```text
config/shadow_hand_bridge/
```

The Foxglove JointState endpoint is:

```text
config/shadow_hand/endpoint_foxglove_jointstate.json
```

and publishes:

```text
/hakoniwa/ShadowHandAsset/joint_states
```

at:

```text
ws://127.0.0.1:8766
```

The full currently verified macOS arm64 runtime and 3D visualization procedure,
including schema preparation and Shadow Robot URDF staging, is documented in
[`docs/shadow-hand-3d.md`](docs/shadow-hand-3d.md).

## Docker core smoke

The Docker setup validates the core adapter on Ubuntu 24.04. It builds the
project, runs CTest, and starts the basic `SimTime` CDR publisher.

**This Docker smoke does not validate the full Shadow Hand workflow.**

```bash
git submodule update --init --recursive
docker compose -f docker/docker-compose.yml build
docker compose -f docker/docker-compose.yml run --rm --no-deps hakoniwa-pdu-foxglove \
  ctest --test-dir build --output-on-failure
docker compose -f docker/docker-compose.yml up --force-recreate hakoniwa-pdu-foxglove
```

Connect the host Foxglove application to:

```text
ws://localhost:8765
```

The Docker topic is `/hakoniwa/FoxgloveDocker/sim_time`.

## Verification strategy

Automated tests cover or should cover:

- valid and invalid configuration parsing
- relative schema-path resolution
- PDU name-to-channel resolution
- duplicate mapping rejection
- lifecycle idempotency
- `send()` before `start()`
- unknown PDU keys
- unsupported receive operations
- exact forwarding of input bytes
- clean shutdown after partial startup failure

Manual live verification checks that Foxglove sees the configured topic and
schema and can decode the corresponding CDR fields.

## Roadmap

### Version 0.1 — Raw CDR live publication

Current version. Explicit schema plus untouched caller-provided CDR through a
Foxglove WebSocket server.

### Version 0.2 — Hakoniwa time integration

Evaluate exposing Foxglove time capability using Hakoniwa simulation time.

### Version 0.3 — Visualization mappings

Evaluate opt-in converters for selected PDU types such as pose, transforms,
images, and point clouds while keeping the raw CDR path available.

### Version 0.4 — Bidirectional control, only when justified

Evaluate Foxglove client-publish support for command PDUs.

## References

- [Foxglove SDK](https://docs.foxglove.dev/docs/sdk)
- [Foxglove WebSocket server](https://docs.foxglove.dev/docs/sdk/websocket-server)
- [Foxglove custom schema encodings](https://docs.foxglove.dev/docs/getting-started/custom/custom-schema-encodings)
- [foxglove/foxglove-sdk](https://github.com/foxglove/foxglove-sdk)
- [hakoniwa-pdu-registry](https://github.com/hakoniwalab/hakoniwa-pdu-registry)
- [hakoniwa-pdu-endpoint](https://github.com/hakoniwalab/hakoniwa-pdu-endpoint)
- [hakoniwa-pdu-bridge-core](https://github.com/hakoniwalab/hakoniwa-pdu-bridge-core)

## License

MIT. See [LICENSE](LICENSE).
