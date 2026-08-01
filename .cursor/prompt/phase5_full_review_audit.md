Hãy đóng vai trò là một Senior Android Automotive Developer / Technical Lead, tuân thủ nghiêm ngặt quy trình làm việc tại `@task-planning-conventions.md`. 

Hiện tại, nhánh phát triển `feat/phase5-ivi-hmi-dev` của chúng ta đã hoàn thành phần lớn logic cốt lõi của Phase 5 (HMI & God View) [2, 3]. Tuy nhiên, để đạt 100% Definition of Done (DoD) chuẩn bị cho Pull Request vào nhánh chính, chúng ta cần lấp đầy các khoảng trống kiểm thử và chuẩn bị tích hợp E2E [3, 4].

Hãy thực hiện quét toàn bộ codebase (bao gồm `/app/src/main` và `/app/src/test`) để thực hiện các nhiệm vụ sau:

### 1. Rà soát & Hiện thực hóa Ma trận Test Suite `17.5.4.2`
Đối chiếu codebase với yêu cầu ma trận test trong `phase5_tasks.md` [1]. Hãy kiểm tra xem các file test sau đã tồn tại và bao phủ đầy đủ các kịch bản chưa:
- **`R4DeserializerTest.kt`**: Đã test đủ 5 trường hợp bóc tách JSON (bao gồm cả trường hợp cảnh báo chưa biết `future_type` tự động degrade) chưa [1, 5, 6]?
- **`R4RepositoryTest.kt`**: Đã kiểm tra cơ chế định tuyến sự kiện và lưu trữ bản tin trạng thái cuối cùng (last-value-wins) chưa [1, 7, 8]?
- **`WarningViewModelTest.kt`**: Đã phủ các kịch bản chuyển đổi trạng thái Idle -> Active và tự động timeout chưa [1, 7, 8]?
- **`MainViewModelTest.kt`**: Xác nhận 3 kịch bản: tự động chuyển WarningView, tự động khôi phục mode cũ, và tôn trọng ý đồ override của người dùng [1, 9].
- **`SceneCoordinateMapperTest.kt`**: Kiểm tra toán học ánh xạ hệ tọa độ tương đối từ mét sang pixel Canvas, bao gồm cả tính năng kẹp biên (clamping) [1, 10, 11].
- **`CanvasWarningViewTest.kt`**: ĐÂY LÀ ĐIỂM QUAN TRỌNG. Kiểm tra xem đã có bộ test bằng Robolectric hoặc Compose Test để xác thực cơ chế bảo vệ nguồn dữ liệu (Defensive Source Guard) chưa [1, 12, 13]? Nếu phát hiện nguồn của xe Ghost C khác `"v2x_relayed"`, hệ thống phải render dấu chấm hỏi màu vàng `[?]` và ghi log lỗi [12, 13].

👉 **Yêu cầu:** Nếu phát hiện bất kỳ file test nào ở trên bị thiếu hoặc viết chưa đủ kịch bản, hãy tự động tạo mới hoặc cập nhật code test hoàn chỉnh. Chạy `./gradlew test` để đảm bảo tất cả đều xanh (GREEN).

### 2. Kiểm tra các điểm Robustness & Chất lượng mã nguồn (Code Quality Audit)
Xác nhận lại một lần nữa tính ổn định của ứng dụng nhúng:
- Socket UDP trong `R4ListenerService` đã có cơ chế đóng tường minh trong `onDestroy` để tránh nghẽn cổng nhận tin 5004 khi service khởi chạy lại chưa [14, 15]?
- Luồng vẽ giao diện trong `MainScreen` đã chuyển hoàn toàn sang sử dụng `collectAsStateWithLifecycle()` thay vì `collectAsState()` để tránh rò rỉ bộ nhớ hoặc lãng phí CPU/GPU khi chạy ẩn chưa?
- Việc gọi hiển thị God View đã được cô lập hoàn toàn qua interface `IviWarningViewSeam` (không gọi trực tiếp implementation `CanvasWarningView` hay banner overlay đã ẩn) đúng theo phản hồi của mentor chưa [16-18]?

### 3. Đánh giá Git Hygiene & File Prompt ghi nhớ
- Kiểm tra xem có file ghi vết prompt `.md` nào trong thư mục `.cursor/prompt/` đang bị bỏ sót ở trạng thái *untracked* không? Theo tài liệu contributor, toàn bộ thư mục `.cursor/` chứa prompt phải được commit để phục vụ chấm điểm [4].
- Hướng dẫn tôi tạo một commit tổng kết sạch sẽ nếu phát hiện thiếu sót.

### 4. Kết xuất Báo cáo (Audit Report Output)
Hãy xuất ra một báo cáo ngắn gọn chỉ rõ:
1. Những file test nào bạn vừa bổ sung/cập nhật để hoàn thiện ma trận `17.5.4.2` [1].
2. Kết quả chạy thử nghiệm test bộ nguồn local (`./gradlew test`).
3. Đánh giá độ sẵn sàng (Scale 1-10) để chính thức mở Pull Request