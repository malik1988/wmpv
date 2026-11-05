cmake_minimum_required(VERSION 3.20)

# === Configuration ===========================================================
# All machine-specific paths live in cmake/config.cmake. Its defaults are
# relative to this source tree, so the project can be moved/copied anywhere.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.cmake")

# === Toolchain ===============================================================
# Build with clang-cl + lld-link (the same LLVM toolchain mpv itself was built
# with). This lets us statically link libmpv.a, which is a GNU `ar` archive of
# COFF objects -- MSVC's link.exe cannot read that format, but lld-link can.
# The compiler is resolved from WMPV_LLVM_BIN, the environment or PATH; no
# absolute Visual Studio path is baked in.
if(NOT CMAKE_CXX_COMPILER)
    set(WMPV_CLANG_CL "")
    if(WMPV_LLVM_BIN)
        get_filename_component(_llvm_bin "${WMPV_LLVM_BIN}" ABSOLUTE)
        set(WMPV_CLANG_CL "${_llvm_bin}/clang-cl.exe")
    elseif(DEFINED ENV{WMPV_LLVM_BIN})
        file(TO_CMAKE_PATH "$ENV{WMPV_LLVM_BIN}" _llvm_bin)
        set(WMPV_CLANG_CL "${_llvm_bin}/clang-cl.exe")
    else()
        find_program(WMPV_CLANG_CL_EXECUTABLE NAMES clang-cl clang-cl.exe)
        if(WMPV_CLANG_CL_EXECUTABLE)
            set(WMPV_CLANG_CL "${WMPV_CLANG_CL_EXECUTABLE}")
        elseif(DEFINED ENV{VCINSTALLDIR})
            file(TO_CMAKE_PATH "$ENV{VCINSTALLDIR}" _vc_dir)
            set(WMPV_CLANG_CL "${_vc_dir}/Tools/Llvm/x64/bin/clang-cl.exe")
        endif()
    endif()
    if(NOT WMPV_CLANG_CL OR NOT EXISTS "${WMPV_CLANG_CL}")
        message(FATAL_ERROR
            "clang-cl not found. Set -DWMPV_LLVM_BIN=<dir with clang-cl.exe>, "
            "or add LLVM's bin directory to PATH.")
    endif()
    set(CMAKE_C_COMPILER   "${WMPV_CLANG_CL}")
    set(CMAKE_CXX_COMPILER "${WMPV_CLANG_CL}")
endif()

project(wmpv LANGUAGES CXX)

