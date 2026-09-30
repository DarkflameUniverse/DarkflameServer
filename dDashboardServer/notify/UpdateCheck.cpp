#include "UpdateCheck.h"

#include <algorithm>

#include "json.hpp"

namespace {
	bool IsNameChar(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
	}

	std::string Plural(uint32_t count, const std::string& one, const std::string& many) {
		return std::to_string(count) + " " + (count == 1 ? one : many);
	}

	std::string ReleaseName(const UpdateCheck::Release& release) {
		return release.version ? "v" + release.version->ToString() : release.tag;
	}

	uint32_t Count(const nlohmann::json& object, const char* key) {
		const auto it = object.find(key);
		if (it == object.end() || !it->is_number_integer() || it->get<int64_t>() < 0) return 0;
		return static_cast<uint32_t>(std::min<int64_t>(it->get<int64_t>(), UINT32_MAX));
	}

	std::string Text(const nlohmann::json& object, const char* key) {
		const auto it = object.find(key);
		return it != object.end() && it->is_string() ? it->get<std::string>() : "";
	}
}

namespace UpdateCheck {
	std::string Version::ToString() const {
		return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
	}

	std::optional<Version> ParseVersion(std::string_view tag) {
		if (!tag.empty() && (tag.front() == 'v' || tag.front() == 'V')) tag.remove_prefix(1);
		uint32_t parts[3]{};
		size_t part = 0;
		bool any = false;
		for (size_t i = 0; i < tag.size() && part < 3; i++) {
			const char c = tag[i];
			if (c >= '0' && c <= '9') {
				const uint64_t next = static_cast<uint64_t>(parts[part]) * 10 + (c - '0');
				if (next > UINT32_MAX) return std::nullopt;
				parts[part] = static_cast<uint32_t>(next);
				any = true;
			} else if (c == '.' && any && i + 1 < tag.size() && tag[i + 1] >= '0' && tag[i + 1] <= '9') {
				part++;
			} else {
				break;
			}
		}
		if (!any) return std::nullopt;
		return Version{ parts[0], parts[1], parts[2] };
	}

	Build CurrentBuild() {
		Build build;
		build.kind = BuildInfo::buildKind;
		build.version = { BuildInfo::versionMajor, BuildInfo::versionMinor, BuildInfo::versionPatch };
		build.commit = std::string(BuildInfo::commit);
		build.branch = std::string(BuildInfo::branch);
		build.dirty = BuildInfo::dirty;
		return build;
	}

	std::string DescribeBuild(const Build& build) {
		const std::string changes = build.dirty ? " with uncommitted changes" : "";
		const std::string ofCommit = build.commit.empty() ? "" : " of commit " + build.ShortCommit();
		switch (build.kind) {
			case BuildInfo::eBuildKind::RELEASE:
				return "release v" + build.version.ToString() + changes;
			case BuildInfo::eBuildKind::LOCAL:
				return "a local development build" + ofCommit + (build.commit.empty() ? " (no git commit recorded)" : "") + changes;
			case BuildInfo::eBuildKind::CI:
				return "a CI build" + ofCommit + changes;
			default:
				return "an unidentified build" + ofCommit + changes;
		}
	}

	std::string BuildLabel(const Build& build) {
		const std::string commit = build.commit.empty() ? "" : " " + build.ShortCommit() + (build.dirty ? "*" : "");
		switch (build.kind) {
			case BuildInfo::eBuildKind::RELEASE: return "v" + build.version.ToString() + (build.dirty ? "*" : "");
			case BuildInfo::eBuildKind::LOCAL: return "Local dev build" + commit;
			case BuildInfo::eBuildKind::CI: return "CI build" + commit;
			default: return "Unidentified build" + commit;
		}
	}

	std::optional<Release> ParseRelease(const std::string& body, std::string& error) {
		const auto json = nlohmann::json::parse(body, nullptr, false);
		if (!json.is_object()) {
			error = "GitHub's answer about releases wasn't JSON";
			return std::nullopt;
		}
		Release release;
		release.tag = Text(json, "tag_name");
		if (release.tag.empty()) {
			const auto message = Text(json, "message");
			error = message.empty() ? "GitHub's answer had no release in it" : "GitHub: " + message;
			return std::nullopt;
		}
		release.name = Text(json, "name");
		release.url = Text(json, "html_url");
		release.publishedAt = Text(json, "published_at");
		release.version = ParseVersion(release.tag);
		return release;
	}

