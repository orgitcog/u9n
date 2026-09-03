# cmake/toolchain-windows-clang-mingw.cmake
#
# Windows toolchain: LLVM clang++ targeting MinGW (x86_64-w64-mingw32) ABI
# with MSYS2 mingw64 as the sysroot.
#
# WHY THIS FILE EXISTS
# --------------------
# On Windows in a clean shell, no C++ compiler is on PATH.  This file lets
# cmake pick up the LLVM clang++ installed to the default LLVM location and
# cross-target the MinGW ABI so that the MSYS2 mingw64 headers and libraries
# are used.  The LLVM binary itself only needs standard Windows system DLLs
# (KERNEL32.dll, ADVAPI32.dll), so it can run from a clean shell with no
# extra environment setup.
#
# USAGE (root quality gate)
# -------------------------
#   cmake -S . -B build \
#         -DCMAKE_BUILD_TYPE=Release \
#         -DBUILD_TESTING=ON \
#         -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-windows-clang-mingw.cmake
#   cmake --build build --config Release -j
#   cd build && ctest --output-on-failure --parallel 2 -L unit -C Release
#
# ASSUMPTIONS
# -----------
#   * LLVM/Clang is installed at C:/Program Files/LLVM  (WinGet: LLVM.LLVM)
#   * MSYS2 + mingw64 is installed at C:/msys64         (WinGet: MSYS2.MSYS2)
#
# COMPATIBILITY
# -------------
# This file is only loaded when explicitly passed via -DCMAKE_TOOLCHAIN_FILE.
# It has no effect on Linux / macOS CI.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# --- Compilers ---------------------------------------------------------------
# LLVM clang++ only depends on standard Windows system DLLs: it can be invoked
# from a clean shell without MinGW/MSYS2 on PATH.
set(CMAKE_CXX_COMPILER "C:/Program Files/LLVM/bin/clang++.exe")
set(CMAKE_C_COMPILER   "C:/Program Files/LLVM/bin/clang.exe")

# --- Target triple and sysroot -----------------------------------------------
# Target MinGW ABI so that MinGW64 headers and static runtime archives are used.
set(CMAKE_CXX_FLAGS_INIT "--target=x86_64-w64-mingw32 --sysroot=C:/msys64/mingw64")
set(CMAKE_C_FLAGS_INIT   "--target=x86_64-w64-mingw32 --sysroot=C:/msys64/mingw64")