# Drive the link through clang-cl with lld-link so we can consume the GNU `ar`
# archives (libmpv.a / libplacebo.a). CMake's default MSVC path goes through
# vs_link_exe -> MSVC link.exe, which cannot read GNU archives. The -fuse-ld
# flag belongs here (before /link) so it reaches the clang driver instead of
# being forwarded to lld-link itself.
set(CMAKE_CXX_LINK_EXECUTABLE
    "\"${CMAKE_CXX_COMPILER}\" /nologo -fuse-ld=lld-link <OBJECTS> /link <LINK_FLAGS> /out:<TARGET> <LINK_LIBRARIES>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# mpv was built with the static MSVC runtime (-fms-runtime-lib=static). wmpv must
# match or we get duplicate/conflicting CRT symbols. Force static CRT for all
# configs (so even a Debug build uses /MT, not /MTd).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# === Dependencies ============================================================
# Fail early with an actionable message instead of a wall of linker errors when
# the project is built on a machine with a different dependency layout.
if(NOT EXISTS "${WMPV_MPV_INCLUDE_DIR}/mpv/client.h")
    message(FATAL_ERROR
        "mpv headers not found at ${WMPV_MPV_INCLUDE_DIR}/mpv/client.h\n"
        "Set -DWMPV_MPV_ROOT=<path> (or -DWMPV_MPV_INCLUDE_DIR=<path>).")
endif()

# mpv statically bundles FFmpeg/libass/etc., but libmpv.a only contains mpv's
# own objects; the dependency static libs must be linked explicitly. This list
# mirrors mpv.exe's own link line (from mpv's build.ninja).
set(WMPV_STATIC_ARCHIVES
    "${WMPV_MPV_LIB_DIR}/libmpv.a"
    "${WMPV_LIBPLACEBO_LIB}"
)
foreach(_dep IN LISTS WMPV_DEP_LIBS)
    list(APPEND WMPV_STATIC_ARCHIVES "${WMPV_VCPKG_LIB_DIR}/${_dep}.lib")
endforeach()

foreach(_archive IN LISTS WMPV_STATIC_ARCHIVES)
    if(NOT EXISTS "${_archive}")
        message(FATAL_ERROR
            "Missing static library: ${_archive}\n"
            "Check WMPV_MPV_ROOT / WMPV_VCPKG_ROOT / WMPV_VCPKG_TRIPLET in cmake/config.cmake.")
    endif()
endforeach()

# === Target ===================================================================
set(WMPV_SOURCES
    "${WMPV_SRC_DIR}/main.cpp"
    "${WMPV_SRC_DIR}/player.cpp"
    "${WMPV_SRC_DIR}/ui.cpp"
    "${WMPV_SRC_DIR}/util.cpp"
    "${WMPV_SRC_DIR}/wmpv.rc"
)

add_executable(wmpv WIN32 ${WMPV_SOURCES})

# MSVC 的 rc.exe 不输出依赖信息；显式声明图标依赖，这样改 assets/icon.ico
# 后 Ninja 才会重新编译 .res 并刷新 exe / 窗口标题栏的内嵌图标。
set_property(SOURCE "${WMPV_SRC_DIR}/wmpv.rc" APPEND PROPERTY OBJECT_DEPENDS "${WMPV_ICON}")

target_include_directories(wmpv PRIVATE
    "${WMPV_SRC_DIR}"
    "${WMPV_MPV_INCLUDE_DIR}"
)

target_compile_definitions(wmpv PRIVATE UNICODE _UNICODE)

target_compile_options(wmpv PRIVATE /utf-8 /EHsc /MT /O2)

# === Static linking of mpv + all its dependencies ============================
# Link the mpv/placebo GNU `ar` archives together with all dependency static
# libs, in dependency order (this mirrors mpv.exe's own link line, which does
# NOT use /WHOLEARCHIVE -- that flag would (a) duplicate symbols inside FFmpeg
# and (b) force-include the .res members that lld-link cannot consume). libmpv.a
# is listed again at the end so any internal circular references within mpv are
# resolved by a second pass. The .res members are unreferenced, so they are
# skipped automatically.
target_link_libraries(wmpv PRIVATE
    ${WMPV_STATIC_ARCHIVES}
    "${WMPV_MPV_LIB_DIR}/libmpv.a"
)

target_link_libraries(wmpv PRIVATE
    # system / OS import libraries
    user32 gdi32 dwmapi shell32 comdlg32 advapi32 gdiplus
    ole32 oleaut32 uuid ws2_32 bcrypt crypt32 ncrypt secur32
    mfuuid strmiids avrt imm32 ntdll pathcch shcore uxtheme version
    runtimeobject psapi shlwapi opengl32 kernel32 winspool
    vfw32 winmm
)

# Embed the application manifest (DPI / theme / Windows 11 styling) via the
# linker (lld-link supports the MSVC manifest flags).
target_link_options(wmpv PRIVATE
    "/MANIFEST:EMBED"
    "/MANIFESTINPUT:${WMPV_MANIFEST}"
)

# === Installation =============================================================
# `cmake --install` copies the built executable -- plus the icon -- into
# <prefix>/<WMPV_INSTALL_SUBDIR>. The .exe is fully self-contained (mpv is
# statically linked), so no other files are required.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}" CACHE PATH "Install prefix" FORCE)
endif()

install(TARGETS wmpv RUNTIME DESTINATION "${WMPV_INSTALL_SUBDIR}")
install(FILES "${WMPV_ICON}" DESTINATION "${WMPV_INSTALL_SUBDIR}" OPTIONAL)
