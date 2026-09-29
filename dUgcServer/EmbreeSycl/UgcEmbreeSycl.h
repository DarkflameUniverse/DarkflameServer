#pragma once

#include <cstdint>

/**
 * The plain C interface of libdlu_embree_sycl (built with DLU_EMBREE_SYCL by a SYCL compiler, loaded by the UGC server
 * when ray_backend=embree-gpu): Embree 4 on an Intel GPU (Arc, Xe) through SYCL. Rays are 48 bytes and hits 16, as
 * UgcRays::Ray and UgcRays::Hit. Calls are made by one thread at a time.
 */
#if defined(_WIN32)
#define DLU_EMBREE_SYCL_API __declspec(dllexport)
#else
#define DLU_EMBREE_SYCL_API __attribute__((visibility("default")))
#endif

extern "C" {
	// Picks the `index`-th SYCL GPU Embree supports and sets it up; 0 when it can, else writes why into `problem`
	DLU_EMBREE_SYCL_API int DluEmbreeSyclInit(int index, char* problem, int problemSize);
	// The mesh on the GPU (positions 3 floats a vertex, indices 3 a triangle); null when it fails
	DLU_EMBREE_SYCL_API void* DluEmbreeSyclScene(const float* positions, uint32_t vertices, const uint32_t* indices, uint32_t triangles);
	DLU_EMBREE_SYCL_API void DluEmbreeSyclRelease(void* scene);
	// The nearest hits (never a ray's skip triangle) and whether anything is hit; 0 when done
	DLU_EMBREE_SYCL_API int DluEmbreeSyclClosest(void* scene, const void* rays, void* hits, uint32_t count);
	DLU_EMBREE_SYCL_API int DluEmbreeSyclOccluded(void* scene, const void* rays, uint8_t* occluded, uint32_t count);
}
