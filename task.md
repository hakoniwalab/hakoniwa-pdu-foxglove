# Initial Implementation Task

Implement version 0.1 of `hakoniwa-pdu-foxglove` as defined in
[`README.md`](README.md).

This task is intended to be executed by Codex. Treat it as an implementation
contract, not as a request for another design proposal.

## Objective

Build and validate the smallest useful Foxglove integration:

```text
caller-provided complete CDR payload
  -> hakoniwa-pdu-endpoint Endpoint OUT
  -> injected FoxgloveComm
  -> FoxglovePublisher
  -> official Foxglove SDK RawChannel
  -> Foxglove WebSocket server
  -> Foxglove app
```

The raw CDR payload must reach the Foxglove SDK without deserialization,
field-wise conversion, or re-serialization inside this repository.

## Read before changing code

Inspect these files and repositories before implementation:

1. `README.md` in this repository.
2. `hakoniwa-pdu-endpoint/include/hakoniwa/pdu/comm/comm.hpp`.
3. `hakoniwa-pdu-endpoint/include/hakoniwa/pdu/endpoint.hpp`.
4. `hakoniwa-pdu-endpoint/include/hakoniwa/pdu/pdu_definition.hpp`.
5. `hakoniwa-pdu-endpoint/docs/rmw_zenoh_integration.md`, especially the
   binary-boundary and CDR-responsibility sections.
6. `hakoniwa-pdu-registry` documentation and generated CDR converters.
7. The current official Foxglove C++ SDK documentation and examples for:
   - `foxglove::Context`
   - `foxglove::WebSocketServer`
   - `foxglove::RawChannel`
   - `foxglove::Schema`

Do not copy obsolete examples from the deprecated standalone Foxglove
WebSocket libraries. Use the current official `foxglove/foxglove-sdk` APIs.

## Non-negotiable design rules

- Implement `FoxgloveComm` as a subclass of `hakoniwa::pdu::PduComm`.
- Keep an Endpoint-independent `FoxglovePublisher` layer that owns Foxglove SDK
  objects.
- Use `Endpoint::set_comm()` for the first implementation.
- Do not add `foxglove` to `hakoniwa-pdu-endpoint/src/pdu_factory.cpp` in this
  task.
- Do not modify files inside either Git submodule.
- Do not deserialize or re-serialize CDR in `FoxgloveComm` or
  `FoxglovePublisher`.
- Do not accept Hakoniwa native/raw PDU bytes while advertising them as CDR.
- Do not implement Foxglove native message conversion in version 0.1.
- Do not implement client publish, services, parameters, assets, graph support,
  MCAP, or simulation-time broadcasting.
- Do not silently infer or switch schema encodings.
- Keep all build dependencies pinned or explicitly configurable. Do not depend
  on an unpinned moving Git branch.

## Phase 0: verify inputs and record decisions

Before writing the main implementation, inspect the submodules and record the
following concrete decisions in the final README update:

- the Foxglove SDK release version used;
- how the SDK archive is selected for macOS and Linux;
- the archive checksum or other reproducible integrity mechanism;
- the initial smoke-test PDU type;
- the exact registry-managed schema artifact used by the smoke test;
- the explicit Foxglove schema encoding for that artifact;
- the generated registry CDR converter used to create the test payload.

For the initial sample, prefer a small scalar message with a deterministic CDR
payload and a numeric field that Foxglove Plot can display. Use a more complex
message only if a small registry-backed type is unavailable.

If the registry provides a `.msg` schema rather than OMG IDL for the chosen
message, configure `ros2msg` explicitly. If it provides an actual compatible
`.idl` artifact, configure `omgidl` or `ros2idl` explicitly as appropriate.
Do not manufacture an OMG IDL schema from assumptions.

## Phase 1: repository bootstrap

- [ ] Add `hakoniwa-pdu-registry` as a Git submodule at repository root.
- [ ] Confirm `hakoniwa-pdu-endpoint` remains a root-level Git submodule.
- [ ] Add a top-level CMake project using C++20.
- [ ] Add a reproducible Foxglove SDK integration under `cmake/`.
- [ ] Add `build.bash` with `set -euo pipefail`.
- [ ] Add a test command or `test.bash` that runs CTest with
      `--output-on-failure`.
