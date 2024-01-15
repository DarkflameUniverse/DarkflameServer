#ifndef __IUGC__H__
#define __IUGC__H__

#include <cstdint>
#include <sstream>
#include <optional>
#include <string>
#include <string_view>

class IUgc {
public:
	struct Model {
		std::stringstream lxfmlData;
		LWOOBJID id{};
	};

	// Gets all UGC models for the given property id.
	virtual std::vector<IUgc::Model> GetUgcModels(const LWOOBJID& propertyId) = 0;

	// Gets all Ugcs models.
	virtual std::vector<IUgc::Model> GetAllUgcModels() = 0;
	
	// Removes ugc models that are not referenced by any property.
	virtual void RemoveUnreferencedUgcModels() = 0;

	// Deletes the ugc model for the given model id.
	virtual void DeleteUgcModelData(const LWOOBJID& modelId) = 0;

	// Inserts a new UGC model into the database.
	virtual void UpdateUgcModelData(const LWOOBJID& modelId, std::istringstream& lxfml) = 0;
};
#endif  //!__IUGC__H__
