#include "ZoneFilePush.h"

#include <fstream>
#include <vector>

#include "ZoneOverrideStore.h"

namespace {
	std::string BaseOf(const std::string& path) {
		const auto slash = path.find_last_of('/');
		return slash == std::string::npos ? path : path.substr(slash + 1);
	}
}

ZoneFilePush::Result ZoneFilePush::Run(const Job& job) {
	Result result;
	const auto zoneFolder = ZonePublish::FolderOf(job.luzPath);
	try {
		ZoneFilePack::Files files;
		const ZoneFilePack::Limits packLimits{ job.limits.maxFileBytes, job.limits.maxTotalBytes, job.limits.maxFiles };
		if (const auto error = ZoneFilePack::Unpack(job.fileType, job.packedFiles, packLimits, files); error != ZoneFilePack::eError::NONE) {
			result.problem = std::string("the files could not be unpacked: ") + ZoneFilePack::Describe(error);
			return result;
		}

		const auto folder = ZoneOverrideStore::MakeUploadDir(job.root, job.id);
		std::vector<ZonePublish::Upload> uploads;
		std::set<std::string> sent;
		uint32_t index = 0;
		// A name is a base name here; it becomes a file of this zone's folder, and the checks decide whether the
		// zone has such a file. Its bytes are written under a name of our own.
		const auto add = [&](const ZoneFilePack::Entry& entry) {
			const auto path = zoneFolder + ZonePublish::Canonical(entry.name);
			const auto file = folder / ("file-" + std::to_string(index++));
			std::ofstream out(file, std::ios::binary | std::ios::trunc);
			out.write(entry.data.data(), static_cast<std::streamsize>(entry.data.size()));
			out.flush();
			if (!out) throw ZoneOverrideStore::StoreError("A file could not be written");
			uploads.push_back({ path, file, entry.data.size() });
			sent.insert(path);
		};
		if (files.zone) add(*files.zone);
		if (files.terrain) add(*files.terrain);
		for (const auto& scene : files.scenes) add(scene);
		for (const auto& triggers : files.triggers) add(triggers);

		// One file replaces that file in the zone's set: the rest of the set stays as it is
		if (job.fileType != ZoneFilePack::eFileType::WHOLE_MAP) {
			for (const auto& current : ZoneOverrideStore::Current(job.root, job.zoneId, zoneFolder)) {
				if (sent.contains(current.path)) continue;
				const auto file = ZonePublish::ResolveInside(job.root / "maps", current.path);
				if (!file) continue;
				uploads.push_back({ current.path, *file, current.bytes });
				sent.insert(current.path);
			}
		}
		// Every set has the zone's .luz: the client's own when none was sent or kept
		if (!sent.contains(job.luzPath) && job.clientLuz) add({ BaseOf(job.luzPath), *job.clientLuz });

		ZonePublish::Zone zone;
		zone.zoneId = job.zoneId;
		zone.luzPath = job.luzPath;
		zone.clientScenes = job.clientScenes;
		zone.hasFile = [&job](const std::string& path) { return job.clientFiles.contains(path); };
		const auto validation = ZonePublish::Validate(zone, uploads, job.limits);
		if (!validation.ok) {
			for (const auto& file : validation.files) {
				if (file.ok || file.errors.empty()) continue;
				result.problem = BaseOf(file.path) + ": " + file.errors.front().message + " (" + file.errors.front().code + ")";
				break;
			}
			ZoneOverrideStore::RemoveStaging(job.root, job.id);
			return result;
		}
		std::vector<ZoneOverrideStore::NewFile> newFiles;
		for (const auto& file : validation.files) newFiles.push_back({ file.path, file.source, file.bytes, file.sha256, file.version });
		const auto applied = ZoneOverrideStore::Apply(job.root, { job.zoneId, zoneFolder, job.id, job.who, "sent from the game client", job.now, job.keepBackups }, newFiles);
		result.applied = true;
		result.files = applied.files.size();
		result.backupId = applied.backupId;
	} catch (const std::exception& ex) {
		result.problem = ex.what();
		ZoneOverrideStore::RemoveStaging(job.root, job.id);
	}
	return result;
}
