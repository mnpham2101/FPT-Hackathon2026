# Cooperative Vehicle Awareness (V2X)

## Project goals

This project implements **V2X (Vehicle-to-Everything)** features that let vehicles communicate with each other to prevent accidents and optimize traffic flow. The system is built and run on a cloud platform to remove any dependency on physical vehicle hardware.

**Milestone 1** demonstrates cooperative, non-line-of-sight (NLOS) awareness: three vehicles drive in a collinear convoy — **A** follows **B** follows **C**. Vehicle A's camera can never see C because B blocks the line of sight. Vehicle B *can* see C, and broadcasts that perception to A over V2X — so both A and B end up displaying C and its relative position, even though A never detects C directly.

Full mission, scope, contracts, and phase plan live in [CLAUDE.md](CLAUDE.md) and [milestone1_high_level_plan.md](documents/Plan/milestone1_high_level_plan.md). Original goals and the deferred future-feature list are in [.claude/prompts/project_goals.md](.claude/prompts/project_goals.md).

## Repository layout

| Folder | Contents |
|---|---|
| [CLAUDE.md](CLAUDE.md) | Project constitution — read this first |
| [requirements/](requirements/) | Requirements documents, analysis, frozen contracts (R1–R6), and deployment guides |
| [Scenario_Player/](Scenario_Player/), [V2X_ECU/](V2X_ECU/), [ADA_ECU/](ADA_ECU/), [IVI_ECU/](IVI_ECU/) | Implementation code per node: source, build config, and tests |
| [documents/](documents/) | The project's written record — `Design/` (one HLD per node), `KnowledgeBase/`, `Plan/`, `Proposals/`, `Requirements/`, `Delivery/` |
| [plans/](plans/) | Implementation plan (phases, tasks, acceptance criteria); includes `doc/` with run records |
| [presentation/](presentation/) | The slide decks, their shared template, and `slide-build-tool/` |
| [website/](website/) | The static hub site: its `css/`, `js/`, assets and `build-pages.py`. `pages/` is generated — build it after cloning |
| [tools/](tools/) | Test equipment — diagnostic tools and containers that stand in for a node |
| [Package-Delivery-tool/](Package-Delivery-tool/) | Builds the outgoing delivery package — `Hackathon-Delivery/` and its zip |
| [.claude/](\.claude/) | Tooling: `rules/` (process conventions), `agents/` (agent specs), `skills/` (procedures), `prompts/` (saved prompts) |

[presentation/](presentation/) and [website/](website/) are the two human-facing publications, each self-contained with its own content, design system and generator. Both render [documents/](documents/); neither owns it.

# Tools

## Building project presentation

Presentations are authored as Marp-flavoured Markdown. `build-slides.py` generates the static HTML:

```bash
python presentation/slide-build-tool/build-slides.py presentation/phase0/phase0-smoke-test-deck.md
# Output: presentation/phase0/phase0-smoke-test-deck.html
```

The export lands beside its Markdown source with the same basename. Edit only the Markdown — the next build overwrites the HTML.

Layouts, styles, and HTML components live in [presentation/template/template.md](presentation/template/template.md). File placement, the asset policy, and the build workflow are in [.claude/rules/deck-authoring-conventions.md](.claude/rules/deck-authoring-conventions.md).

## Building the project wiki website

Both build modes below generate the pages strictly from the markdown in [documents/](documents/) and place them in `website/pages/` (generated, gitignored).

### 1 · Build wiki pages served by an HTTP server

Stand in the repo root directory.

Build:

```bash
python website/build-pages.py
```

Run:

```bash
python -m http.server 8080
```

Open the site at `http://localhost:8080/website/index.html`.

### 2 · Build static, standalone wiki pages, served without an HTTP server

Stand in the repo root directory.

Build:

```bash
python website/build-pages.py --bundle
```

This also writes a self-contained copy to `dist/`. Open `dist/index.html` by double-clicking — no server, no Python needed.

### Build the project delivery page for Round 2

Stand in the repo root directory and run:

```bash
python Package-Delivery-tool/build_package.py
```

The output is `Package-Delivery-tool/Hackathon-Delivery/` and `Hackathon-Delivery.zip`.

### Build the project delivery page for Round 3

## Project delivery

- To build the APK for the IVI-ECU and deploy it on the IVI-ECU: [apk-deploy.md](documents/Delivery/Test-Guides/apk-deploy.md).
- To build the images for the ADA-ECU, the V2X-ECU and the other nodes: build on GitHub Actions (details to be provided later).

## Platform access & credentials

