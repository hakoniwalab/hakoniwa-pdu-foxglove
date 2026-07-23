# Shadow Hand SHM to TCP bridge config

This directory is a `hakoniwa-pdu-bridge-core` config root for the Shadow Hand
Foxglove path.

The bridge process itself is provided by `hakoniwa-pdu-bridge-core`, but this
scenario-specific configuration is owned by `hakoniwa-pdu-foxglove` because it
belongs to the Foxglove integration recipe.

Current validated scope:

```text
ShadowHandAsset/joint_states
  -> callback SHM endpoint
  -> Hakoniwa bridge asset
  -> TCP endpoint
```

The TCP payload is still Hakoniwa native PDU binary. The next Foxglove step must
receive this TCP payload, decode `sensor_msgs/JointState` with
`hakoniwa-pdu-registry`, convert it to CDR, and publish it with
`FoxgloveComm`.

## Why the bridge process is used

The initial implementation attempted to read `ShadowHandAsset/joint_states`
directly from Hakoniwa shared memory, convert the native PDU to CDR, and publish
it to Foxglove from the same process.

That approach introduced runtime/threading complexity between the Hakoniwa
callback/SHM execution path and the Foxglove WebSocket publisher. Rather than
spending more time on a multi-threaded single-process implementation, the
integration was simplified by separating the two runtime models at the existing
`hakoniwa-pdu-bridge-core` SHM-to-TCP boundary.

This is an implementation choice, not a fundamental limitation. A future
implementation may remove this bridge process by running the Hakoniwa SHM reader
and Foxglove publisher on separate threads with an explicit internal queue.

For the current validated path, the responsibility split is:

- `hakoniwa-pdu-bridge-core` participates in the Hakoniwa callback/SHM execution
  model and forwards native Hakoniwa PDU bytes over TCP;
- the downstream Python converter performs the type-specific
  `sensor_msgs/JointState` native-PDU-to-CDR conversion;
- `cdr_stdin_publisher` and `FoxgloveComm` handle the generic Foxglove
  publication path.

Keeping these runtime responsibilities in separate processes made the Shadow
Hand integration straightforward to validate without making the bridge a
Foxglove-specific component.
