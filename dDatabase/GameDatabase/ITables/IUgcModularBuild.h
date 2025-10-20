#ifndef IUGCMODULARBUILD_H
#define IUGCMODULARBUILD_H

#include <cstdint>
#include <optional>
#include <string>

class IUgcModularBuild {
public:
	virtual void InsertUgcBuild(const std::string& modules, const LWOOBJID bigId, const std::optional<LWOOBJID> characterId) = 0;
	virtual void DeleteUgcBuild(const LWOOBJID bigId) = 0;
};

#endif  //!IUGCMODULARBUILD_H
