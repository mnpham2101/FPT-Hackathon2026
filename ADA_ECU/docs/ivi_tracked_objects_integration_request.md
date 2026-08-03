# ADA → IVI integration request: `trackedObjects`

The shared R4 contract now ratifies `trackedObjects` as an optional additive field. ADA sends the
complete tracked vehicle B (`own_sensor`) and vehicle C (`v2x_relayed`) in warning messages while
keeping the existing `object` field for backward compatibility.

Phase 5 owner action:

1. Add optional `trackedObjects` decoding to the IVI R4 Kotlin model.
2. Render or log B and C from that collection; do not infer B from the legacy `object` field.
3. Preserve compatibility with R4 messages that omit `trackedObjects`.
4. Add one fixture test using `contracts/samples/r4-warning.json` and one legacy-message test.
5. During integration, capture the UDP packet at IVI and confirm both IDs: `own:B` and
   `v2x:1201:7`.

ADA-owned proof is executable with:

```sh
python3 ADA_ECU/tools/check_evt_log.py <ada-event-log.jsonl>
```

No Phase 5 source is changed by the ADA implementation.
