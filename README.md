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
