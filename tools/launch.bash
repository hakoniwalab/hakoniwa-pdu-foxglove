#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <launch_file>"
    exit 1
fi

LAUNCH_FILE=$1

if [ ! -f "$LAUNCH_FILE" ]; then
    echo "ERROR: launch file not found: $LAUNCH_FILE" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
export FOXGLOVE_DIRPATH="$(cd "${SCRIPT_DIR}/.." && pwd -P)"

export WORKSPACE_DIR="$(dirname "${FOXGLOVE_DIRPATH}")"
export MUJOCO_DIRPATH="${WORKSPACE_DIR}/hakoniwa-mujoco-robots"
export BRIDGE_DIRPATH="${WORKSPACE_DIR}/hakoniwa-pdu-bridge-core"
export HAKONIWA_CORE_ROOT="${HAKONIWA_CORE_ROOT:-/usr/local/hakoniwa}"
export HAKONIWA_ENDPOINT_CORE_PREFIX="${HAKONIWA_ENDPOINT_CORE_PREFIX:-${HAKONIWA_DEMO_PREFIX:-${FOXGLOVE_DIRPATH}/work/install/endpoint-core}}"
export HAKO_PDU_ENDPOINT_PYTHON_BUILD_DIR="${HAKO_PDU_ENDPOINT_PYTHON_BUILD_DIR:-${FOXGLOVE_DIRPATH}/work/build/manifest/endpoint-core-callback/python}"
export PYTHONPATH="${FOXGLOVE_DIRPATH}/hakoniwa-pdu-endpoint/python:${HAKO_PDU_ENDPOINT_PYTHON_BUILD_DIR}${PYTHONPATH:+:${PYTHONPATH}}"
export HAKONIWA_LIBPATH="${HAKONIWA_LIBPATH:-${HAKONIWA_CORE_ROOT}/lib}"
export HAKONIWA_ENDPOINT_LIBPATH="${HAKONIWA_ENDPOINT_LIBPATH:-${HAKONIWA_ENDPOINT_CORE_PREFIX}/lib}"
export HAKONIWA_BINPATH="${HAKONIWA_BINPATH:-${HAKONIWA_CORE_ROOT}/bin}"

OS_TYPE=$(uname)
if [ "$OS_TYPE" == "Darwin" ]; then
    DEFAULT_ENDPOINT_SHARED_LIB="${HAKONIWA_ENDPOINT_CORE_PREFIX}/lib/libhakoniwa_pdu_endpoint_core_callback.dylib"
else
    DEFAULT_ENDPOINT_SHARED_LIB="${HAKONIWA_ENDPOINT_CORE_PREFIX}/lib/libhakoniwa_pdu_endpoint_core_callback.so"
fi
export HAKO_PDU_ENDPOINT_SHARED_LIB="${HAKO_PDU_ENDPOINT_SHARED_LIB:-${DEFAULT_ENDPOINT_SHARED_LIB}}"
export PYTHON_CMD="${PYTHON_CMD:-${HAKO_PYTHON:-python3.12}}"

require_local_artifact() {
    if [ ! -e "$1" ]; then
        echo "ERROR: required local demo artifact not found: $1" >&2
        echo "Build the Shadow Hand demo binaries documented in README.md before launching." >&2
        exit 1
    fi
}

require_local_artifact "${MUJOCO_DIRPATH}/src/cmake-build/examples/actuators/shadow_hand/shadow-hand-hakoniwa-asset"
require_local_artifact "${BRIDGE_DIRPATH}/build-shadow-hand/hakoniwa-pdu-web-bridge"
require_local_artifact "${FOXGLOVE_DIRPATH}/build/cdr_stdin_publisher"
require_local_artifact "${HAKO_PDU_ENDPOINT_SHARED_LIB}"
require_local_artifact "${HAKO_PDU_ENDPOINT_PYTHON_BUILD_DIR}/hakoniwa_pdu_endpoint"

if ! "$PYTHON_CMD" -c '
import sys
if sys.version_info[:2] != (3, 12):
    raise SystemExit(f"Hakoniwa Python workflows require Python 3.12, got {sys.version.split()[0]}")
import hakoniwa_pdu.apps.launcher.hako_launcher
'; then
    echo "ERROR: Hakoniwa Python 3.12 with hakoniwa-pdu is required." >&2
    echo "Set HAKO_PYTHON or PYTHON_CMD to the prepared Python 3.12 interpreter." >&2
    exit 1
fi

"$PYTHON_CMD" -m hakoniwa_pdu.apps.launcher.hako_launcher "$LAUNCH_FILE"
