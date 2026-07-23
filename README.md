# Cooperative Vehicle Awareness (V2X)

## Project goal

This project implements **V2X (Vehicle-to-Everything)** features that let vehicles communicate with each other to prevent accidents and optimize traffic flow. The system is built and run on a cloud platform to remove any dependency on physical vehicle hardware.

**Milestone 1** demonstrates cooperative, non-line-of-sight (NLOS) awareness: three vehicles drive in a collinear convoy — **A** follows **B** follows **C**. Vehicle A's camera can never see C because B blocks the line of sight. Vehicle B *can* see C, and broadcasts that perception to A over V2X — so both A and B end up displaying C and its relative position, even though A never detects C directly.

Full mission, scope, contracts, and phase plan live in [CLAUDE.md](CLAUDE.md) and [plans/milestone1.md](plans/milestone1.md). Original goals and the deferred future-feature list are in [.claude/prompt/project_goals.md](.claude/prompt/project_goals.md).

## Repository layout

| Path | Purpose |
|---|---|
| [CLAUDE.md](CLAUDE.md) | Project constitution — read this first |
| [plans/](plans/) | Active implementation plan — phases, acceptance criteria, task decomposition source |
| [.claude/rules/](.claude/rules/) | Standing process rules (task/commit conventions, solution selection, HLD format) |
| [.claude/agents/](.claude/agents/) | Subagent specs: project-researcher, project-planner, project-architecture |
| [.claude/skills/](.claude/skills/) | Reusable procedures the agents follow |
| [.claude/prompt/](.claude/prompt/) | Important prompts saved verbatim, with self-graded outcomes |

## Hướng dẫn dành cho contributor

- **Tuân thủ constitution.** Mọi thay đổi phải nhất quán với [CLAUDE.md](CLAUDE.md) — đây là nguồn tham chiếu cao nhất của dự án (mission, governing principles, scope, roles). Rule/skill/agent khác chỉ bổ sung chi tiết, không được mâu thuẫn với file này.
- **Dùng agentic AI, đừng code tay một mình.** Khi cần thêm quy trình hay năng lực mới, hãy tạo agent, rule, hoặc skill tương ứng (theo convention của Claude Code) thay vì tự viết code trực tiếp không qua agent. Tham khảo cấu trúc hiện có tại [.claude/agents/](.claude/agents/), [.claude/rules/](.claude/rules/), [.claude/skills/](.claude/skills/).
- **Lưu lại prompt quan trọng.** Các prompt khởi tạo hoặc định hình lớn cho một task nên được lưu nguyên văn vào [.claude/prompt/](.claude/prompt/) dưới dạng markdown. Sau khi agent hoàn thành task, hãy **tự chấm điểm (rate) prompt của mình theo thang 0–10**, kèm nhận xét ngắn gọn vì sao (prompt có rõ ràng không, agent có hiểu đúng scope không, có thiếu rule/context nào không...). Xem format mẫu ở các file có sẵn trong `.claude/prompt/`.
- **Bắt buộc atomic commit, single objective.** Khi prompt cho agent/subagent thực hiện task, luôn yêu cầu rõ: mỗi subtask chỉ có **một mục tiêu duy nhất (single objective)**, tạo ra đúng **một atomic commit**, và **không** được lẫn code ngoài phạm vi (out-of-scope) vào cùng commit đó. Quy tắc đầy đủ ở [task-planning-conventions.md](.claude/rules/task-planning-conventions.md).
- **Dùng AI platform khác (ví dụ Cursor...).** Nếu không dùng Claude Code, hãy tạo một thư mục ẩn tương ứng (ví dụ `.cursor/`) với cấu trúc tương tự `.claude/` để lưu prompt, rules, skills, agents, plan, task, subtask cho platform đó. Các thư mục này **phải được commit vào git như bình thường**, không được `.gitignore`.
- **Làm việc theo branch, mở Pull Request.** Mỗi feature làm trên một branch riêng, không commit thẳng vào `main`/`master`. Khi xong, mở Pull Request (PR) để review trước khi merge.
- **Mục tiêu số 1 của dự án này là học cách dùng agentic AI đúng cách** — nắm vững kiến thức và luồng vận hành của project (command of the project), chứ **không phải "vibe coding"** (để AI tự quyết định mọi thứ mà mình không hiểu rõ). Vì vậy, hãy luôn đọc và hiểu output của agent trước khi merge — đừng approve chỉ vì "nó chạy được".
