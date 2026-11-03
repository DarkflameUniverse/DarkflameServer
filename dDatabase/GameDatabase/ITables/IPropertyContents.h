#ifndef __IPROPERTIESCONTENTS__H__
#define __IPROPERTIESCONTENTS__H__

#include <array>
#include <cstdint>
#include <string_view>

class IPropertyContents {
public:	
	struct Model {
		inline bool operator==(const LWOOBJID& other) const noexcept {
			return id == other;
		}

		NiPoint3 position;
		NiQuaternion rotation = QuatUtils::IDENTITY;
		LWOOBJID id{};
		LOT lot{};
		LWOOBJID ugcId{};
		std::array<LWOOBJID, 5> behaviors{};
	};

	// Inserts a new UGC model into the database. unsplitId is the save it was split from (InsertUnsplitUgcModel), if any.
	virtual void InsertNewUgcModel(
		std::stringstream& sd0Data,
		const uint64_t blueprintId,
		const uint32_t accountId,
		const LWOOBJID characterId,
		const std::optional<LWOOBJID> unsplitId) = 0;

	// Keeps a brick by brick save as the client sent it, before it is split into models.
	virtual void InsertUnsplitUgcModel(std::stringstream& sd0Data, const LWOOBJID id, const uint32_t accountId, const LWOOBJID characterId) = 0;

	// Get the property models for the given property id.
	virtual std::vector<IPropertyContents::Model> GetPropertyModels(const LWOOBJID& propertyId) = 0;

	// Insert a new property model into the database.
	virtual void InsertNewPropertyModel(const LWOOBJID& propertyId, const IPropertyContents::Model& model, const std::string_view name) = 0;

	// Update the model position and rotation for the given property id.
	virtual void UpdateModel(const LWOOBJID& modelID, const NiPoint3& position, const NiQuaternion& rotation, const std::array<std::pair<LWOOBJID, std::string>, 5>& behaviors) = 0;
	virtual void UpdateModel(const LWOOBJID& modelID, const NiPoint3& position, const NiQuaternion& rotation, const std::array<LWOOBJID, 5> behaviorIDs) {
		std::array<std::pair<LWOOBJID, std::string>, 5> behaviors;
		for (int32_t i = 0; i < behaviors.size(); i++) behaviors[i].first = behaviorIDs[i];
		UpdateModel(modelID, position, rotation, behaviors);
	}

	// Remove the model for the given property id.
	virtual void RemoveModel(const LWOOBJID& modelId) = 0;

	// Gets a model by ID
	virtual std::optional<Model> GetModel(const LWOOBJID modelID) = 0;
};
#endif  //!__IPROPERTIESCONTENTS__H__
