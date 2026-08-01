Hãy thực hiện tuần tự các bước sau đây trực tiếp trên dự án:

### BƯỚC 1: Dọn dẹp Git & Chuẩn bị lệnh Push nhánh
1. Kiểm tra trạng thái Git hiện tại (`git status`). Phát hiện toàn bộ các file ghi vết câu lệnh trong thư mục `.cursor/prompt/` đang ở trạng thái chưa được theo dõi (untracked) hoặc chưa commit.
2. Tạo đúng 1 commit phụ (chore commit) để lưu trữ tất cả tài liệu prompt này vào lịch sử Git:
   - Stage toàn bộ thư mục `.cursor/` (và các file `.vscode/settings.json` nếu có thay đổi cấu hình).
   - Thực hiện commit với thông điệp chuẩn chỉ: `chore: commit and archive all Phase 5 AI prompt records for auditing`
3. Cung cấp cho tôi các câu lệnh Git chuẩn xác để tôi gõ vào terminal nhằm push nhánh `feat/phase5-ivi-hmi-dev` này lên remote repository an toàn.

### BƯỚC 2: Viết Báo Cáo Nghiệm Thu Phase 5 (`IVI_ECU/deployment/phase5_completion_report.md`)
Hãy tạo mới một tài liệu Markdown cực kỳ chuyên nghiệp tại đường dẫn `IVI_ECU/deployment/phase5_completion_report.md` để trình bày cho Ban giám khảo và Mentor. Nội dung báo cáo phải bám sát cấu trúc kỹ thuật thực tế đã làm, bao gồm các phần:

1. **Executive Summary (Tóm tắt dự án):**
   - Khẳng định ứng dụng IVI HMI trên nền tảng Android Automotive OS (AAOS) đã đạt trạng thái **"Kiến trúc hoàn chỉnh" (Architecture-Complete)** và **"Sẵn sàng tích hợp" (Integration-Capable)** cho Milestone 1.
   - Dung lượng APK tối ưu cực sâu: **~24.5 MB** (vượt xa ngân sách yêu cầu < 50 MB của BTC).

2. **Kiến Trúc Hệ Thống & Luồng Dữ Liệu R4 (Task Group 5.1):**
   - **Tầng dữ liệu kiểu mạnh:** `R4Message.kt` (sealed class gồm `R4WarningEvent` và `R4StateMessage`).
   - **Cơ chế bóc tách an toàn (Lenient Deserializer):** Giải pháp xử lý sai số cấu trúc trong `R4Deserializer.kt`. Cơ chế tự động hạ cấp (degrade) an toàn khi gặp `warningType` lạ sang `"unknown"` mà không gây crash app.
   - **Dịch vụ mạng UDP nền:** `R4ListenerService.kt` chạy trên `Dispatchers.IO`, tự động mở cổng 5004 nhận gói tin JSON và phát thông lượng qua `SharedFlow`.
   - **Cầu nối dữ liệu:** `R4Repository.kt` thu thập sự kiện dạng *last-value-wins* và `WarningViewModel.kt` quản lý trạng thái hiển thị kèm tính năng tự động tắt cảnh báo (timeout) bảo vệ tài nguyên.

3. **Giao Diện HMI & Logic Tự Động Chuyển Màn (Task Group 5.2):**
   - Cấu trúc Main HMI (`MainScreen.kt`) chia tỉ lệ chuẩn AAOS (70% Display Area ở giữa, xung quanh là dải nút điều hướng Home/Apps/Settings).
   - Cơ chế **Wake-on-Warning Auto-Switch** (`MainViewModel.kt`): Tự động ép chuyển vùng hiển thị sang `WarningView` khi có cảnh báo nguy hiểm, tự động khôi phục màn hình cũ (`previousMode`) khi hết hiểm họa, và tôn trọng tuyệt đối ý đồ chuyển hướng thủ công của tài xế (`userOverrodeDuringWarning`).

