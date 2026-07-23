#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <launch_file>"
    exit 1
fi

LAUNCH_FILE=$1

export HAKONIWA_LIBPATH=/usr/local/hakoniwa/lib
export HAKONIWA_BINPATH=/usr/local/hakoniwa/bin
export FOXGLOVE_DIRPATH="$(pwd)"
export MUJOCO_DIRPATH="$(pwd)/../hakoniwa-mujoco-robots"
export BRIDGE_DIRPATH="$(pwd)/../hakoniwa-pdu-bridge-core"

OS_TYPE=$(uname)
if [ "$OS_TYPE" == "Darwin" ]; then
    export HAKO_PDU_ENDPOINT_SHARED_LIB="$(pwd)/hakoniwa-pdu-endpoint/build-shared/src/libhakoniwa_pdu_endpoint.dylib"
else
    export HAKO_PDU_ENDPOINT_SHARED_LIB="$(pwd)/hakoniwa-pdu-endpoint/build-shared/src/libhakoniwa_pdu_endpoint.so"
fi
export PYTHON_CMD="${PYTHON_CMD:-python3}"
python -m hakoniwa_pdu.apps.launcher.hako_launcher "$LAUNCH_FILE"

