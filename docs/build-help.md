
##  MiniExchange — CMake Build Guide

This document explains how to configure, build, and test the **MiniExchangeV2** project using CMake.
It also lists all available options and recommended command-line invocations.

---

### ️ 1. Requirements

| Dependency       | Version               | Notes                            |
| ---------------- | --------------------- | -------------------------------- |
| **CMake**        | ≥ 3.16                | Required                         |
| **C++ Compiler** | GCC ≥ 12 / Clang ≥ 15 | C++23 support required           |
| **Python**       | 3.12+                 | For `miniexchange_client` module |
| **pybind11**     | Latest                | Found automatically by CMake     |
| **OpenSSL**      | 1.1+                  | Required for secure components   |

---

###  2. Build Types

CMake supports several **build types** that control optimization and debug info.
You specify one via `-DCMAKE_BUILD_TYPE=<type>`.

| Build Type         | Description                         | Optimization | Debug Info | Typical Use                |
| ------------------ | ----------------------------------- | ------------ | ---------- | -------------------------- |
| **Debug**          | No optimization, full debug symbols | `-O0`        | ✅          | Development, unit testing  |
| **Release**        | Full optimization, no debug info    | `-O3`        | ❌          | Production binaries        |
| **RelWithDebInfo** | Optimized build with symbols        | `-O2`        | ✅          | Profiling, perf analysis   |

If you don’t specify one, the default is **Debug**.

---

###  3. CMake Options

You can toggle these at configuration time using `-DOPTION=ON` or `-DOPTION=OFF`.

| Option             | Default | Description                                                                      |
| ------------------ | ------- | -------------------------------------------------------------------------------- |
| `ENABLE_ASAN`      | `OFF`   | Enables **AddressSanitizer** (`-fsanitize=address`) for detecting memory errors. |
| `ENABLE_LTO`       | `OFF`   | Enables **Link Time Optimization (IPO)** for faster and smaller release builds.  |
| `BUILD_TESTING`    | `ON`    | Enables building unit tests via `CTest`.                                         |
| `CMAKE_BUILD_TYPE` | `Debug` | Controls the optimization/debug level.                                           |

---

### ️ 4. Example Build Commands

####  Debug build (default)

```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
```

 Fast to compile, best for debugging and development.

---

####  Release build (optimized)

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
```

 Fully optimized with `-O3 -march=native -DNDEBUG`.

---

####  Release with debug info

```bash
cmake -B build-relwithdeb -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-relwithdeb -j
```

 Great for profiling — contains symbols but remains optimized.

---

####  AddressSanitizer (ASAN)

```bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build-asan -j
```

Detects memory leaks and buffer overflows.
Do **not** use for production binaries (slower runtime).

---

####  Release with LTO (Link Time Optimization)

```bash
cmake -B build-lto -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=ON
cmake --build build-lto -j
```

Enables whole-program optimization for 5–15 % performance gain.
 Linking takes slightly longer.

---

####  LTO + Debug Info

```bash
cmake -B build-rel-lto -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_LTO=ON
cmake --build build-rel-lto -j
```

 Balanced choice for performance + profiling.

---

####  ASAN + LTO (advanced)

```bash
cmake -B build-asan-lto -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_ASAN=ON -DENABLE_LTO=ON
cmake --build build-asan-lto -j
```

️ Supported on Clang, may not work reliably on GCC.

---

###  5. Running Tests

If you built with `-DBUILD_TESTING=ON` (default), you can run:

```bash
cd build-debug   # or any other build directory
ctest --output-on-failure
```

---

### 6. Python Module (`miniexchange_client`)

The CMake build automatically compiles the **pybind11** module.
The resulting `.so` or `.pyd` file will be placed in:

```
<build-dir>/py/miniexchange_client.*.so
```

You can import it from Python:

```python
import sys
sys.path.append("build-debug/py")

import miniexchange_client
client = miniexchange_client.MiniExchangeClient(...)
```

---

###  7. Common Build Outputs

| Component               | Type           | Output Path                            |
| ----------------------- | -------------- | -------------------------------------- |
| **MiniExchangeCore**    | Static library | `<build-dir>/libMiniExchangeCore.a`    |
| **MiniExchangeClient**  | Static library | `<build-dir>/libMiniExchangeClient.a`  |
| **MiniExchange**        | Executable     | `<build-dir>/MiniExchange`             |
| **miniexchange_client** | Python module  | `<build-dir>/py/miniexchange_client.*` |

---


### 8. Summary Table

| Build Type           | Example Command                                                                              |
| -------------------- | -------------------------------------------------------------------------------------------- |
| Debug                 |`cmake -B build-debug`                                                                       |
| Debug + ASAN          |`cmake -B build-asan -DENABLE_ASAN=ON`                                                       |
| Release               |`cmake -B build-release -DCMAKE_BUILD_TYPE=Release`                                          |
| Release + LTO         |`cmake -B build-lto -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=ON`                              |
| RelWithDebInfo        |`cmake -B build-rel -DCMAKE_BUILD_TYPE=RelWithDebInfo`                                       |
| RelWithDebInfo + LTO  |`cmake -B build-rel-lto -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_LTO=ON`                   |
| ASAN + LTO            |`cmake -B build-asan-lto -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_ASAN=ON -DENABLE_LTO=ON` |

