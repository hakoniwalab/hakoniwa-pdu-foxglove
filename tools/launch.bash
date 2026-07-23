#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <launch_file>"
    exit 1
fi

LAUNCH_FILE=$1

export HAKONIWA_LIBPATH=/usr/local/hakoniwa/lib
export HAKONIWA_BINPATH=/usr/local/hakoniwa/bin

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
export FOXGLOVE_DIRPATH="$(cd "${SCRIPT_DIR}/.." && pwd -P)"

export WORKSPACE_DIR="$(dirname "${FOXGLOVE_DIRPATH}")"
export MUJOCO_DIRPATH="${WORKSPACE_DIR}/hakoniwa-mujoco-robots"
export BRIDGE_DIRPATH="${WORKSPACE_DIR}/hakoniwa-pdu-bridge-core"

OS_TYPE=$(uname)
if [ "$OS_TYPE" == "Darwin" ]; then
    export HAKO_PDU_ENDPOINT_SHARED_LIB="${FOXGLOVE_DIRPATH}/hakoniwa-pdu-endpoint/build-shared/src/libhakoniwa_pdu_endpoint.dylib"
else
    export HAKO_PDU_ENDPOINT_SHARED_LIB="${FOXGLOVE_DIRPATH}/hakoniwa-pdu-endpoint/build-shared/src/libhakoniwa_pdu_endpoint.so"
fi
export PYTHON_CMD="${PYTHON_CMD:-python3}"
"$PYTHON_CMD" -m hakoniwa_pdu.apps.launcher.hako_launcher "$LAUNCH_FILE"

