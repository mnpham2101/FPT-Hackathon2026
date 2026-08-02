# Bản kế hoạch hành động Phase 5 — Vũ Xuân Bách

Đối chiếu yêu cầu Lead ([phase5_minh_tasks.md](phase5_minh_tasks.md) — **không sửa**) với tiến độ cá nhân. HMI tối giản: Standby đen + Wake-on-Warning God View (không video).

> Chỉ cập nhật trạng thái tại file này.

---

## Bảng so khớp tiến độ

| Yêu cầu Lead | Trạng thái Bách | Ghi chú |
| :--- | :--- | :--- |
| Nhận R4 UDP + Kotlin / kotlinx.serialization (không nlohmann) | **[x] done** | `R4ListenerService`, `R4Deserializer`, Hilt |
| Override cổng UDP `5004` → `47300` | **[x] done** | `local.properties` → `r4.udp.port` |
| Màn hình đen Standby mặc định (`#0D0D1A` + `V2X LINK: STANDBY`) | **[x] done** | `DisplayMode.StandbyView` |
| Wake-on-Warning: Standby → Warning → Standby | **[x] done** | `MainViewModel` + tests |
| Option video ego POV | **[x] cancelled (Lead: tối giản, không video)** | Đã gỡ `VideoView` / `VideoPlayerView` |
| Defensive source guard + Canvas 2D | **[x] done** | Giữ nguyên |
| Unit / integration tests GREEN | **[x] done** | Standby→Warning→Standby |
| Jacoco ≥ 80% | **[ ] pending** | Chưa gate |

---

## Task groups

### B.1 — R4 ingest & cổng ADA

- [x] **B.1.1** Models + deserializer
- [x] **B.1.2** UDP FGS
- [x] **B.1.3** Port override: `r4.udp.port=47300` trong `IVI_ECU/local.properties`

### B.2 — Standby HMI (R16) — tối giản

- [x] **B.2.1** R16 scaffold
- [x] **B.2.2** Standby `#0D0D1A` + `V2X LINK: STANDBY`
- [x] **B.2.3** Không dùng video phức tạp (theo chỉ thị Lead mới)

### B.3 — Wake-on-Warning & Canvas (R17)

- [x] **B.3.1** God View Canvas
- [x] **B.3.2** Defensive source guard
- [x] **B.3.3** Standby → Warning → Standby; tôn trọng user override
- [x] **B.3.4** Tests: default Standby; Standby→Warning→Standby

---

## ADA port (thủ công khi tích hợp)

```properties
# IVI_ECU/local.properties
r4.udp.port=47300
```
