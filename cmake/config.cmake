# =============================================================================
#  wmpv 构建配置 —— 唯一的路径配置入口
# -----------------------------------------------------------------------------
#  由顶层 CMakeLists.txt 用 include() 引入（在 project() 之前）。
#  所有默认值都相对于工程根目录（本文件的上一级），不含任何机器相关的
#  绝对路径，因此工程可以整体移动到任意目录 / 盘符 / 机器。
#
#  默认依赖布局：
#      <parent>/wmpv/           本工程（cmake/config.cmake 所在处）
#      <parent>/mpv/            mpv 源码及其静态构建产物
#      <grandparent>/vcpkg/     vcpkg 静态 triplet
#
#  覆盖方式（任选其一）：
#    1) 命令行：
#         cmake -S . -B build -DWMPV_MPV_ROOT=D:/work/mpv -DWMPV_VCPKG_ROOT=D:/work/vcpkg
#       （build.bat 会把额外参数原样透传，可直接 build.bat -DWMPV_MPV_ROOT=...）
#    2) 本机 initial-cache 文件（推荐，避免每次敲 -D），例如 local.cmake：
#         set(WMPV_MPV_ROOT   "D:/work/mpv"   CACHE PATH "" FORCE)
#         set(WMPV_VCPKG_ROOT "D:/work/vcpkg" CACHE PATH "" FORCE)
#       然后：cmake -C local.cmake -S . -B build   或   build.bat -C local.cmake
#    3) cmake-gui / ccmake 中直接编辑下面的 CACHE 变量
# =============================================================================

# ------------------------------------------------------------------ 工程内路径
# CMAKE_CURRENT_LIST_DIR 指向本文件所在的 cmake/，与 include 它的位置无关
get_filename_component(WMPV_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(WMPV_SRC_DIR        "${WMPV_ROOT}/src"    CACHE PATH   "源码目录")
set(WMPV_ASSETS_DIR     "${WMPV_ROOT}/assets" CACHE PATH   "资源目录（图标等）")
set(WMPV_INSTALL_SUBDIR "install"             CACHE STRING "cmake --install 输出子目录（相对 prefix）")

# -------------------------------------------------------------------- 工具链
# 留空时按顺序探测：环境变量 WMPV_LLVM_BIN -> PATH 中的 clang-cl -> %VCINSTALLDIR%
set(WMPV_LLVM_BIN "" CACHE PATH "clang-cl.exe / lld-link.exe 所在目录；留空自动探测")

# ---------------------------------------------------------------- 第三方依赖根
set(WMPV_MPV_ROOT      "${WMPV_ROOT}/../mpv"      CACHE PATH   "mpv 源码/构建根目录")
set(WMPV_VCPKG_ROOT    "${WMPV_ROOT}/../../vcpkg" CACHE PATH   "vcpkg 根目录")
set(WMPV_VCPKG_TRIPLET "x64-windows-static"       CACHE STRING "vcpkg triplet")

# 规范化（消除路径里的 ..），让诊断信息更易读
foreach(_dep_root WMPV_MPV_ROOT WMPV_VCPKG_ROOT)
    get_filename_component(${_dep_root} "${${_dep_root}}" ABSOLUTE)
endforeach()

# ------------------------------------------------ 派生路径（留空则按根目录推导）
# 这些是普通变量，每次 configure 都会依据上面的 CACHE 值重新计算，
# 所以在 cmake-gui 里改根目录后无需清缓存即可生效。
set(WMPV_MPV_INCLUDE_DIR "" CACHE PATH   "mpv 头文件目录；留空则按 WMPV_MPV_ROOT 推导")
set(WMPV_MPV_LIB_DIR     "" CACHE PATH   "libmpv.a 所在目录；留空则按 WMPV_MPV_ROOT 推导")
set(WMPV_LIBPLACEBO_LIB  "" CACHE FILEPATH "libplacebo.a 路径；留空则按 WMPV_MPV_ROOT 推导")
set(WMPV_VCPKG_LIB_DIR   "" CACHE PATH   "vcpkg 静态库目录；留空则按 WMPV_VCPKG_ROOT + triplet 推导")

if(NOT WMPV_MPV_INCLUDE_DIR)
    set(WMPV_MPV_INCLUDE_DIR "${WMPV_MPV_ROOT}/install/include")
endif()
if(NOT WMPV_MPV_LIB_DIR)
    set(WMPV_MPV_LIB_DIR "${WMPV_MPV_ROOT}/install/lib")
endif()
if(NOT WMPV_LIBPLACEBO_LIB)
    set(WMPV_LIBPLACEBO_LIB "${WMPV_MPV_ROOT}/build/subprojects/libplacebo/src/libplacebo.a")
endif()
if(NOT WMPV_VCPKG_LIB_DIR)
    set(WMPV_VCPKG_LIB_DIR "${WMPV_VCPKG_ROOT}/installed/${WMPV_VCPKG_TRIPLET}/lib")
endif()

set(WMPV_MANIFEST "${WMPV_SRC_DIR}/wmpv.manifest")
set(WMPV_ICON     "${WMPV_ASSETS_DIR}/icon.ico")

# -------------------------------------------------- 需要链接的 vcpkg 静态库清单
# 顺序与 mpv.exe 自身的链接行一致；换 triplet / 库版本时只需改这里。
if(NOT DEFINED WMPV_DEP_LIBS)
    set(WMPV_DEP_LIBS
        ass harfbuzz fribidi freetype bz2 libpng16 zlib
        brotlidec brotlicommon
        avcodec swresample avutil avfilter swscale avformat
        lcms2 avdevice lua51 jpeg
        shaderc glslang SPIRV-Tools-opt SPIRV-Tools shaderc_util
        spirv-cross-c spirv-cross-core spirv-cross-glsl spirv-cross-hlsl
        spirv-cross-util spirv-cross-cpp spirv-cross-msl spirv-cross-reflect
        vulkan-1
    )
endif()
