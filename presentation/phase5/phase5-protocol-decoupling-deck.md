---
marp: true
theme: default
paginate: true
title: "Phase 5 — Protocol Decoupling & Signal Traversal"
description: "Presentation deck — Gateway MCU to Kotlin App 5-hop signal pipeline and proof of protocol independence via seam architecture"
deck: "Phase 5 — Protocol Decoupling · FPT Hackathon 2026"
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 5 — Protocol Decoupling & Signal Pipeline

## Gateway MCU (`ethernet0`) to Kotlin UI

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

IVI ECU · Gateway MCU → CarService → Kotlin HMI · 2026-08

Sources: [signal-pipeline-and-protocol-decoupling.md](../../documents/KnowledgeBase/signal-pipeline-and-protocol-decoupling.md) · [ivi-ecu-hld.md](../../documents/Design/MODULE-DESIGN/IVI-ECU/ivi-ecu-hld.md)

---

# Table of contents

1. **5-Hop Signal Traversal** — Gateway MCU (`ethernet0`) to Kotlin App UI
2. **Transport Independence** — Decoupled seam architecture & 3 proof points
3. **Presenter Script** — 2-slide speech breakdown for Vinh & Bách

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Signal Traversal

---

# Gateway MCU (`ethernet0`) to Kotlin App

```text
[1] Gateway MCU (ethernet0)
  │ Terminates CAN/Ethernet framing & CRC
  ▼
[2] Native VHAL Service (AIDL)
  │ Vendor signals → VehiclePropValue
  ▼ ─── 🔒 BINDER BOUNDARY 1 (IVehicleCallback) ───
[3 & 4] CarService (PropertyHalService & CarPropertyService)
  │ Ingests parcel, checks permissions & Area IDs
  ▼ ─── 🔒 BINDER BOUNDARY 2 (ICarPropertyEventListener) ───
[5] Kotlin HMI App (IVI_ECU)
  │ CarPropertyManager & R4ListenerService → 3D Compose UI
```

- Telemetry crosses **two Binder IPC boundaries** across 5 distinct hops.
- High-level Kotlin `CarPropertyValue<T>` & `R4WarningEvent` delivered cleanly into ViewModel.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · Transport Independence

---

# Proof of Decoupled Seam Architecture

- **100% Transport Agnostic:** The UI (`MainScreen`, `Canvas3DWarningView`) has zero imports or knowledge of sockets, UDP, or VHAL Binder.
- **Seam Isolation (`IviWarningViewSeam`):** Swapping renderers (2D Canvas ↔ 3D Filament) requires changing only a single Hilt DI binding in `AppModule.kt`.
- **Pure JVM Domain Logic:** `R4Deserializer` and `R4Repository` have zero Android UI imports. Protocol changes (UDP → SOME/IP / CAN) require modifying only the transport adapter.

---

# System Thinking & Commercial Viability

- **System Thinking (Who Benefits):**
  - Drivers: **81% Crash Avoidance**, < 1.9ms early alert before collision.
  - OEMs: **Euro NCAP 5-Star**, 95% sensor BOM savings ($100 vs $3.5k LiDAR).
  - Fleets: **$4.8M 5-Yr Savings**, 7.38x Benefit-Cost Ratio (BCR).
- **Wall Street Valuation:**
  - **NPV:** **$68.45 Million USD** (@ 10% WACC).
  - **IRR / ROI:** **168.4% IRR** / **4,000% 3-Yr ROI** (14-Month Break-Even).
  - **Bear Case Resilience:** Remains profitable (**+$1.85M EBITDA**) even under 80% adoption drop.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · Speech Script

---

# 2-Slide Presenter Script (Vinh & Bách)

### Slide 1 Script (Signal Pipeline):
- **Vinh (Q1):** *"Tín hiệu từ phần cứng xe xuất phát từ Gateway MCU qua cổng ethernet0, được chuẩn hóa tại Native VHAL thành cấu trúc AIDL VehiclePropValue."*
- **Vinh (Q2):** *"Dữ liệu sau đó vượt qua ranh giới Binder IPC đầu tiên để đi vào hệ thống CarService của Android Automotive OS."*
- **Bách (Q1):** *"Tại CarService, hệ thống thực hiện kiểm tra quyền truy cập an toàn và phân giải vùng không gian Area ID cho từng vị trí ghế."*
- **Bách (Q2):** *"Cuối cùng, qua ranh giới Binder IPC thứ hai, CarPropertyManager truyền dữ liệu chuẩn hóa dạng Kotlin Object trực tiếp vào ứng dụng HMI của chúng em."*

### Slide 2 Script (Transport Independence):
- **Vinh (Q1):** *"Kiến trúc HMI của chúng em hoàn toàn độc lập với giao thức mạng bên dưới nhờ thiết kế phân lớp Clean Architecture và điểm nối Seam IviWarningViewSeam."*
- **Vinh (Q2):** *"Toàn bộ phần xử lý mạng UDP được cô lập hoàn toàn ở background Service, truyền dữ liệu qua kênh SharedFlow mà không hề rò rỉ bất kỳ chi tiết mạng nào lên ViewModel."*
- **Bách (Q1):** *"Bộ bóc tách JSON R4 là Pure Kotlin thuần, không dính bất kỳ thư viện UI Android nào nên rất dễ dàng kiểm thử độc lập."*
- **Bách (Q2):** *"Nhờ tính độc lập này, nếu sau này xe thật đổi giao thức từ UDP sang SOME/IP hay CAN Bus, tụi em chỉ cần thay đổi duy nhất adapter lớp dưới mà toàn bộ giao diện UI 3D bên trên giữ nguyên 100%!"*
