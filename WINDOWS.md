# Windows x86_64 build and validation

`hakoniwa-pdu-foxglove` supports the official Foxglove C++ SDK prebuilt archive for Windows x86_64 / MSVC.

This document covers the native Windows build and the repository smoke path. Windows ARM64 is not currently claimed as supported because its SDK archive/checksum and runtime path have not been verified here.

## Prerequisites

- Windows 11 x86_64
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.20 or later
- Git with recursive submodules
- Boost.Asio and Boost.Beast headers used by `hakoniwa-pdu-endpoint`
- Fast-CDR for the bundled examples

The repository fetches the pinned Foxglove SDK automatically. For dependency discovery, vcpkg is the recommended Windows path.

Example:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
& "$env:VCPKG_ROOT\vcpkg.exe" install boost-asio:x64-windows boost-beast:x64-windows fastcdr:x64-windows
```

Initialize the repository submodules:

```powershell
git submodule update --init --recursive
```

## Build, test, and smoke

The recommended path is:

```powershell
.\tools\run_windows_smoke.ps1 -VcpkgRoot $env:VCPKG_ROOT
```

The runner:

1. configures an x64 MSVC build;
2. builds the library, examples, and tests;
3. runs CTest;
4. launches `cdr_publisher_example.exe` for a short three-sample publisher lifecycle smoke.

A custom build directory or configuration can be selected with:

```powershell
.\tools\run_windows_smoke.ps1 `
  -BuildDir build-win `
  -Configuration Release `
  -VcpkgRoot $env:VCPKG_ROOT
```

If dependencies are already discoverable through CMake's normal search paths, `-VcpkgRoot` may be omitted. The runner emits a warning in that case so dependency discovery is not implicit.

## What the Windows compatibility code does

The Windows path has three repository-level compatibility requirements:

1. `cmake/FoxgloveSdk.cmake` selects the pinned `x86_64-pc-windows-msvc` Foxglove SDK archive and verifies its SHA256.
2. The Foxglove SDK target links `ntdll` because the Rust-built Foxglove core references `NtCreateNamedPipeFile` on Windows.
3. The `hakoniwa_pdu_foxglove` consumer target uses MSVC's `/Zc:twoPhase-` compatibility mode. Foxglove SDK v0.25.2 contains a template-specialization/header-ordering pattern in `parameter.hpp` that MSVC rejects under strict two-phase lookup. The workaround is intentionally scoped to this target and should be removed when the pinned/upstream SDK no longer requires it.

## Foxglove Desktop connection

After a successful build, a longer publisher run can be started manually:

```powershell
.\build-win\Release\cdr_publisher_example.exe config\sample\endpoint_foxglove.json
```

Connect Foxglove Desktop to:

```text
ws://127.0.0.1:8765
```

Then confirm the topic:

```text
/hakoniwa/FoxgloveDemo/sim_time
```

## Windows Firewall note

An external Windows 11 reproduction observed that Foxglove Desktop connectivity changed after adding a Windows Defender Firewall inbound rule for the publisher executable. That before/after observation is useful diagnostic evidence, but the exact mechanism was not isolated and is not treated here as a code-level root cause.

Therefore this repository does **not** automatically modify Windows Firewall rules.

If the publisher smoke passes but a desktop client cannot connect, inspect the listener and local network/firewall state explicitly before attributing the failure to the Foxglove protocol or this adapter. In particular, check the configured port, process lifetime, `netstat`/`Get-NetTCPConnection`, and relevant Windows Firewall policy/rules.

## Validation boundary

`run_windows_smoke.ps1` validates build, unit tests, endpoint startup, CDR conversion, send calls, and publisher shutdown. It does not claim end-to-end desktop connectivity through the machine's local firewall/network policy.

A full manual verification should additionally connect the official Foxglove Desktop application and confirm that `/hakoniwa/FoxgloveDemo/sim_time` updates live.
