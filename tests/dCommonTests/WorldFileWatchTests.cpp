#include <gtest/gtest.h>

#include <map>

#include "WorldFileWatch.h"

using namespace WorldFileWatch;
using InstanceMigration::InstanceView;

namespace {
	using Kind = ZoneFileLog::eKind;

	ZoneFileLog::Entry File(const std::string& path, uint64_t hash, Kind kind = Kind::SCENE, bool packed = false) {
		return { kind, packed, 100, hash, path };
	}

	Stamp At(uint64_t size, int64_t mtime) {
		Stamp stamp;
		stamp.exists = true;
		stamp.size = size;
		stamp.mtime = mtime;
		return stamp;
	}

	// The files on disk: path -> stamp
	struct Disk {
		std::map<std::string, Stamp> stamps;
		std::vector<HashJob> Poll(Tracker& tracker) {
			return tracker.Poll([this](const std::string& path) {
				const auto it = stamps.find(path);
				return it == stamps.end() ? Stamp{} : it->second;
			});
		}
	};

	// Polls and finishes every hash with the given contents: path -> hash
	std::vector<std::string> PollAndHash(Tracker& tracker, Disk& disk, const std::map<std::string, uint64_t>& contents, std::vector<std::string>* changed = nullptr) {
		std::vector<std::string> hashed;
		for (const auto& job : disk.Poll(tracker)) {
			hashed.push_back(job.path);
			const auto it = contents.find(job.path);
			const bool didChange = tracker.Hashed(job, it == contents.end() ? std::nullopt : std::optional<uint64_t>(it->second), 100);
			if (changed && didChange) changed->push_back(job.path);
		}
		return hashed;
	}

	InstanceView View(uint32_t zone, uint32_t instance, int32_t players, uint32_t clone = 0) {
		InstanceView view;
		view.zoneId = zone;
		view.instanceId = instance;
		view.cloneId = clone;
		view.players = players;
		view.ready = true;
		return view;
	}
}

TEST(WorldFileWatchTests, NewFilesAreReadOnceAndMatchingInstancesAreNotStale) {
	Tracker tracker;
	Disk disk;
	disk.stamps["/res/a.luz"] = At(10, 1);
	disk.stamps["/res/a.lvl"] = At(20, 1);
	tracker.Report(1100, 1, 0, { File("/res/a.luz", 1, Kind::ZONE), File("/res/a.lvl", 2) });
	// A second instance of the zone shares the files: they are read once
	tracker.Report(1100, 2, 0, { File("/res/a.luz", 1, Kind::ZONE), File("/res/a.lvl", 2) });

	EXPECT_EQ(PollAndHash(tracker, disk, { { "/res/a.luz", 1 }, { "/res/a.lvl", 2 } }).size(), 2u);
	EXPECT_EQ(tracker.Files().size(), 2u);
	EXPECT_TRUE(tracker.Stale().empty());
	// Nothing changed: nothing is read again
	EXPECT_TRUE(PollAndHash(tracker, disk, { { "/res/a.luz", 1 }, { "/res/a.lvl", 2 } }).empty());
}

TEST(WorldFileWatchTests, ChangeMustHoldStillForOnePoll) {
	Tracker tracker;
	Disk disk;
	disk.stamps["/res/a.lvl"] = At(20, 1);
	tracker.Report(1100, 1, 0, { File("/res/a.lvl", 2) });
	PollAndHash(tracker, disk, { { "/res/a.lvl", 2 } });

	// Being written: the stamp moves, so it isn't read yet
	disk.stamps["/res/a.lvl"] = At(25, 2);
	EXPECT_TRUE(PollAndHash(tracker, disk, { { "/res/a.lvl", 3 } }).empty());
	disk.stamps["/res/a.lvl"] = At(30, 3);
	EXPECT_TRUE(PollAndHash(tracker, disk, { { "/res/a.lvl", 3 } }).empty());
	EXPECT_TRUE(tracker.Stale().empty());

	// Held still for one poll: read, changed, and the instance is stale
	std::vector<std::string> changed;
	EXPECT_EQ(PollAndHash(tracker, disk, { { "/res/a.lvl", 3 } }, &changed).size(), 1u);
	EXPECT_EQ(changed, std::vector<std::string>{ "/res/a.lvl" });
	ASSERT_EQ(tracker.Stale().size(), 1u);
	EXPECT_EQ(tracker.Stale()[0], (InstanceKey{ 1100, 1 }));
	EXPECT_TRUE(tracker.IsChanged(1100, "/res/a.lvl"));
	EXPECT_EQ(tracker.ChangedFiles({ 1100, 1 }), std::vector<std::string>{ "/res/a.lvl" });
}

