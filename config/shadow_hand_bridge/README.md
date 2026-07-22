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
