# Đề xuất Phương án Demo Giao diện IVI HMI (Phase 5)

**Người gửi:** Vinh (Team IVI)  
**Ngày:** 02/08/2026  
**Chủ đề:** Đơn giản hóa kiến trúc Demo IVI: Chuyển từ Video Feed sang Event-Driven Wake-on-Warning HMI

---

## 1. Lý do không triển khai Video Feed trên IVI

Qua phân tích tài liệu kiến trúc (`m1-cooperative-awareness.md` & `m1-video-source-and-ivi-dashcam.md`), team đề xuất **không đưa Video Feed vào màn hình IVI** trong Phase 5 vì các lý do kỹ thuật sau:

1. **Ngoài Scope Yêu cầu gốc (Deferred Scope):** 
   - Yêu cầu chính của IVI theo Milestone 1 là **R16 (Display Area Switcher)** và **R17 (God-View Canvas Renderer)**. Video feed trên IVI đã được ghi nhận là mục mở rộng (deferred), không nằm trong tiêu chí nghiệm thu chính.
2. **Phức tạp về mặt Kiến trúc & Hạ tầng:**
   - Cần thêm 1 node nguồn phát video, cấu hình stream HTTP/RTP. 
   - Phía CarSky `video` pin hiện chưa có C++ SDK chính thức và gặp rủi ro lớn về băng thông/fan-out.
3. **Vấn đề Đồng bộ Thời gian (Time-Sync Hazard):**
   - Video là bản ghi sẵn (pre-recorded), trong khi bản tin V2X `R4` từ ADA-ECU phát theo thời gian thực (real-time UDP). 
   - Nếu video và bản tin V2X bị lệch dù chỉ 1–2 giây (ví dụ: hình xe B xuất hiện ở giây 10 nhưng cảnh báo V2X lại nổ ở giây 12), demo sẽ bị giảm tính thuyết phục và thiếu tính chính xác.

---

## 2. Giải pháp Đề xuất: Event-Driven Wake-on-Warning HMI

Thay vì phát video tĩnh, IVI sẽ vận hành theo đúng **kiến trúc HMI ô tô thực tế (Automotive Standard)** dựa trên trạng thái nhận bản tin V2X:

```
[Màn hình Idle: HomeView] ──(Nhận R4 UDP Packet từ ADA)──> [Màn hình Cảnh báo: WarningView]
         ▲                                                               │
         └───────────────────(Hết Timeout 10 giây)───────────────────────┘
```

### Chi tiết các trạng thái Giao diện:

1. **Trạng thái Mặc định (Idle / HomeView):**
   - Màn hình hiển thị **Automotive Dashboard**: Đồng hồ thời gian, vận tốc xe Ego, trạng thái kết nối mạng V2X (`V2X LINK: ACTIVE`), thông tin hệ thống.
   - Giúp IVI có giao diện hoàn chỉnh ngay khi vừa mở ứng dụng, thay vì màn hình trống/stub.

2. **Trạng thái Kích hoạt Cảnh báo (Event Trigger / WarningView):**
   - Ngay khi `R4ListenerService` nhận bản tin `R4WarningEvent` qua UDP từ ADA-ECU, `MainViewModel` lập tức ưu tiên chiếm quyền hiển thị (Wake-on-warning) với hiệu ứng chuyển cảnh mượt (`AnimatedContent` fade 200ms).
   - Màn hình hiển thị **`CanvasWarningView` (God-View 2D Renderer)**: Vẽ trực tiếp bằng Compose Canvas tọa độ các xe (Ego A, xe che khuất B, xe mù V2X C nhấp nháy đỏ theo mức độ rủi ro Risk State).

3. **Trạng thái Tự động Khôi phục (Clear / Restore):**
   - Nếu sau 10 giây không có bản tin `R4` mới (Timeout), hệ thống tự động trả giao diện về lại `HomeView`.

---

## 3. Lợi ích của Phương án Đề xuất

- **Đúng chuẩn Automotive HMI:** Mô phỏng chính xác tính năng cảnh báo an toàn trên các xe thương mại (chỉ hiện popup/screen khẩn cấp khi có nguy cơ va chạm).
- **Đáp ứng 100% Tiêu chuẩn Chấm điểm:** Chứng minh hoàn hảo tính năng R16 (chuyển đổi View theo state) và R17 (Canvas rendering + R19 Provenance Guard).
- **Hoàn toàn Độc lập & Ổn định (Deterministic):** Không phụ thuộc vào file video, phần cứng ngoài hay chất lượng mạng stream. Chạy mượt mà 60fps trên AAOS Emulator và thiết bị thật.
- **Dễ dàng Kiểm thử (Automated Unit/UI Test):** Toàn bộ flow đều có thể test tự động trong CI bằng JUnit & Compose Test framework.

---

## 4. Các bước triển khai tiếp theo (Priority 1)

1. **UI-1:** Dựng giao diện `HomeView` Automotive Dashboard.
2. **UI-2:** Wire StateFlow giữa `WarningViewModel` và `MainViewModel` để tự động đổi mode.
3. **UI-3:** Mount `CanvasWarningView` thực sự vào `WarningView` slot.
4. **UI-4:** Cập nhật trạng thái `V2X LINK` động trên thanh Bottom Bar.