TEST(WorldFileWatchTests, TouchedButSameContentIsNotAChange) {
	Tracker tracker;
	Disk disk;
	disk.stamps["/res/a.lvl"] = At(20, 1);
	tracker.Report(1100, 1, 0, { File("/res/a.lvl", 2) });
	PollAndHash(tracker, disk, { { "/res/a.lvl", 2 } });

	disk.stamps["/res/a.lvl"] = At(20, 9);
	PollAndHash(tracker, disk, { { "/res/a.lvl", 2 } });
	std::vector<std::string> changed;
	EXPECT_EQ(PollAndHash(tracker, disk, { { "/res/a.lvl", 2 } }, &changed).size(), 1u);
	EXPECT_TRUE(changed.empty());
	EXPECT_TRUE(tracker.Stale().empty());
}

TEST(WorldFileWatchTests, NewInstanceOnTheNewFileIsNotStale) {
	Tracker tracker;
	Disk disk;
	disk.stamps["/res/a.lvl"] = At(20, 1);
	tracker.Report(1100, 1, 0, { File("/res/a.lvl", 2) });
	PollAndHash(tracker, disk, { { "/res/a.lvl", 2 } });
	disk.stamps["/res/a.lvl"] = At(30, 2);
	PollAndHash(tracker, disk, { { "/res/a.lvl", 3 } });
	PollAndHash(tracker, disk, { { "/res/a.lvl", 3 } });

	// The replacement loaded the new version; the old one is stale until it stops
	tracker.Report(1100, 5, 0, { File("/res/a.lvl", 3) });
	EXPECT_EQ(tracker.Stale(), std::vector<InstanceKey>{ (InstanceKey{ 1100, 1 }) });
	tracker.Forget(1100, 1);
	EXPECT_TRUE(tracker.Stale().empty());
	EXPECT_FALSE(tracker.IsChanged(1100, "/res/a.lvl"));
}

TEST(WorldFileWatchTests, AWorldThatReadAnOlderVersionIsStaleOnceMasterReadsTheFile) {
	Tracker tracker;
	Disk disk;
	disk.stamps["/res/a.lvl"] = At(20, 1);
	// The file changed between the world's read and master's first read
	tracker.Report(1100, 1, 0, { File("/res/a.lvl", 2) });
	EXPECT_TRUE(tracker.Stale().empty());
	PollAndHash(tracker, disk, { { "/res/a.lvl", 7 } });
	EXPECT_EQ(tracker.Stale().size(), 1u);
}

TEST(WorldFileWatchTests, PackedFilesAreNotWatchedAndUnusedFilesAreDropped) {
	Tracker tracker;
	Disk disk;
	disk.stamps["/res/b.luz"] = At(20, 1);
	tracker.Report(1200, 1, 0, { File("maps/b.lvl", 2, Kind::SCENE, true), File("/res/b.luz", 1, Kind::ZONE) });
	EXPECT_EQ(PollAndHash(tracker, disk, { { "/res/b.luz", 1 } }), std::vector<std::string>{ "/res/b.luz" });
	EXPECT_EQ(tracker.FilesOf(1200).size(), 2u);

	tracker.Forget(1200, 1);
	EXPECT_TRUE(tracker.Files().empty());
	EXPECT_TRUE(tracker.Zones().empty());
}

TEST(WorldFileWatchTests, MissingFileIsNotAChangeUntilItComesBack) {
	Tracker tracker;
	Disk disk;
	disk.stamps["/res/a.lvl"] = At(20, 1);
	tracker.Report(1100, 1, 0, { File("/res/a.lvl", 2) });
	PollAndHash(tracker, disk, { { "/res/a.lvl", 2 } });

	disk.stamps.erase("/res/a.lvl");
	EXPECT_TRUE(PollAndHash(tracker, disk, {}).empty());
	EXPECT_TRUE(PollAndHash(tracker, disk, {}).empty());
	EXPECT_TRUE(tracker.Files().at("/res/a.lvl").missing);
	EXPECT_TRUE(tracker.Stale().empty());

	disk.stamps["/res/a.lvl"] = At(22, 5);
	PollAndHash(tracker, disk, { { "/res/a.lvl", 4 } });
	PollAndHash(tracker, disk, { { "/res/a.lvl", 4 } });
	EXPECT_FALSE(tracker.Files().at("/res/a.lvl").missing);
	EXPECT_EQ(tracker.Stale().size(), 1u);
}

