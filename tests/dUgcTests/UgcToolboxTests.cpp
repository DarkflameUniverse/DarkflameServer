// processor=toolbox-blender: the option, the fallback, the worker protocol and the Blender worker's process handling
// (with a stand-in for Blender), and LU Toolbox itself when Blender and LU Toolbox are there (skipped otherwise)

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

#include "NifFile.h"
#include "UgcBricks.h"
#include "UgcJobs.h"
#include "UgcKeys.h"
#include "UgcToolbox.h"
#include "UgcToolboxProtocol.h"
#include "json.hpp"

namespace {
	std::filesystem::path Folder(const std::string& name) {
		const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
		auto path = std::filesystem::temp_directory_path() / ("dlu_toolbox_test_" + name + "_" + (test ? std::string(test->name()) : std::string()) + "_" + std::to_string(::getpid()));
		std::filesystem::remove_all(path);
		std::filesystem::create_directories(path);
		return path;
	}

	void WriteFile(const std::filesystem::path& path, const std::string& text) {
		std::filesystem::create_directories(path.parent_path());
		std::ofstream(path, std::ios::binary) << text;
	}

	std::optional<std::filesystem::path> FindProgram(const std::string& name) {
		const char* path = std::getenv("PATH");
		if (!path) return std::nullopt;
		std::string paths(path);
		size_t start = 0;
		while (start <= paths.size()) {
			auto end = paths.find(':', start);
			if (end == std::string::npos) end = paths.size();
			const auto candidate = std::filesystem::path(paths.substr(start, end - start)) / name;
			if (::access(candidate.c_str(), X_OK) == 0) return candidate;
			start = end + 1;
		}
		return std::nullopt;
	}

	// Everything toolbox-blender needs, with `blender` as the executable (a stand-in unless given)
	UgcToolbox::Config LayOut(const std::filesystem::path& root, const std::filesystem::path& blender) {
		UgcToolbox::Config config;
		config.blender = blender;
		config.standalone = root / "standalone";
		WriteFile(config.standalone / "lu_batch_driver.py", "# LU-Toolbox-Standalone\n");
		config.scripts = root / "scripts";
		WriteFile(config.scripts / "addons" / "lu_toolbox" / "__init__.py", "");
		WriteFile(config.scripts / "addons" / "io_scene_niftools" / "__init__.py", "");
		config.worker = root / "dlu_toolbox_worker.py";
		WriteFile(config.worker, "# worker\n");
		config.res = root / "res";
		WriteFile(config.res / "brickdb.zip", "PK");
		config.brickdb = root / "brickdb";
		config.work = root / "work";
		config.timeoutSeconds = 30;
		return config;
	}

	// A stand-in for Blender running dlu_toolbox_worker.py: the same protocol, and it "makes" a .nif from the LXFML's
	// text. An LXFML saying CRASH makes it exit, HANG makes it stop answering, FAIL makes LU Toolbox fail.
	const char* FAKE_BLENDER = R"(#!/usr/bin/env python3
import json, os, sys, time
out = os.fdopen(os.dup(1), "w")
os.dup2(2, 1)
def send(m):
    m["dlutb"] = 1
    out.write(json.dumps(m) + "\n"); out.flush()
print("blender chatter that must not reach the replies")
send({"type": "ready", "blender": "fake", "toolbox": "2.4.0", "niftools": "0.1.1", "device": sys.argv[sys.argv.index("--device") + 1]})
for line in sys.stdin:
    request = json.loads(line)
    if request.get("cmd") == "quit":
        break
    text = open(request["input"]).read()
    if "CRASH" in text:
        os._exit(3)
    if "HANG" in text:
        time.sleep(3600)
    if "FAIL" in text:
        send({"type": "done", "id": request["id"], "ok": False, "error": "Process Model failed"})
        continue
    sys.stdout.write("partial line without a newline ")
    sys.stdout.flush()
    open(request["output"], "w").write("NIF:" + text + ":" + ",".join(str(l) for l in request["lods"]))
    send({"type": "done", "id": request["id"], "ok": True, "ms": {"process": 5, "bake": 2}})
)";
}

