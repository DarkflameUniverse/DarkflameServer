# libdlu_embree_sycl: Embree 4 with its SYCL (Intel GPU) support and the UGC server's GPU ray kernels, for
# ray_backend=embree-gpu. A project of its own, built by a SYCL compiler (Intel oneAPI DPC++'s icpx, or the open source
# DPC++'s clang++) as an external project of the servers' build (DLU_EMBREE_SYCL), so the rest never needs one. Embree
# is linked into it statically and bound inside it (-Bsymbolic, none of its symbols exported), so it never meets the
# servers' own Embree (CPU), whose symbols have the same names.
cmake_minimum_required(VERSION 3.25)
project(dlu_embree_sycl CXX)

include(FetchContent)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_TESTING OFF)
set(EMBREE_SYCL_SUPPORT ON)
set(EMBREE_SYCL_AOT_DEVICES "none") # the kernels are compiled for the GPU found when first used
set(EMBREE_STATIC_LIB ON)
set(EMBREE_TASKING_SYSTEM "INTERNAL")
set(EMBREE_ISPC_SUPPORT OFF)
set(EMBREE_TUTORIALS OFF)
set(EMBREE_ZIP_MODE OFF)
set(EMBREE_INSTALL_DEPENDENCIES OFF)
set(EMBREE_FILTER_FUNCTION OFF)
set(EMBREE_RAY_MASK OFF)
set(EMBREE_RAY_PACKETS OFF)
set(EMBREE_BACKFACE_CULLING OFF)
set(EMBREE_GEOMETRY_TRIANGLE ON)
foreach(geometry QUAD CURVE SUBDIVISION USER INSTANCE INSTANCE_ARRAY GRID POINT)
	set(EMBREE_GEOMETRY_${geometry} OFF)
endforeach()
set(EMBREE_MAX_ISA "NONE")
set(EMBREE_ISA_SSE2 ON)
set(EMBREE_ISA_SSE42 OFF)
set(EMBREE_ISA_AVX OFF)
set(EMBREE_ISA_AVX2 OFF)
set(EMBREE_ISA_AVX512 OFF)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

FetchContent_Declare(
	embree
	GIT_REPOSITORY https://github.com/RenderKit/embree.git
	GIT_TAG ff9381774dc99fea81a932ad276677aad6a3d4dd #refs/tags/v4.4.0
	GIT_PROGRESS TRUE
	GIT_SHALLOW 1
)
FetchContent_MakeAvailable(embree)

add_library(dlu_embree_sycl SHARED UgcEmbreeSycl.cpp)
target_compile_options(dlu_embree_sycl PRIVATE -fsycl -Xclang -fsycl-allow-func-ptr -fsycl-targets=spir64)
target_link_options(dlu_embree_sycl PRIVATE -fsycl -fsycl-targets=spir64 -Wl,-Bsymbolic -Wl,--exclude-libs,ALL)
target_link_libraries(dlu_embree_sycl PRIVATE embree ze_wrapper ${CMAKE_DL_LIBS})
# Only the C interface is seen from outside
set_target_properties(dlu_embree_sycl PROPERTIES CXX_VISIBILITY_PRESET hidden BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")

install(TARGETS dlu_embree_sycl LIBRARY DESTINATION . RUNTIME DESTINATION .)
