# Simulating a UDP message producer to exercise its consumer

A consumer that receives messages over UDP cannot be developed or tested against a producer that does not yet run, runs only on a target rack, or is being changed at the same time. A simulated producer removes that coupling: it emits the same wire format on the same transport, so the consumer under test executes its real receive, deserialisation and presentation path.

This document covers the producer stand-in for the ADA-ECU to IVI-ECU message stream defined in [r4-ada-ivi.schema.json](../../contracts/r4-ada-ivi.schema.json).

## 1. What a producer stand-in has to reproduce

Only the properties the consumer can observe:

- **Wire format** — identical bytes on the wire, generated from the same schema samples the consumer's tests use, so a divergence is a defect in one of the two bindings rather than in the harness.
- **Message sequence** — a realistic cycle rather than a single packet: approach, periodic state, departure, and a message carrying an unrecognised enumeration value.
- **Transport parameters** — destination host and port supplied by environment variable, so the same artifact drives a loopback interface, a container network, or a device on a bench.

It does not have to reproduce the producer's internal logic. Emission timing that depends on the producer's own perception and risk assessment is not modelled, and a harness therefore proves nothing about that logic — only about the consumer's handling of what arrives.

## 2. Candidate producers

| Candidate | Assessment | Verdict |
|---|---|---|
| **A. Script-based sender in a scripting language** | Independent of the consumer's build; runs on a laptop and in a container; parameterised entirely by environment | **Selected** — smallest artifact that exercises the real transport |
| **B. Sender written in the consumer's language, in the consumer's build** | Shares the model classes, so unit tests can call it in process | Useful for in-process tests only; adds networking surface to the application build for no gain on the wire |
| **C. The real producer, deployed alongside** | The genuine emission path | Requires the producer's own inputs and its full deployment; appropriate for integration testing, not for developing the consumer |

A and C run as Linux containers and can therefore be deployed on the same network as the consumer; B is local to a development machine.

The existing stand-in is [IVI_ECU/mock-sender/](../../IVI_ECU/mock-sender/), which emits the cycle described in §1.

## 3. Invocation modes

### 3.1 Loopback on a development machine

```text
cd IVI_ECU/mock-sender
IVI_ECU_HOST=127.0.0.1 IVI_ECU_PORT=5004 CYCLES=2 python mock_r4_sender.py
```

The consumer must listen on the same port. The port is build configuration on the consumer side, so the two values are set together.

### 3.2 Container on the same network as the consumer

The sender is built as an image and deployed as a node beside the consumer. Its destination host is the consumer node's address on the shared network, supplied by environment variable, and its destination port matches the consumer's listen port.

### 3.3 No network — fixtures in unit tests

Schema sample files are fed directly into the deserialiser and the repository under test, bypassing the socket. A full-stack variant opens a loopback socket inside the test process, which exercises the receive loop without leaving the JVM.

These three modes are complementary: §3.3 proves deserialisation and state propagation, §3.1 proves the receive loop and the rendering path, §3.2 proves address configuration and container networking. A defect that appears only in §3.2 is a deployment or configuration fault, not a parsing fault.

## 4. Consumer behaviour the harness verifies

| Emitted message | Expected consumer behaviour |
|---|---|
| Warning with a recognised type | Warning view raised; geometry rendered; risk state reflected |
| Periodic state | Scene updated, last value winning, ordered by sequence number |
| Warning with an unrecognised type | No failure; degraded presentation; value preserved |
| Object whose provenance field is not the relayed value | Defensive marker rendered and the condition logged |

## 5. The event seam the harness depends on

The harness only works if the consumer's arrival path is a seam rather than a straight line from socket to view:

- The datagram is received on an IO dispatcher, decoded as UTF-8, deserialised, and published as a domain event on a hot stream.
- The presentation layer observes view-model state and performs no deserialisation on the main thread.
- The event is named at the repository or service boundary in terms of the message, not in terms of the transport or the deployment platform that delivered it. A consumer that names its events after its transport cannot be driven by any other producer, which is the coupling the harness exists to remove.

## Related

- Deserialising the payload: [UDP-msg-parsing.md](UDP-msg-parsing.md)
