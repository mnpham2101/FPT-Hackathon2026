# Sổ ghi nhận tính năng tương lai — Nhận thức phương tiện hợp tác (không thuộc phạm vi M1)

> Bản dịch tiếng Việt của [m1-future-features-register.md](m1-future-features-register.md) dành cho lập trình viên Việt Nam — khi có mâu thuẫn, bản tiếng Anh và báo cáo gốc thắng.

> Bản đối chiếu (mirror) của [m1-cooperative-awareness.md § Future developments](../m1-cooperative-awareness.md#future-developments) — danh sách tính năng tương lai có thẩm quyền; khi có mâu thuẫn, báo cáo thắng, và mọi thay đổi phải vào báo cáo trước. Các tham chiếu Rx trỏ tới §2 của báo cáo (R1–R19). Không mục nào ở đây thuộc phạm vi M1; mỗi mục nêu rõ điểm nối (seam) của M1 mà nó mở rộng.

- **Kịch bản cảnh báo dạng mô-đun.** Các kịch bản cảnh báo tương lai (nguy cơ tại giao lộ, điểm mù khúc cua) cắm vào dưới dạng hiện thực hóa (realization) mới của lớp trừu tượng Đánh giá rủi ro va chạm (Collision Risk Assessment, R14) mà không phải sửa lại mã hiện có.
- **Cảnh báo nhường đường cho xe ưu tiên.** Cảnh báo khi xe khẩn cấp/xe ưu tiên đang tiếp cận để tài xế nhường đường — một hiện thực hóa R14 mới, được vẽ là một realization tương lai trong [ada-ecu.svg](../ada-ecu.svg).
- **Đánh giá rủi ro va chạm theo thang tốc độ.** Tiêu chí khoảng cách tiếp nhận/rủi ro (R13) tỉ lệ theo tốc độ giao thông thay vì ngưỡng cố định của M1.
- **Luồng camera trực tiếp trong vùng Display của IVI.** Vùng Display (R16) hiển thị khung hình camera trực tiếp; video trực tiếp cũng cấp đầu vào cho thuật toán phát hiện vật cản của ADA.
- **Phát hiện đối tượng trực tiếp ở tốc độ cao.** Xe B chạy tới 120 km/h (33.3 m/s) và phải phát hiện vật cản trong phạm vi quãng đường dừng + quãng đường phát tin. Đặc tả suy ra:
  - **Tầm phát hiện, toàn bộ là giả định (A):** phát hiện một ô tô ở ≥ 130–150 m đường khô / ≥ 190 m đường ướt — quãng phanh 79 m khô (a = 7 m/s²) / 139 m ướt (a = 4 m/s²) + phản xạ tài xế 1.0–1.5 s ≈ 33–50 m + biên ~10 m cho ngân sách phát tin đầu-cuối ~300 ms (giả định (A)); biến thể phanh tự động (bỏ thành phần phản xạ) hạ yêu cầu xuống ≥ 100 m khô / ≥ 160 m ướt.
  - **HFOV của camera là một giả định (A) tường minh** (60° so với 90° làm số pixel một vật thể chiếm thay đổi ~1.5×) — chốt lại khi có spec camera.
  - **Ràng buộc quyết định là tầm phát hiện (độ phân giải đầu vào), không phải FPS:** suy luận (inference) ở 640 px, một ô tô 1.8 m chỉ chiếm ~10–12 px ở 100 m — dưới sàn tin cậy ~20–30 px của nano-detector (COCO "small" < 32² px) ⇒ cần suy luận ≥ 1280–1920 px ở ≥ 10 Hz.
  - **Suy luận trên CPU thất bại hoàn toàn** (≈ 224 ms @ 1280 / ≈ 505 ms @ 1920) ⇒ **bắt buộc có tăng tốc cấp GPU** (tham chiếu: T4 TensorRT ≈ 1.5 ms @ 640, mở rộng lên ≈ 6–14 ms). Độ mở của stack tăng tốc (ứng viên mã nguồn mở: ROCm, OpenVINO, ONNX Runtime) là quyết định của người dùng tại milestone đó — M1 không mang phụ thuộc GPU nào (§4).
- **Phát video clip ego trên IVI.** Clip góc nhìn ego được cung cấp (B che khuất tầm nhìn phía trước) phát trong vùng Display (R16) — phương pháp "video feed" trong bảng demo §1 của báo cáo; một bề mặt hiển thị bổ sung, không phải làm lại.
- **Chuyển đổi góc nhìn 2D ⇄ 3D.** Nút gạt cho người dùng chuyển giữa góc nhìn 2D và God view 3D tùy chọn, nằm sau seam view của R17 (SceneView/Filament là stack 3D tùy chọn).
- **Đa tiến trình, đánh thức khi có cảnh báo.** Một ứng dụng riêng đang ngủ được đánh thức và vẽ khung cảnh khi có sự kiện ADA; đường đa tiến trình tùy chọn của M1 (R16) là nền móng.
- **Tùy biến giao diện / theme.** Avatar phương tiện có thể được tùy biến.
- **Tiêu chí benchmark Fast-UI + C2X.** Hành vi UI cực nhanh chạy song song các dịch vụ C2X — ví dụ một ứng dụng media ở foreground cùng lúc với hiển thị 2D/3D hoặc thông báo về xe C / tin nhắn V2X tới; các ngưỡng benchmark có thể thương lượng theo tính khả thi kỹ thuật.
- **Phát hiện nhiều vật cản khuất.** ADA phát hiện và theo dõi đồng thời nhiều đối tượng ngoài tầm nhìn thẳng (M1 chỉ xử lý một đối tượng).
- **Gộp thành một tin nhắn duy nhất.** Nhiều vật cản phát hiện được gộp vào một tin nhắn V2X — không gây bão quảng bá (broadcast storm).
- **Giảm tin nhắn theo lựa chọn người dùng.** Người dùng có thể chọn nhận ít tin nhắn V2X hơn.
- **Lọc theo mức nghiêm trọng.** Người dùng có thể chọn chỉ nhận cảnh báo từ một mức nghiêm trọng (criticality) trở lên; mức nghiêm trọng được tra từ trường `warningType` của tin nhắn ADA→IVI (R4).
- **Các loại cảnh báo nguy cơ khác.** Đường trơn, đá rơi, ổ gà, tình trạng mặt đường, có trẻ em, cảnh sát, giới hạn tốc độ, cấm bấm còi/luật đường bộ khác, tình hình giao thông — được mang bởi DENM (vị trí sự kiện + mã nguyên nhân), họ tin nhắn được chỉ định cho các loại này (ghi chú R1).
- **Phân phối theo loại tin nhắn V2X mở rộng được.** Pipeline Rx (R9) phân phối (dispatch) theo loại tin nhắn, nên họ tin nhắn mới chỉ cần thêm một module codec cộng một mục dispatch — M1 chỉ giải mã CPM (R1).
- **Lệnh tới các ECU khác.** Tầng đầu ra của ADA mở rộng thành đầu ra lệnh/chấp hành tới các ECU/phần cứng khác; M1 chỉ hiện thực snapshot lưu trữ R10 gửi tới V2X ECU.
