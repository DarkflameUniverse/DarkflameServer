#include "UgcHsr.h"

#include "UgcRender.h"

namespace UgcHsr {
	Result RemoveHiddenFaces(UgcModel::Model& model, const Options& options) {
		Result result;
		auto& opaque = model.opaque;
		result.trianglesBefore = opaque.TriangleCount() + model.transparent.TriangleCount();
		if (opaque.Empty() || !options.enabled) return result;
		result.kept = UgcRender::VisibleFromAround(model, options.resolution, options.groundPlane);
		for (const bool kept : result.kept) result.trianglesRemoved += kept ? 0 : 1;
		UgcModel::KeepTriangles(opaque, result.kept);
		return result;
	}
}