- [ ] Add a reasonable `.gitignore` for CMake and local SDK artifacts.
- [ ] Avoid hard-coded Homebrew, user-home, or machine-specific absolute paths.

The CMake setup should build at least:

- a reusable `hakoniwa_pdu_foxglove` library;
- a CDR publisher example;
- unit tests when testing is enabled.

Prefer the Foxglove SDK release archive and its supplied CMake package config,
following the official SDK installation guidance. Do not make a Rust toolchain
a normal build prerequisite for this repository.

## Phase 2: configuration model

Create a typed configuration model and parser for the Foxglove communication
configuration.

Minimum server fields:

```text
protocol
server.name
server.host
server.port
```

Minimum per-channel fields:

```text
pdu_key.robot
pdu_key.pdu
topic
schema.name
schema.encoding
schema.file
```

Requirements:

- [ ] Resolve `schema.file` relative to the communication config file.
- [ ] Require `protocol` to be `foxglove`.
- [ ] Validate port range.
- [ ] Reject an empty server name, host, topic, schema name, or schema file.
- [ ] Accept only explicitly supported schema encodings.
- [ ] Fix the message encoding to `cdr` for version 0.1.
- [ ] Reject duplicate `(robot, pdu)` mappings.
- [ ] Reject duplicate Foxglove topics.
- [ ] Reject missing schema files.
- [ ] Reject a missing `PduDefinition`.
- [ ] Resolve every `(robot, pdu)` through `PduDefinition` during `open()`.
- [ ] Store the resolved `(robot, channel_id)` key used by `send()`.
- [ ] Preserve loaded schema storage for the channel lifetime.
- [ ] Return an appropriate existing `HakoPduErrorType`; do not add new error
      values to the Endpoint submodule.

Add a JSON Schema for the communication config if it can be done without
unnecessary complexity. At minimum, provide a validated sample configuration.

## Phase 3: FoxglovePublisher

Implement an Endpoint-independent publisher class responsible for the official
Foxglove SDK objects.

Suggested responsibilities:

```cpp
class FoxglovePublisher {
public:
    HakoPduErrorType configure(const FoxgloveConfig& config);
    HakoPduErrorType start();
    HakoPduErrorType publish(
        const hakoniwa::pdu::PduResolvedKey& key,
        std::span<const std::byte> cdr_payload) noexcept;
    HakoPduErrorType stop() noexcept;
    HakoPduErrorType close() noexcept;
    bool is_running() const noexcept;
};
```

The exact public API may differ, but preserve these responsibilities and keep
it independent from `Endpoint`.

Implementation requirements:

- [ ] Create a dedicated `foxglove::Context`.
- [ ] Configure the WebSocket server with that same context.
- [ ] Create one `foxglove::RawChannel` per validated channel mapping using that
      context.
- [ ] Use message encoding `cdr`.
- [ ] Load `foxglove::Schema` from the configured schema name, encoding, and
      file contents.
- [ ] Use nanoseconds since Unix epoch for the initial wall-clock log time.
- [ ] Pass the exact input pointer and size to `RawChannel::log()`.
- [ ] Do not copy the payload merely to reinterpret it.
- [ ] Map Foxglove SDK failures to existing Hakoniwa error values and log a
      useful diagnostic.
- [ ] Make `stop()` and `close()` idempotent.
- [ ] Clean up correctly when startup fails after only some channels were
      created.
- [ ] Do not expose partially initialized running state.

The Foxglove SDK documents `RawChannel` logging as thread-safe. Still protect
this repository's lifecycle state and key-to-channel map against races between
`send()`, `stop()`, and `close()`.

## Phase 4: FoxgloveComm

Implement:

```cpp
class FoxgloveComm final : public hakoniwa::pdu::PduComm
```

Required behavior:

- [ ] `open(config_path)` parses and validates all configuration without
      opening the network server.
- [ ] `start()` starts the publisher and enters running state.
- [ ] `send(key, data)` returns `HAKO_PDU_ERR_NOT_RUNNING` before `start()`.
- [ ] `send(key, data)` returns `HAKO_PDU_ERR_INVALID_PDU_KEY` for an
      unconfigured resolved key.