TEST(UgcToolboxOptions, ProcessorIsAnOption) {
	UgcProcessOptions::Choice choice;
	ASSERT_TRUE(UgcProcessOptions::Parse("toolbox-blender", choice));
	EXPECT_EQ(choice.processor, "toolbox-blender");
	EXPECT_TRUE(choice.rays.empty());
	EXPECT_EQ(UgcProcessOptions::ToString(choice), "toolbox-blender");
	ASSERT_TRUE(UgcProcessOptions::Parse("toolbox-blender oidn,embree", choice));
	EXPECT_EQ(UgcProcessOptions::ToString(choice), "embree oidn toolbox-blender");
	ASSERT_TRUE(UgcProcessOptions::Parse("native", choice));
	EXPECT_EQ(UgcProcessOptions::ToString(choice), "native");
	EXPECT_FALSE(UgcProcessOptions::Parse("native toolbox-blender", choice)); // two processors
	EXPECT_FALSE(UgcProcessOptions::Parse("blender", choice));
	// Options stored before there was a processor still read the same; the retired hidden-face method "toolbox" is
	// not the processor
	for (const auto* old : { "embree off", "embree toolbox off", "builtin fast", "hiprt oidn", "" }) {
		ASSERT_TRUE(UgcProcessOptions::Parse(old, choice)) << old;
		EXPECT_TRUE(choice.processor.empty()) << old;
	}
	ASSERT_TRUE(UgcProcessOptions::Parse("embree toolbox off", choice));
	EXPECT_EQ(UgcProcessOptions::ToString(choice), "embree off");

	// Applied over the settings; left out, the setting's stays
	UgcJobs::Settings settings;
	EXPECT_EQ(settings.processor, "native");
	ASSERT_TRUE(UgcProcessOptions::Parse("toolbox-blender", choice));
	UgcJobs::ApplyOptions(settings, choice);
	EXPECT_EQ(settings.processor, "toolbox-blender");
	ASSERT_TRUE(UgcProcessOptions::Parse("oidn", choice));
	UgcJobs::ApplyOptions(settings, choice);
	EXPECT_EQ(settings.processor, "toolbox-blender");
	ASSERT_TRUE(UgcProcessOptions::Parse("native", choice));
	UgcJobs::ApplyOptions(settings, choice);
	EXPECT_EQ(settings.processor, "native");
	// A native make records what it did as before (the processor isn't named), so old and new rows compare
	EXPECT_EQ(UgcProcessOptions::ToString(UgcJobs::MadeWith(settings)), "embree off");
}

