# Knowledge base

Topic notes that outlive the work that prompted them: the platform, the protocols, the toolchain, and anything a teammate had to work out once and should not have to work out again.

Notes here are written for a reader outside this project. They use engineering vocabulary and name no requirement numbers, no phases, and no project-internal terms of art — a note that cannot be written that way is about one of our nodes, and belongs in [Design/](../Design/) instead.

**Concrete identifiers are allowed as worked examples.** Where a subject cannot be taught without something specific to point at — a field, a message type, a test — this project's own names may serve as that example, provided the note teaches the general case and a reader can substitute their own names for ours. Where the lesson does not turn on the specifics, prefer a generic role name:

| Note | Naming | Why |
|---|---|---|
| [android-automotive-os.md](android-automotive-os.md) | Role names — `IncomingMessageListener`, `MessageRepository` | Nothing in the lifecycle lesson depends on what the types are called |
| [UDP-msg-parsing.md](UDP-msg-parsing.md) | Real field, message and test names | The decode behaviour it describes is only checkable against the actual schema and tests |

| Document | Topic |
|---|---|
| [android-automotive-os.md](android-automotive-os.md) | Android Automotive OS as an application target: what the platform provides and what it constrains |
| [UDP-msg-parsing.md](UDP-msg-parsing.md) | Deserialising a UDP JSON payload into typed Kotlin: buffer slicing, library selection, and the handling the library does not provide |
| [producer-simulation-harness.md](producer-simulation-harness.md) | Simulating a message producer so its consumer can be exercised alone |

A note belongs here rather than in [Design/](../Design/) when it is about a **subject** rather than about one of our nodes — V2X message structure, how AAOS boots, what a CarSky blueprint is. Anything specific to a node goes in that node's Design subfolder.

Platform and node facts that agents rely on stay in [requirements/car-sky-guide/](../../requirements/car-sky-guide/), which is the reference other work reads; a note here may explain those facts but never replaces them.
