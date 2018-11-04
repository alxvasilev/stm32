This sysroot directory contains a minimal abstraction layer and a linker script,
which are used only if libopencm3 is not used. When using libopencm3, these are not
used or needed. However, the sysroot directory is still passed to CMake for the
CMAKE_FIND_ROOT_PATH, so it can be used for standard libraries and headers.