TEST(UgcToolboxOptions, FallsBackToNativeAndSaysWhy) {
	const auto root = Folder("fallback");
	std::string why;
	// Nothing set up: not usable, and the reason names the setting
	UgcToolbox::Config config;
	auto problem = UgcToolbox::Problem(config);
#if defined(_WIN32)
	EXPECT_FALSE(problem.empty());
#else
	EXPECT_NE(problem.find("toolbox_blender"), std::string::npos) << problem;
	EXPECT_EQ(UgcToolbox::Resolve("toolbox-blender", problem, why), "native");
	EXPECT_EQ(why, problem);
	// Native asked for: native, no reason given
	EXPECT_EQ(UgcToolbox::Resolve("native", problem, why), "native");
	EXPECT_TRUE(why.empty());
	EXPECT_EQ(UgcToolbox::Resolve("", "", why), "native");

	// Everything there: usable
	const auto blender = root / "blender";
	WriteFile(blender, "#!/bin/sh\n");
	std::filesystem::permissions(blender, std::filesystem::perms::owner_all);
	config = LayOut(root, blender);
	EXPECT_EQ(UgcToolbox::Problem(config), "");
	EXPECT_EQ(UgcToolbox::Resolve("toolbox-blender", UgcToolbox::Problem(config), why), "toolbox-blender");
	EXPECT_TRUE(why.empty());

	// Each missing piece is named
	auto broken = config;
	std::filesystem::permissions(blender, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
	EXPECT_NE(UgcToolbox::Problem(broken).find("not an executable"), std::string::npos);
	std::filesystem::permissions(blender, std::filesystem::perms::owner_all);
	std::filesystem::remove(config.scripts / "addons" / "io_scene_niftools" / "__init__.py");
	EXPECT_NE(UgcToolbox::Problem(config).find("io_scene_niftools"), std::string::npos);
	broken.scripts.clear(); // the add-ons in Blender's own folder: not checked
	EXPECT_EQ(UgcToolbox::Problem(broken), "");
	broken.standalone = root / "nowhere";
	EXPECT_NE(UgcToolbox::Problem(broken).find("lu_batch_driver.py"), std::string::npos);
	broken = LayOut(root, blender);
	broken.device = "metal";
	EXPECT_NE(UgcToolbox::Problem(broken).find("toolbox_device"), std::string::npos);
	broken = LayOut(root, blender);
	std::filesystem::remove(broken.worker);
	EXPECT_NE(UgcToolbox::Problem(broken).find("worker script"), std::string::npos);
	broken = LayOut(root, blender);
	std::filesystem::remove(broken.res / "brickdb.zip");
	EXPECT_NE(UgcToolbox::Problem(broken).find("brickdb.zip"), std::string::npos);
	std::filesystem::create_directories(broken.brickdb / "Assemblies"); // made already: the zip isn't needed
	EXPECT_EQ(UgcToolbox::Problem(broken), "");
#endif
	std::filesystem::remove_all(root);
}

TEST(UgcToolboxProtocol, FramesOneMessagePerLine) {
	// Framed: compact, versioned, one line whatever the strings hold
	const auto line = UgcToolboxProtocol::Frame({ { "cmd", "make" }, { "input", "a\nb\r\"c\"" } });
	ASSERT_FALSE(line.empty());
	EXPECT_EQ(line.back(), '\n');
	EXPECT_EQ(line.find('\n'), line.size() - 1);
	const auto parsed = UgcToolboxProtocol::Parse(std::string_view(line).substr(0, line.size() - 1));
	ASSERT_TRUE(parsed);
	EXPECT_EQ((*parsed)["dlutb"], UgcToolboxProtocol::VERSION);
	EXPECT_EQ((*parsed)["input"], "a\nb\r\"c\"");
	const auto request = UgcToolboxProtocol::MakeRequest(7, "/w/7.lxfml", "/w/7.nif", { 0, 2 });
	EXPECT_EQ(request["cmd"], "make");
	EXPECT_EQ(request["lods"], nlohmann::json::array({ 0, 2 }));

	// Lines arrive in pieces and several at once
	UgcToolboxProtocol::LineReader reader;
	reader.Feed("{\"dlutb\":1,\"ty");
	EXPECT_FALSE(reader.Next());
	reader.Feed("pe\":\"ready\"}\r\n\n{\"dlutb\":1,\"type\":\"pong\",\"id\":3}\n{\"dlu");
	auto first = reader.Next();
	ASSERT_TRUE(first);
	EXPECT_EQ(*first, "{\"dlutb\":1,\"type\":\"ready\"}");
	auto blank = reader.Next();
	ASSERT_TRUE(blank);
	EXPECT_TRUE(blank->empty());
	EXPECT_FALSE(UgcToolboxProtocol::Parse(*blank));
	auto second = reader.Next();
	ASSERT_TRUE(second);
	EXPECT_EQ(UgcToolboxProtocol::TypeOf(*UgcToolboxProtocol::Parse(*second)), UgcToolboxProtocol::eType::PONG);
	EXPECT_FALSE(reader.Next());
	EXPECT_EQ(reader.Pending(), 5u);
	EXPECT_EQ(UgcToolboxProtocol::TypeOf(*UgcToolboxProtocol::Parse(*first)), UgcToolboxProtocol::eType::READY);

	// Not messages: other versions, other JSON, not JSON
	EXPECT_FALSE(UgcToolboxProtocol::Parse("{\"dlutb\":2,\"type\":\"ready\"}"));
	EXPECT_FALSE(UgcToolboxProtocol::Parse("{\"type\":\"ready\"}"));
	EXPECT_FALSE(UgcToolboxProtocol::Parse("[1,2]"));
	EXPECT_FALSE(UgcToolboxProtocol::Parse("Info: Exporting NiNode block"));

	// A line too long to be a message is dropped up to its newline, and what follows still reads
	UgcToolboxProtocol::LineReader flooded;
	flooded.Feed(std::string(UgcToolboxProtocol::MAX_LINE / 2, 'x'));
	EXPECT_FALSE(flooded.Next());
	flooded.Feed(std::string(UgcToolboxProtocol::MAX_LINE, 'x'));
	EXPECT_FALSE(flooded.Next());
	EXPECT_LE(flooded.Pending(), UgcToolboxProtocol::MAX_LINE);
	flooded.Feed(std::string(100, 'x') + "\n{\"dlutb\":1,\"type\":\"pong\"}\n");
	auto after = flooded.Next();
	ASSERT_TRUE(after);
	EXPECT_EQ(UgcToolboxProtocol::TypeOf(*UgcToolboxProtocol::Parse(*after)), UgcToolboxProtocol::eType::PONG);
	EXPECT_EQ(flooded.Overflows(), 1u);

	// A done reply
	auto done = UgcToolboxProtocol::ParseDone(*UgcToolboxProtocol::Parse(R"({"dlutb":1,"type":"done","id":9,"ok":true,"ms":{"process":1200,"bake":300}})"));
	ASSERT_TRUE(done);
	EXPECT_EQ(done->id, 9u);
	EXPECT_TRUE(done->ok);
	ASSERT_EQ(done->ms.size(), 2u);
	done = UgcToolboxProtocol::ParseDone(*UgcToolboxProtocol::Parse(R"({"dlutb":1,"type":"done","id":9,"ok":false})"));
	ASSERT_TRUE(done);
	EXPECT_FALSE(done->ok);
	EXPECT_FALSE(done->error.empty());
	EXPECT_FALSE(UgcToolboxProtocol::ParseDone(*UgcToolboxProtocol::Parse(R"({"dlutb":1,"type":"done"})"))); // no id
	EXPECT_FALSE(UgcToolboxProtocol::ParseDone(*UgcToolboxProtocol::Parse(R"({"dlutb":1,"type":"ready"})")));
}

#if !defined(_WIN32)
TEST(UgcToolboxWorker, StaysUpRestartsAndGivesUp) {
	if (!FindProgram("python3")) GTEST_SKIP() << "no python3 for the stand-in Blender";
	const auto root = Folder("worker");
	const auto blender = root / "blender";
	WriteFile(blender, FAKE_BLENDER);
	std::filesystem::permissions(blender, std::filesystem::perms::owner_all);
	auto config = LayOut(root, blender);
	config.device = "hip";
	ASSERT_EQ(UgcToolbox::Problem(config), "");

	UgcToolbox::Worker worker;
	worker.Configure(config);
	// Two models on one Blender, started for the first
	auto result = worker.Make(1, "<LXFML one/>", { 0, 2 });
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_TRUE(result.started);
	EXPECT_EQ(result.nif, "NIF:<LXFML one/>:0,2");
	EXPECT_EQ(result.ms.value("process", 0.0), 5.0);
	EXPECT_EQ(result.blender, "Blender fake, LU Toolbox 2.4.0, niftools 0.1.1, hip");
	result = worker.Make(2, "<LXFML two/>", { 0 });
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_FALSE(result.started);
	EXPECT_EQ(result.nif, "NIF:<LXFML two/>:0");
	auto status = worker.Status();
	EXPECT_EQ(status["running"], true);
	EXPECT_EQ(status["starts"], 1);
	EXPECT_EQ(status["models"], 2);
	// Its files are cleaned up
	EXPECT_TRUE(std::filesystem::is_empty(config.work) || !std::filesystem::exists(config.work / "model-2.lxfml"));

	// LU Toolbox failing fails the model; Blender stays up
	result = worker.Make(3, "FAIL", { 0 });
	EXPECT_FALSE(result.ok);
	EXPECT_NE(result.error.find("Process Model failed"), std::string::npos) << result.error;
	EXPECT_EQ(worker.Status()["starts"], 1);

	// Blender crashing fails the model, and the next one gets a new Blender
	result = worker.Make(4, "CRASH", { 0 });
	EXPECT_FALSE(result.ok);
	EXPECT_NE(result.error.find("exited with code 3"), std::string::npos) << result.error;
	result = worker.Make(5, "<LXFML five/>", { 0 });
	ASSERT_TRUE(result.ok) << result.error;
	EXPECT_TRUE(result.started);
	EXPECT_EQ(worker.Status()["starts"], 2);

	// A model taking too long fails, and Blender is started again
	config.timeoutSeconds = 1;
	worker.Configure(config);
	const auto start = std::chrono::steady_clock::now();
	result = worker.Make(6, "HANG", { 0 });
	EXPECT_FALSE(result.ok);
	EXPECT_NE(result.error.find("toolbox_timeout_seconds"), std::string::npos) << result.error;
	EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(20));
	EXPECT_EQ(worker.Status()["running"], false);

	// The pace is called while it works; throwing from it gives up and stops Blender
	config.timeoutSeconds = 30;
	worker.Configure(config);
	struct GiveUp {};
	int paced = 0;
	EXPECT_THROW(worker.Make(7, "HANG", { 0 }, [&paced](double cpuSeconds, const UgcToolbox::Worker::Pause& pause) {
		EXPECT_GE(cpuSeconds, 0.0);
		pause(true);
		pause(false);
		if (++paced >= 3) throw GiveUp{};
	}), GiveUp);
	EXPECT_GE(paced, 3);
	EXPECT_EQ(worker.Status()["running"], false);

	// Stopped, it starts again when needed
	result = worker.Make(8, "<LXFML eight/>", { 0 });
	ASSERT_TRUE(result.ok) << result.error;
	worker.Stop();
	EXPECT_EQ(worker.Status()["running"], false);

	// A Blender that isn't one fails to start, with its reason
	WriteFile(blender, "#!/bin/sh\necho 'Error: no such add-on' >&2\nexit 7\n");
	result = worker.Make(9, "<LXFML/>", { 0 });
	EXPECT_FALSE(result.ok);
	EXPECT_NE(result.error.find("no such add-on"), std::string::npos) << result.error;
	std::filesystem::remove_all(root);
}
#endif