	std::optional<Comparison> ParseComparison(const std::string& body, std::string& error) {
		const auto json = nlohmann::json::parse(body, nullptr, false);
		if (!json.is_object()) {
			error = "GitHub's comparison wasn't JSON";
			return std::nullopt;
		}
		Comparison comparison;
		comparison.status = Text(json, "status");
		// GitHub's error bodies have a "status" too (the HTTP code), but no counts
		const auto aheadBy = json.find("ahead_by");
		if (comparison.status.empty() || aheadBy == json.end() || !aheadBy->is_number_integer()) {
			const auto message = Text(json, "message");
			error = message.empty() ? "GitHub's answer had no comparison in it" : "GitHub: " + message;
			return std::nullopt;
		}
		comparison.commitsBehind = Count(json, "ahead_by");
		comparison.commitsAhead = Count(json, "behind_by");
		comparison.url = Text(json, "html_url");
		return comparison;
	}

	std::string ErrorMessage(const std::string& body) {
		const auto json = nlohmann::json::parse(body, nullptr, false);
		return json.is_object() ? Text(json, "message") : "";
	}

	bool IsValidRepo(std::string_view repo) {
		const auto slash = repo.find('/');
		if (slash == std::string_view::npos || slash == 0 || slash + 1 >= repo.size() || repo.size() > 140) return false;
		const auto owner = repo.substr(0, slash);
		const auto name = repo.substr(slash + 1);
		return std::ranges::all_of(owner, IsNameChar) && std::ranges::all_of(name, IsNameChar) && name != "." && name != "..";
	}

	bool IsSafeBranch(std::string_view branch) {
		if (branch.empty() || branch.size() > 200 || branch.front() == '/' || branch.find("..") != std::string_view::npos) return false;
		return std::ranges::all_of(branch, [](char c) { return IsNameChar(c) || c == '/'; });
	}

	bool ReleaseIsNewer(const Build& build, const Status& status) {
		return status.release && status.release->version && *status.release->version > build.version;
	}

	eState State(const Build& build, const Status& status) {
		if (!status.release && !status.comparison) return status.checkedAt == 0 ? eState::UNKNOWN : eState::FAILED;
		if (ReleaseIsNewer(build, status)) return eState::UPDATE_AVAILABLE;
		if (build.kind != BuildInfo::eBuildKind::RELEASE && status.comparison && status.comparison->commitsBehind > 0) return eState::UPDATE_AVAILABLE;
		return eState::UP_TO_DATE;
	}

	std::string Summary(const Build& build, const Status& status) {
		const auto running = "this server is running " + DescribeBuild(build);
		const auto state = State(build, status);
		if (state == eState::UNKNOWN) return "Not checked for updates yet";
		if (state == eState::FAILED) return "Could not check for updates: " + (status.error.empty() ? std::string("no answer") : status.error);

		const bool devBehind = build.kind != BuildInfo::eBuildKind::RELEASE && status.comparison && status.comparison->commitsBehind > 0;
		if (state == eState::UPDATE_AVAILABLE) {
			std::string line = "Update available: ";
			if (ReleaseIsNewer(build, status)) {
				line += "release " + ReleaseName(*status.release);
				if (devBehind) line += ", and ";
			}
			if (devBehind) line += Plural(status.comparison->commitsBehind, "newer commit", "newer commits") + " on " + status.comparedBranch;
			line += " (" + running + ")";
			const auto& url = devBehind ? status.comparison->url : status.release->url;
			if (!url.empty()) line += ": " + url;
			return line;
		}

		std::string line = "Up to date";
		if (build.kind != BuildInfo::eBuildKind::RELEASE && status.comparison) {
			line += " with " + status.comparedBranch;
			if (status.comparison->commitsAhead > 0) line += " (and " + Plural(status.comparison->commitsAhead, "commit", "commits") + " not on it)";
		} else if (status.release) {
			line += ": the newest release is " + ReleaseName(*status.release);
		}
		line += "; " + running;
		return line;
	}
}
