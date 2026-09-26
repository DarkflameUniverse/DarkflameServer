#include <gtest/gtest.h>

#include "EventParts.h"

using namespace EventParts;

namespace {
	Part Make(eKind kind, nlohmann::json config, bool applied = false) {
		return { .kind = kind, .config = std::move(config), .applied = applied };
	}
}

TEST(EventPartsTests, KindNames) {
	EXPECT_EQ(KindName(eKind::LIVE_EVENT), "live_event");
	EXPECT_EQ(KindOf("announcement"), eKind::ANNOUNCEMENT);
	EXPECT_EQ(KindOf("Vanity"), eKind::VANITY);
	EXPECT_FALSE(KindOf("party").has_value());
}

TEST(EventPartsTests, ParseAndWrite) {
	std::string error;
	EXPECT_TRUE(Parse("", error)->empty());
	// applied may be 1, as the migration from the calendar and vanity events wrote it
	const auto parts = Parse(R"([{"kind":"feature","config":{"feature":"oct2011content"},"applied":1,"state":{"slot":3},"status":"On in event_3"},{"kind":"vanity"}])", error);
	ASSERT_TRUE(parts) << error;
	ASSERT_EQ(parts->size(), 2u);
	EXPECT_EQ((*parts)[0].kind, eKind::FEATURE);
	EXPECT_TRUE((*parts)[0].applied);
	EXPECT_EQ((*parts)[0].state["slot"], 3);
	EXPECT_FALSE((*parts)[1].applied);
	EXPECT_TRUE((*parts)[1].config.is_object());

	const auto again = Parse(Write(*parts), error);
	ASSERT_TRUE(again);
	EXPECT_EQ(Write(*again), Write(*parts));

	EXPECT_FALSE(Parse("{}", error));
	EXPECT_FALSE(Parse(R"([{"kind":"party"}])", error));
	EXPECT_NE(error.find("live_event"), std::string::npos); // says what the kinds are
	EXPECT_FALSE(Parse("[", error));
}

TEST(EventPartsTests, Steps) {
	const auto idle = Make(eKind::FEATURE, {});
	const auto started = Make(eKind::FEATURE, {}, true);
	EXPECT_EQ(StepFor(idle, true), eStep::START);
	EXPECT_EQ(StepFor(started, true), eStep::CONTINUE);
	EXPECT_EQ(StepFor(started, false), eStep::END);
	EXPECT_EQ(StepFor(idle, false), eStep::NONE);
}

TEST(EventPartsTests, ReconcileKeepsWhatDidntChange) {
	auto feature = Make(eKind::FEATURE, { {"feature", "oct2011content"} }, true);
	feature.state = { {"slot", 2} };
	const auto announcement = Make(eKind::ANNOUNCEMENT, { {"message", "Boo"} }, true);
	const auto vanity = Make(eKind::VANITY, { {"file", "halloween.xml"} }, false);

	auto live = Make(eKind::LIVE_EVENT, { {"type", "bonus"} }, true);
	live.state = { {"id", 7} };

	// The feature stays, the announcement's text and the live event changed, the vanity part is gone, a restart is new
	const auto result = Reconcile({ feature, announcement, vanity, live }, {
		Make(eKind::ANNOUNCEMENT, { {"message", "Boo!"} }), Make(eKind::FEATURE, { {"feature", "oct2011content"} }), Make(eKind::LIVE_EVENT, { {"type", "invasion"} }, true),
		Make(eKind::RESTART, { {"when", "end"} }, true) });
	ASSERT_EQ(result.parts.size(), 4u);
	EXPECT_TRUE(result.parts[0].applied);  // momentary: already said, not said again
	EXPECT_TRUE(result.parts[1].applied);  // the same: keeps its slot
	EXPECT_EQ(result.parts[1].state["slot"], 2);
	EXPECT_FALSE(result.parts[2].applied); // changed: starts afresh
	EXPECT_FALSE(result.parts[3].applied); // new, whatever the page sent
	// Only started parts that are gone or changed are undone
	ASSERT_EQ(result.retired.size(), 1u);
	EXPECT_EQ(result.retired[0].state["id"], 7);

	// Two of the same kind and config are matched one to one
	const auto twice = Reconcile({ announcement }, { Make(eKind::ANNOUNCEMENT, { {"message", "Boo"} }), Make(eKind::ANNOUNCEMENT, { {"message", "Boo"} }) });
	EXPECT_TRUE(twice.parts[0].applied);
	EXPECT_FALSE(twice.parts[1].applied);
	EXPECT_TRUE(twice.retired.empty());
}

TEST(EventPartsTests, VanityChanges) {
	const auto changes = VanityChanges("Halloween", R"([
		{"kind":"feature","config":{"feature":"oct2011content"}},
		{"kind":"vanity","config":{"file":"halloween-look.xml","removals":["Bob","Alice"],"fileSwitches":{"halloween.xml":true,"summer.xml":false}}},
		{"kind":"vanity","config":{"removals":"Carl\n","file":7}}])");
	ASSERT_EQ(changes.size(), 2u);
	EXPECT_EQ(changes[0].name, "Halloween");
	EXPECT_EQ(changes[0].file, "halloween-look.xml");
	EXPECT_EQ(changes[0].removals, "Bob\nAlice\n");
	std::string error;
	const auto switches = VanityEvents::ParseFileSwitches(changes[0].fileSwitches, error);
	ASSERT_TRUE(switches) << error;
	EXPECT_EQ(switches->at("summer.xml"), false);
	EXPECT_EQ(changes[1].file, ""); // not a name: ignored rather than failing the world
	EXPECT_EQ(changes[1].removals, "Carl\n");
	EXPECT_TRUE(VanityChanges("Broken", "[").empty());
}