- [ ] `send(key, data)` forwards the byte sequence unchanged.
- [ ] `recv(...)` returns `HAKO_PDU_ERR_UNSUPPORTED`.
- [ ] `recv_next(...)` returns `HAKO_PDU_ERR_UNSUPPORTED`.
- [ ] `set_recv_event(...)` should not pretend receive support exists; use the
      most appropriate existing unsupported behavior.
- [ ] `stop()` and `close()` are safe when called repeatedly.
- [ ] `is_running()` accurately reports state.
- [ ] Exceptions must not escape from methods declared `noexcept`.

Do not register `FoxgloveComm` in the Endpoint factory yet. The example must
construct it and inject it using `Endpoint::set_comm()` before opening the
Endpoint.

## Phase 5: CDR publisher example

Add a minimal example under `examples/cdr-publisher/`.

The example must:

- [ ] load an Endpoint configuration and Foxglove communication configuration;
- [ ] instantiate and inject `FoxgloveComm`;
- [ ] use a generated `hakoniwa-pdu-registry` CDR converter to produce a
      complete CDR payload, including the expected CDR encapsulation;
- [ ] publish a changing value at a modest rate;
- [ ] shut down cleanly on SIGINT;
- [ ] print the WebSocket connection address and published topic;
- [ ] avoid requiring ROS 2 runtime initialization when the generated converter
      itself does not require it.

Provide sample configuration files with paths that work from a documented
working directory. Do not leave placeholder schema paths in the final sample.

## Phase 6: tests

Use GoogleTest or the testing convention already established by the Endpoint
submodule.

Automated tests must not require the Foxglove desktop or web application.

Required coverage:

- [ ] valid configuration;
- [ ] malformed JSON;
- [ ] invalid protocol;
- [ ] invalid port;
- [ ] missing schema file;
- [ ] unsupported schema encoding;
- [ ] unresolved PDU name;
- [ ] duplicate PDU mapping;
- [ ] duplicate topic;
- [ ] relative schema path resolution;
- [ ] `send()` before `start()`;
- [ ] unknown resolved key;
- [ ] unsupported receive methods;
- [ ] repeated `stop()` and `close()`;
- [ ] exact byte-for-byte payload forwarding through a mock or test publisher
      boundary;
- [ ] cleanup after simulated partial startup failure.

Add one SDK-backed integration or smoke test only if it is stable in CI and can
bind an ephemeral port. Do not make tests depend on port `8765` being free.

## Phase 7: documentation completion

After implementation:

- [ ] Update `README.md` with actual build commands.
- [ ] Replace the sample schema placeholder with the verified registry path.
- [ ] Document the selected Foxglove SDK version and pinning mechanism.
- [ ] Document how to initialize submodules.
- [ ] Document how to run tests.
- [ ] Document how to run the CDR publisher example.
- [ ] Document the Foxglove connection steps.
- [ ] State clearly that the caller must provide CDR, not native Hakoniwa PDU
      bytes.
- [ ] Record current platform verification results for macOS and/or Ubuntu.
- [ ] Mark completed checkboxes in this file or add an implementation result
      section with commands and outcomes.

## Acceptance criteria

The task is complete only when all of the following are true:

1. `git submodule update --init --recursive` initializes both required
   submodules.
2. A clean CMake configure and build succeeds on the current development
   platform.
3. CTest passes with `--output-on-failure`.
4. The example produces CDR using a registry-generated converter.
5. The example publishes through an injected `FoxgloveComm`.
6. Foxglove can connect to the configured WebSocket address.
7. The configured topic appears with the expected schema.
8. Raw Messages shows decoded fields from the CDR payload.
9. A numeric field can be plotted when the selected sample type supports it.
10. The bytes supplied to `send()` are not transformed inside this repository.
11. No files inside `hakoniwa-pdu-endpoint` or `hakoniwa-pdu-registry` are
    modified.
12. The README contains reproducible commands and accurately states remaining
    limitations.

## Out-of-scope follow-up ideas

Do not implement these as part of this task:

- Endpoint factory registration for `protocol: foxglove`;
- Hakoniwa simulation-time broadcasting through Foxglove `Time` capability;
- automatic registry schema discovery;
- Foxglove native pose, transform, image, or point-cloud conversion;
- bidirectional command publishing;
- MCAP recording;
- packaged installation or apt distribution.

