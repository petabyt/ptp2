cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
project(ptp2)

set(CMAKE_C_FLAGS_DEBUG -g)
set(CMAKE_CXX_FLAGS_DEBUG -g)
set(CMAKE_C_FLAGS_RELWITHDEBINFO -g)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO -g)

# find_package(PkgConfig)
# if(PKG_CONFIG_FOUND AND NOT ANDROID)
#     list(APPEND CMAKE_PREFIX_PATH "/usr/local")
#     pkg_check_modules(LIBPAK IMPORTED_TARGET GLOBAL libpak)
#     if(LIBPAK_FOUND)
#         add_library(pak ALIAS PkgConfig::LIBPAK)
#     endif()
# endif()

add_library(ptp2 SHARED
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

target_compile_options(ptp2 PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/libgphoto2_port/gphoto2/gphoto2-port-log.h -D_GPHOTO2_INTERNAL_CODE=)
target_link_libraries(ptp2 PRIVATE pakrt)
target_include_directories(ptp2 PUBLIC . libgphoto2_port/ ptp2_upstream/ /usr/local/include/libpak)

if(NOT LINUX)
	add_executable(test main.c)
	target_link_libraries(test PRIVATE ptp2 m pakrt)
endif()
