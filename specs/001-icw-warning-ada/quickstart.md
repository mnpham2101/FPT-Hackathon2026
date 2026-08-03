# Quickstart & Testing Guide: ICW Handling in ADA_ECU

## Prerequisites
- CMake 3.22+
- C++17 compliant compiler (g++ 11+ or clang 13+)

---

## 🛠️ Build Steps

1. Navigate to the `ADA_ECU` directory:
   ```bash
   cd ADA_ECU
   ```

2. Create build directory and compile:
   ```bash
   cmake -B build -S .
   cmake --build build -j$(nproc)
   ```

---

## 🧪 Running Unit Tests

Run the GoogleTest suite for ICW risk evaluation and track coasting:

```bash
ctest --test-dir build --output-on-failure
```

Or execute the specific test target directly:

```bash
./build/ada_icw_evaluator_test
```

---

## 🔍 Manual Verification

Validate configuration parsing and risk matrix loading:

```bash
./build/ada_icw_evaluator_test --gtest_filter="ICWRiskConfigTest.*"
```