TEST(WorldFileWatchTests, SignatureFollowsTheVersionOnDisk) {
	Tracker tracker;
	Disk disk;
	disk.stamps["/res/a.lvl"] = At(20, 1);
	tracker.Report(1100, 1, 0, { File("/res/a.lvl", 2) });
	PollAndHash(tracker, disk, { { "/res/a.lvl", 2 } });
	const auto before = tracker.Signature({ 1100, 1 });
	disk.stamps["/res/a.lvl"] = At(30, 2);
	PollAndHash(tracker, disk, { { "/res/a.lvl", 3 } });
	PollAndHash(tracker, disk, { { "/res/a.lvl", 3 } });
	const auto after = tracker.Signature({ 1100, 1 });
	EXPECT_NE(before, after);
	EXPECT_EQ(after, tracker.Signature({ 1100, 1 }));
}

TEST(WorldFileWatchTests, ChooseReplacesBusyAndStopsEmptyInstances) {
	std::vector<InstanceView> views{
		View(1100, 1, 5),
		View(1100, 2, 0),
		View(1200, 3, 2),
		View(1150, 4, 1, 77),      // a property with players: never moved, kept until they left
		View(0, 5, 3),             // character selection
	};
	auto starting = View(1100, 6, 0);
	starting.ready = false;
	views.push_back(starting);
	auto draining = View(1100, 7, 4);
	draining.draining = true;
	views.push_back(draining);
	auto stopping = View(1100, 8, 0);
	stopping.shuttingDown = true;
	views.push_back(stopping);

	const auto choices = Choose(views, [](const InstanceView& view) { return view.zoneId != 1200; }, {});
	std::map<uint32_t, eAction> byInstance;
	for (const auto& choice : choices) byInstance[choice.view.instanceId] = choice.action;
	EXPECT_EQ(byInstance.size(), 7u);           // 1200 wasn't wanted
	EXPECT_EQ(byInstance[1], eAction::REPLACE);
	EXPECT_EQ(byInstance[2], eAction::STOP);
	EXPECT_EQ(byInstance[4], eAction::KEEP_UNTIL_EMPTY);
	EXPECT_EQ(byInstance[5], eAction::SKIP);
	EXPECT_EQ(byInstance[6], eAction::SKIP);
	EXPECT_EQ(byInstance[7], eAction::SKIP);
	EXPECT_EQ(byInstance[8], eAction::SKIP);
	for (const auto& choice : choices) {
		if (choice.action == eAction::SKIP) EXPECT_FALSE(choice.reason.empty());
	}
}

TEST(WorldFileWatchTests, ChooseKeepsAnInstanceOfKeptZones) {
	// Only empty public instances of a kept zone: one new instance is started for them
	{
		const std::vector<InstanceView> views{ View(1000, 1, 0), View(1000, 2, 0) };
		const auto choices = Choose(views, [](const InstanceView&) { return true; }, { 1000 });
		ASSERT_EQ(choices.size(), 2u);
		EXPECT_EQ(choices[0].action, eAction::START_THEN_STOP);
		EXPECT_EQ(choices[1].action, eAction::STOP);
	}
	// A busy public instance is replaced anyway: the empty one just stops
	{
		const std::vector<InstanceView> views{ View(1000, 1, 0), View(1000, 2, 3) };
		const auto choices = Choose(views, [](const InstanceView&) { return true; }, { 1000 });
		ASSERT_EQ(choices.size(), 2u);
		EXPECT_EQ(choices[0].action, eAction::STOP);
		EXPECT_EQ(choices[1].action, eAction::REPLACE);
	}
	// Not a kept zone, or a private instance: stopped
	{
		auto privateView = View(1000, 3, 0);
		privateView.isPrivate = true;
		const std::vector<InstanceView> views{ View(1100, 1, 0), privateView };
		const auto choices = Choose(views, [](const InstanceView&) { return true; }, { 1000 });
		ASSERT_EQ(choices.size(), 2u);
		EXPECT_EQ(choices[0].action, eAction::STOP);
		EXPECT_EQ(choices[1].action, eAction::STOP);
	}
}