Building and deploying on CarSky needs a **Zot registry API key** (`zak_...`) — the `docker login` password for pushing the ECU/bench images, and the `CARSKY_ZOT_API_KEY` GitHub Actions secret the CI push job uses. How to create, store, and use it: [requirements/car-sky-guide/zot-registry-api-key.md](requirements/car-sky-guide/zot-registry-api-key.md). All other CarSky deployment guides live in [requirements/car-sky-guide/](requirements/car-sky-guide/).

## Hướng dẫn dành cho contributor

- **Tuân thủ constitution.** Mọi thay đổi phải nhất quán với [CLAUDE.md](CLAUDE.md) — đây là nguồn tham chiếu cao nhất của dự án (mission, governing principles, scope, roles). Rule/skill/agent khác chỉ bổ sung chi tiết, không được mâu thuẫn với file này.
- **Dùng agentic AI, đừng code tay một mình.** Khi cần thêm quy trình hay năng lực mới, hãy tạo agent, rule, hoặc skill tương ứng (theo convention của Claude Code) thay vì tự viết code trực tiếp không qua agent. Tham khảo cấu trúc hiện có tại [.claude/agents/](.claude/agents/), [.claude/rules/](.claude/rules/), [.claude/skills/](.claude/skills/).
- **Quy trình gợi ý khi được giao một requirement.**
  1. Gọi agent **[project-researcher](.claude/agents/project-researcher.md)** nghiên cứu requirement (Rx trong [m1-cooperative-awareness.md](documents/Requirements/m1-cooperative-awareness.md)) và đề xuất kế hoạch triển khai — một file `plans/Rx_[Phase].md` với các bước được đánh số (theo skill [implementation-step-proposal](.claude/skills/implementation-step-proposal/SKILL.md)).
  2. **Tự review kế hoạch**: đọc và hiểu từng bước, đối chiếu với định nghĩa và acceptance của requirement trước khi đi tiếp.
  3. Gọi agent **[project-planner](.claude/agents/project-planner.md)** tạo task/subtask theo mã `X.Y.Z.W` từ kế hoạch đó và điều phối subagent thực hiện.
  4. **Build và test**: mỗi subtask chỉ được coi là "done" khi build pass, unit test pass, và có đúng một atomic commit ([task-planning-conventions.md](.claude/rules/task-planning-conventions.md)).
  5. Nếu điều kiện cho phép, **deploy blueprint lên CarSky** (tạo Room) để kiểm thử end-to-end trên môi trường thật (R5/R6).
- **Lưu lại prompt quan trọng.** Các prompt khởi tạo hoặc định hình lớn cho một task nên được lưu nguyên văn vào [.claude/prompts/](.claude/prompts/) dưới dạng markdown. Sau khi agent hoàn thành task, hãy **tự chấm điểm (rate) prompt của mình theo thang 0–10**, kèm nhận xét ngắn gọn vì sao (prompt có rõ ràng không, agent có hiểu đúng scope không, có thiếu rule/context nào không...). Xem format mẫu ở các file có sẵn trong `.claude/prompts/`.
- **Bắt buộc atomic commit, single objective.** Khi prompt cho agent/subagent thực hiện task, luôn yêu cầu rõ: mỗi subtask chỉ có **một mục tiêu duy nhất (single objective)**, tạo ra đúng **một atomic commit**, và **không** được lẫn code ngoài phạm vi (out-of-scope) vào cùng commit đó. Quy tắc đầy đủ ở [task-planning-conventions.md](.claude/rules/task-planning-conventions.md).
- **Dùng AI platform khác (ví dụ Cursor...).** Nếu không dùng Claude Code, hãy tạo một thư mục ẩn tương ứng (ví dụ `.cursor/`) với cấu trúc tương tự `.claude/` để lưu prompt, rules, skills, agents, plan, task, subtask cho platform đó. Các thư mục này **phải được commit vào git như bình thường**, không được `.gitignore`.
- **Làm việc theo branch, mở Pull Request.** Mỗi feature làm trên một branch riêng, không commit thẳng vào `main`/`master`. Khi xong, mở Pull Request (PR) để review trước khi merge.
- **Mục tiêu số 1 của dự án này là học cách dùng agentic AI đúng cách** — nắm vững kiến thức và luồng vận hành của project (command of the project), chứ **không phải "vibe coding"** (để AI tự quyết định mọi thứ mà mình không hiểu rõ). Vì vậy, hãy luôn đọc và hiểu output của agent trước khi merge — đừng approve chỉ vì "nó chạy được".
