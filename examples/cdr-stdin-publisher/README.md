# cdr_stdin_publisher

`cdr_stdin_publisher` is a small process boundary between a type-specific CDR
producer and the generic `FoxgloveComm` transport.

It does not decode Hakoniwa native PDU data. Its input contract is already
encoded, complete Foxglove-compatible CDR payloads.

## Why this process exists

`FoxgloveComm` intentionally stays type-independent: it publishes caller-provided
CDR bytes and does not know how to convert Hakoniwa native PDU representations.

For the Shadow Hand integration, the Python converter owns the type-specific
part:

```text
Hakoniwa native JointState PDU
  -> pdu_to_py_JointState()
  -> py_to_cdr_JointState()
  -> complete CDR payload
```

`cdr_stdin_publisher` then owns the generic publication boundary:

```text
stdin CDR frame
  -> cdr_stdin_publisher
  -> hakoniwa-pdu-endpoint
  -> FoxgloveComm
  -> Foxglove WebSocket
```

This separation avoids adding Shadow-Hand-specific conversion logic or Python
bindings to `FoxgloveComm`. It also makes the publisher reusable by any producer
that can generate a complete CDR payload.

## Stdin framing

The default non-multiplexed framing is:

```text
<u32 payload_size, little-endian><CDR payload>
```

With `--multiplex`, each frame is:

```text
<u16 channel_index, little-endian><u32 payload_size, little-endian><CDR payload>
```

The endpoint configuration determines the Foxglove topic, schema, and PDU key.

## Shadow Hand usage

The Shadow Hand Python converter starts this executable as a child process and
writes length-prefixed `sensor_msgs/msg/JointState` CDR frames to its stdin.
The default endpoint config is:

```text
config/shadow_hand/endpoint_foxglove_jointstate.json
```

The resulting topic is:

```text
/hakoniwa/ShadowHandAsset/joint_states
```