4. **Bản Đồ 2D God View Canvas (Task Group 5.3):**
   - **Tách biệt hiển thị (View Seam):** Sử dụng interface `IviWarningViewSeam` giúp cô lập lõi render và dễ dàng tráo đổi giữa 2D Canvas và 3D SceneView trong tương lai.
   - **Toán học tọa độ tương đối:** Lớp `SceneCoordinateMapper.kt` (pure Kotlin math) ánh xạ khoảng cách mét sang tọa độ pixel trên màn hình cực kỳ chuẩn xác, hỗ trợ kẹp biên (clamping) khi xe ngoài phạm vi hiển thị.
   - **Đồ họa Canvas mượt mà:** Vẽ Ego (Cyan), xe B (Amber), xe Ghost C (đường viền đứt nét màu đỏ với hiệu ứng vòng tròn pulsing alpha lặp vô hạn).
   - **Lá chắn bảo vệ nguồn (Defensive Source Guard):** Trái tim bảo mật thông tin. Nếu xe Ghost C có thuộc tính `source` khác `"v2x_relayed"`, hệ thống lập tức chặn đứng việc render bình thường, chuyển sang vẽ vòng tròn màu vàng dấu chấm hỏi `[?] UNKNOWN SOURCE` để chống giả mạo tín hiệu và ghi log lỗi hệ thống.
   - **Quyết định thiết kế theo feedback:** Xác nhận việc ẩn hoàn toàn thanh `WarningBannerOverlay` khỏi giao diện chính để tối ưu không gian hiển thị cho God View theo đúng góp ý của Mentor ngày 26-07-2026.

5. **Giải Pháp Tích Hợp & Chẩn Đoán Sức Khỏe Hệ Thống (Diagnostics & Integration):**
   - Ráp nối toàn bộ hệ thống thông qua Dagger Hilt DI (`AppModule.kt`, `@AndroidEntryPoint` trên `MainActivity` và `R4ListenerService`).
   - **Giải phóng tài nguyên nhúng:** Cơ chế đóng và hủy liên kết `DatagramSocket` tường minh trong hàm `onDestroy()` của service nền để tránh lỗi treo/nghẽn cổng 5004.
   - **Tiết kiệm điện năng CPU/GPU:** Sử dụng `collectAsStateWithLifecycle()` giúp dừng thu thập dữ liệu cảnh báo và dừng render vẽ canvas khi app chạy ẩn dưới nền.

6. **Bảng Thống Kê Ma Trận Kiểm Thử (Task Group 5.4 - Test Matrix):**
   - Liệt kê đầy đủ danh sách các file test đã chạy xanh (GREEN) bao phủ toàn bộ các kịch bản logic cốt lõi:
     - `R4DeserializerTest.kt` (5 kịch bản parse JSON)
     - `R4RepositoryTest.kt`
     - `WarningViewModelTest.kt`
     - `MainViewModelTest.kt` (Auto-switch, Auto-restore, User override)
     - `FullStackIntegrationTest.kt` (Robolectric + Hilt test giả lập UDP loopback)

7. **Kế Hoạch Tiếp Theo (Next Steps for Phase 6 Integration):**
   - Thực hiện Smoke Test thực tế trên thiết bị xe ảo Skycraft của nền tảng CarSky bằng cách cài đặt APK và sử dụng Python Mock Sender bắn UDP.
   - Kiểm tra đóng băng hợp đồng dữ liệu (Contract Freeze) cổng mạng 5004 với V2X ECU và ADA ECU.
   - Gỡ bỏ mock-sender, nối dây trực tiếp để HMI tiêu thụ luồng dữ liệu thật của ADA ECU.

### BƯỚC 3: Tạo Commit Cho Báo Cáo Nghiệm Thu
Sau khi tạo xong file báo cáo `phase5_completion_report.md` trên, hãy tự động stage file này và tạo đúng 1 commit với thông điệp:
`[16.5.2.5] docs: add Phase 5 IVI HMI and God-View completion report`