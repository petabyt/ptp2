cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
project(ptp2)

set(CMAKE_C_FLAGS_DEBUG -g)
set(CMAKE_CXX_FLAGS_DEBUG -g)
set(CMAKE_C_FLAGS_RELWITHDEBINFO -g)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO -g)

find_package(PkgConfig)
if(PKG_CONFIG_FOUND AND NOT ANDROID)
    list(APPEND CMAKE_PREFIX_PATH "/usr/local")
    pkg_check_modules(LIBPAK IMPORTED_TARGET GLOBAL libpak)
    if(LIBPAK_FOUND)
        add_library(pakrt ALIAS PkgConfig::LIBPAK)
    endif()
endif()

set(PTP2_SRC_FILES
	ptp2_upstream/chdk.c
	ptp2_upstream/config.c
	ptp2_upstream/fujiptpip.c
	ptp2_upstream/library.c
	ptp2_upstream/n-param-test.c
	ptp2_upstream/olympus-wrap.c
	ptp2_upstream/ptp.c
	ptp2_upstream/ptpip.c
	ptp2_upstream/ptp-pack.c
	ptp2_upstream/usb.c
	module.c gphoto2.c
)

# n-param-test.c is a standalone test program with its own main(); the
# native test exe brings main.c instead (wasm/so builds don't care).
if(CMAKE_SYSTEM_PROCESSOR AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32" AND MAKE_EXECUTABLE)
	list(REMOVE_ITEM PTP2_SRC_FILES ptp2_upstream/n-param-test.c)
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32")
    set(WASM 1)
else()
    set(WASM 0)
endif()

if(MAKE_EXECUTABLE)
    if(WASM)
        add_executable(ptp2 ${PTP2_SRC_FILES})
        # pak_*/libusb_* imports are allowed via toolchain/symbols.txt (the
        # wasm runtime provides them); anything else fails at link time.
        # libusb.h lives in the wasm toolchain's include dir (pkg-config is
        # disabled by the toolchain). -idirafter keeps wasi-libc headers
        # (unistd.h etc.) winning over anything else in that directory.
        if(EXISTS /libpak/toolchain/include/libusb.h)
            # clang silently drops the "-idirafter=dir" joined form; keep
            # the flag and path as separate arguments
            target_compile_options(ptp2 PRIVATE "-idirafter" "/libpak/toolchain/include")
        elseif(EXISTS /usr/include/libusb-1.0/libusb.h)
            target_include_directories(ptp2 PRIVATE /usr/include/libusb-1.0)
        else()
            message(FATAL_ERROR "wasm build needs libusb.h (expected /libpak/toolchain/include or libusb-1.0 dev)")
        endif()
    else()
        add_executable(ptp2 ${PTP2_SRC_FILES} main.c)
    endif()
else()
    add_library(ptp2 SHARED ${PTP2_SRC_FILES})
endif()

target_compile_options(ptp2 PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/libgphoto2_port/gphoto2/gphoto2-port-log.h -D_GPHOTO2_INTERNAL_CODE=)
target_include_directories(ptp2 PUBLIC . libgphoto2_port/ ptp2_upstream/)

if(WASM)
    # pakrt (include dirs) comes from toolchain/pakrt.cmake or pkg-config
    if(TARGET pakrt)
        target_link_libraries(ptp2 PRIVATE pakrt)
    endif()
else()
    # USB transport: vcam's libusb wrapper for the in-process test harness
    # (-DVCAM_LIB=/path/to/libusb-vcam.so), real libusb-1.0 otherwise.
    # pak_* on the native test exe are stubbed in main.c (no libpak);
    # the libpak headers are still needed for runtime.h/wifi.h.
    if(EXISTS /libpak/include/runtime.h)
        target_include_directories(ptp2 PRIVATE /libpak/include)
    endif()
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBUSB libusb-1.0)
    endif()
    if(VCAM_LIB)
        target_link_libraries(ptp2 PRIVATE ${VCAM_LIB} m)
    elseif(LIBUSB_FOUND)
        target_link_libraries(ptp2 PRIVATE ${LIBUSB_LIBRARIES} m)
    endif()
    if(LIBUSB_FOUND)
        target_include_directories(ptp2 PRIVATE ${LIBUSB_INCLUDE_DIRS})
    elseif(EXISTS /libpak/src/android/libusb.h)
        # full libusb API header from libpak; no system libusb dev needed
        target_include_directories(ptp2 PRIVATE /libpak/src/android)
    endif()
    # Android's app build links libpak's usb-1.0 (libusb shim) itself.
    if(TARGET pakrt)
        target_link_libraries(ptp2 PRIVATE pakrt)
    endif()
endif()
