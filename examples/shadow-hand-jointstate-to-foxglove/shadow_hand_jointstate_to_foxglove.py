#!/usr/bin/env python3
"""Bridge ShadowHandAsset/joint_states from Hakoniwa TCP to Foxglove CDR."""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
import time
from pathlib import Path

from python.sensor_msgs.pdu_cdr_conv_JointState import py_to_cdr_JointState
from python.sensor_msgs.pdu_conv_JointState import pdu_to_py_JointState
from hakoniwa_pdu_endpoint.c_endpoint import Endpoint, EndpointError, PduKey


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ENDPOINT_CONFIG = REPO_ROOT / "config/shadow_hand_bridge/endpoint/shadow-hand-tcp-server.json"
#DEFAULT_FOXGLOVE_ENDPOINT_CONFIG = REPO_ROOT / "config/shadow_hand/endpoint_foxglove_jointstate_tf.json"
DEFAULT_FOXGLOVE_ENDPOINT_CONFIG = REPO_ROOT / "config/shadow_hand/endpoint_foxglove_jointstate.json"
DEFAULT_PUBLISHER = REPO_ROOT / "build/cdr_stdin_publisher"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Read bridged ShadowHandAsset/joint_states from TCP and publish it to Foxglove."
    )
    parser.add_argument("--endpoint-config", default=str(DEFAULT_ENDPOINT_CONFIG))
    parser.add_argument(
        "--foxglove-endpoint-config",
        default=str(DEFAULT_FOXGLOVE_ENDPOINT_CONFIG),
    )
    parser.add_argument("--publisher", default=str(DEFAULT_PUBLISHER))
    parser.add_argument("--robot", default="ShadowHandAsset")
    parser.add_argument("--joint-state-pdu", default="joint_states")
    parser.add_argument("--endpoint-name", default="shadow_hand_jointstate_tcp_to_foxglove")
    parser.add_argument("--rate-hz", type=float, default=20.0)
    parser.add_argument("--samples", type=int, default=0)
    return parser.parse_args()


def write_frame(pipe, payload: bytes) -> None:
    pipe.write(struct.pack("<I", len(payload)))
    pipe.write(payload)
    pipe.flush()




def compact_joint_state(joint_state) -> str:
    parts: list[str] = []
    for index, name in enumerate(joint_state.name[:5]):
        position = joint_state.position[index] if index < len(joint_state.position) else 0.0
        parts.append(f"{name}={position:.3f}")
    return " ".join(parts)



def main() -> int:
    args = parse_args()
    if args.rate_hz <= 0.0:
        print("[ERROR] --rate-hz must be positive", file=sys.stderr)
        return 2

    endpoint_config = str(Path(args.endpoint_config).resolve())
    foxglove_endpoint_config = str(Path(args.foxglove_endpoint_config).resolve())
    publisher = str(Path(args.publisher).resolve())
    key = PduKey(args.robot, args.joint_state_pdu)

    endpoint = Endpoint(args.endpoint_name, "inout")
    endpoint.open(endpoint_config)
    endpoint.start()
    pdu_size = endpoint.get_pdu_size(key)
    if pdu_size <= 0:
        print(f"[ERROR] PDU size is not available for {args.robot}/{args.joint_state_pdu}", file=sys.stderr)
        endpoint.stop()
        endpoint.close()
        return 1

    proc = subprocess.Popen(
        [publisher, "--endpoint-config", foxglove_endpoint_config],
        stdin=subprocess.PIPE,
    )
    assert proc.stdin is not None

    print(
        "[INFO] Shadow Hand JointState -> Foxglove bridge started. "
        "Connect Foxglove to ws://127.0.0.1:8766. "
        "Input is Hakoniwa native PDU over TCP; output is CDR.",
        file=sys.stderr,
    )

    interval = 1.0 / args.rate_hz
    sent = 0
    skipped = 0
    try:
        while args.samples == 0 or sent < args.samples:
            try:
                raw = endpoint.recv_by_name(key, pdu_size)
            except EndpointError as exc:
                skipped += 1
                if skipped == 1 or skipped % 50 == 0:
                    print(f"[INFO] waiting for TCP joint_states: {exc}", file=sys.stderr)
                time.sleep(interval)
                continue
            if isinstance(raw, tuple):
                raw = raw[0]
            if not raw:
                skipped += 1
                time.sleep(interval)
                continue
            try:
                joint_state = pdu_to_py_JointState(raw)
            except Exception as exc:
                skipped += 1
                if skipped == 1 or skipped % 50 == 0:
                    print(f"[INFO] waiting for valid joint_states: {exc}", file=sys.stderr)
                time.sleep(interval)
                continue

            # debug only
            #joint_state.position = [0.0] * len(joint_state.position)
            cdr_payload = py_to_cdr_JointState(joint_state)
            write_frame(proc.stdin, cdr_payload)

            if sent % 20 == 0:
                print(
                    "sent JointState CDR "
                    f"samples={sent} joints={len(joint_state.name)} "
                    f"bytes={len(cdr_payload)} {compact_joint_state(joint_state)}",
                    file=sys.stderr,
                )
            sent += 1
            time.sleep(interval)
    except KeyboardInterrupt:
        print("[INFO] interrupted", file=sys.stderr)
    finally:
        try:
            proc.stdin.close()
        except BrokenPipeError:
            pass
        endpoint.stop()
        endpoint.close()
        proc.wait(timeout=5)

    return proc.returncode if proc.returncode not in (None, 0) else 0


if __name__ == "__main__":
    raise SystemExit(main())