/**
 * LU Toolbox itself: runs only when Blender, LU-Toolbox-Standalone and a client's res folder are given
 * (DLU_TEST_TOOLBOX_BLENDER, DLU_TEST_TOOLBOX_STANDALONE, DLU_TEST_CLIENT_RES; and DLU_TEST_TOOLBOX_SCRIPTS when the
 * add-ons aren't in Blender's own folder). Makes a small model and reads its .nif back.
 */
TEST(UgcToolboxIntegration, MakesAModelWithLuToolbox) {
	const auto env = [](const char* name) { const char* value = std::getenv(name); return value ? std::string(value) : std::string(); };
	UgcToolbox::Config config;
	config.blender = env("DLU_TEST_TOOLBOX_BLENDER");
	config.standalone = env("DLU_TEST_TOOLBOX_STANDALONE");
	config.scripts = env("DLU_TEST_TOOLBOX_SCRIPTS");
	config.res = env("DLU_TEST_CLIENT_RES");
	config.worker = DLU_TOOLBOX_WORKER;
	const auto root = std::filesystem::temp_directory_path() / "dlu_toolbox_integration";
	config.brickdb = env("DLU_TEST_TOOLBOX_BRICKDB").empty() ? root / "brickdb" : std::filesystem::path(env("DLU_TEST_TOOLBOX_BRICKDB"));
	config.work = root / "work";
	config.threads = 2;
	config.timeoutSeconds = 600;
	if (config.blender.empty() || config.standalone.empty() || config.res.empty()) {
		GTEST_SKIP() << "set DLU_TEST_TOOLBOX_BLENDER, DLU_TEST_TOOLBOX_STANDALONE and DLU_TEST_CLIENT_RES to run LU Toolbox";
	}
	if (const auto problem = UgcToolbox::Problem(config); !problem.empty()) GTEST_SKIP() << problem;

	// Two bricks: a 2x4 brick (3001) on a 2x2 (3003)
	const std::string lxfml = R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<LXFML versionMajor="5" versionMinor="0"><Meta><Application name="LEGO Universe" versionMajor="0" versionMinor="0"/><Brand name="LEGOUniverse"/><BrickSet version="457"/></Meta><Bricks>
<Brick refID="0" designID="3001"><Part refID="0" designID="3001" materials="21"><Bone refID="0" transformation="1,0,0,0,1,0,0,0,1,0,0,0"/></Part></Brick>
<Brick refID="1" designID="3003"><Part refID="1" designID="3003" materials="23"><Bone refID="1" transformation="1,0,0,0,1,0,0,0,1,0,0.96,0"/></Part></Brick>
</Bricks></LXFML>)";
	UgcBricks::BrickLibrary library(config.res, 0);
	library.LoadMaterials();
	UgcJobs::Settings settings;
	settings.icon.size = 64;
	UgcToolbox::Worker worker;
	worker.Configure(config);
	const auto outcome = UgcJobs::ProcessModelToolbox(lxfml, library, settings, worker, 1);
	worker.Stop();
	ASSERT_TRUE(outcome.ok) << outcome.error;
	EXPECT_EQ(outcome.options, "toolbox-blender");
	ASSERT_TRUE(outcome.files.contains("model.nif.gz"));
	ASSERT_TRUE(outcome.files.contains("icon.png"));
	const auto stats = nlohmann::json::parse(outcome.stats);
	EXPECT_EQ(stats["bricks"], 2);
	EXPECT_EQ(stats["settings"]["processor"], "toolbox-blender");
	ASSERT_EQ(stats["lods"].size(), 2u);
	EXPECT_GT(stats["lods"][0]["trianglesInNif"].get<size_t>(), 0u);
	EXPECT_LE(stats["lods"][0]["opaqueAfter"].get<size_t>(), stats["lods"][0]["opaqueBefore"].get<size_t>());
}
