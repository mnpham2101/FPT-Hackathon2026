# Contributor documents — Milestone 1

Team learning notes and short reports written during code freeze, with product code unchanged. The lead aggregates them into the delivery report.

These are **contributor write-ups, not authorities**. A node's design is fixed by its HLD and the requirements report; a document here explains, summarises or teaches — it never redefines. Where the two disagree, the authority wins.

| Folder | What goes in it |
|---|---|
| [Plan-Proposal/](Plan-Proposal/) | The milestone plan: its phases, their inputs and their acceptance criteria — the authority phase planning decomposes from |
| [Design/](Design/) | How a node is built and why, one subfolder per ECU |
| [Delivery/](Delivery/) | Reports on what was delivered, and the evidence behind it |
| [KnowledgeBase/](KnowledgeBase/) | Topic notes that outlive the milestone — platform, protocol, tooling |

## Authorities these documents defer to

| Document | Fixes |
|---|---|
| [m1-cooperative-awareness.md](../requirements/m1-cooperative-awareness.md) | The requirements R1–R19 and the decision record |
| `<Node>/doc/<node-slug>-hld.md` | That node's components, paths, seams and contracts |
| [car-sky-guide/](../requirements/car-sky-guide/) | The platform, its nodes, and the deploy/verify walkthroughs |