Finish version 0.1 first and leave these as documented follow-up work.

## Implementation Result

Implemented version 0.1 in this repository.

Completed:

- [x] Added `hakoniwa-pdu-registry` as a root-level Git submodule.
- [x] Confirmed `hakoniwa-pdu-endpoint` remains a root-level Git submodule.
- [x] Added top-level CMake project using C++20.
- [x] Added reproducible Foxglove SDK integration in `cmake/FoxgloveSdk.cmake`.
- [x] Added `build.bash` and `test.bash` with `set -euo pipefail`.
- [x] Added `.gitignore` for CMake and local SDK artifacts.
- [x] Implemented typed Foxglove config parsing and validation.
- [x] Implemented endpoint-independent `FoxglovePublisher`.
- [x] Implemented `FoxgloveComm final : hakoniwa::pdu::PduComm`.
- [x] Added CDR publisher example using `Endpoint::set_comm()`.
- [x] Added sample Endpoint and Foxglove communication configs.
- [x] Added Docker-based live smoke environment for Ubuntu container execution.
- [x] Added automated tests for config validation, lifecycle, unsupported receive
      APIs, unknown keys, repeated stop/close, start failure, and byte-for-byte
      forwarding through a mock publisher boundary.
- [x] Updated `README.md` with build, test, example, SDK pinning, registry
      artifact, Docker live smoke, and Foxglove connection steps.

Recorded implementation decisions:

- Foxglove SDK release: `sdk/v0.25.2`.
- SDK archive selection:
  - macOS arm64: `foxglove-v0.25.2-cpp-aarch64-apple-darwin.zip`
  - macOS x86_64: `foxglove-v0.25.2-cpp-x86_64-apple-darwin.zip`
  - Linux aarch64: `foxglove-v0.25.2-cpp-aarch64-unknown-linux-gnu.zip`
  - Linux x86_64: `foxglove-v0.25.2-cpp-x86_64-unknown-linux-gnu.zip`
- Integrity mechanism: CMake `URL_HASH SHA256=...` for each selected SDK
  archive.
- Initial smoke-test PDU type: `hako_msgs/SimTime`.
- Registry-managed schema artifact:
  `hakoniwa-pdu-registry/idl/hako_msgs/msg/SimTime.msg`.
- Explicit Foxglove schema encoding: `ros2msg`.
- Generated registry CDR converter:
  `hakoniwa-pdu-registry/pdu/types/hako_msgs/pdu_cpptype_cdr_conv_SimTime.hpp`.

Verification performed:

```text
./test.bash
100% tests passed, 0 tests failed out of 1
```

CDR publisher smoke:

```text
./build/cdr_publisher_example /private/tmp/hako-foxglove-sample/endpoint_foxglove.json 3
Foxglove WebSocket: ws://127.0.0.1:18765
Publishing topic: /hakoniwa/FoxgloveDemo/sim_time
Schema: hako_msgs/msg/SimTime (ros2msg), message encoding: cdr
published time_usec=0 bytes=12
published time_usec=100000 bytes=12
published time_usec=200000 bytes=12
```

Docker live smoke:

```text
docker compose -f docker/docker-compose.yml build
100% tests passed, 0 tests failed out of 1

docker compose -f docker/docker-compose.yml run --rm --no-deps hakoniwa-pdu-foxglove \
  ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 1

./docker/run-smoke-test.bash
published time_usec=... bytes=12
```

Foxglove UI confirmation:

- Connected from the host browser to `ws://localhost:8765`.
- Confirmed topic `/hakoniwa/FoxgloveDocker/sim_time`.
- Confirmed decoded field `_time_usec` with type `uint64`.
- Confirmed Raw Messages display using
  `/hakoniwa/FoxgloveDocker/sim_time`.
- Confirmed Plot display using
  `/hakoniwa/FoxgloveDocker/sim_time._time_usec`.

Notes:

- The smoke used a temporary copy of the sample config with port `18765`
  because `8765` was unavailable in the sandboxed run.
- Local WebSocket bind required running the example outside the sandbox.
- Browser-based Foxglove UI Raw Messages and Plot confirmation has been
  completed with the Docker publisher.
