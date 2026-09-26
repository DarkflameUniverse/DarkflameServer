#pragma once

#include <optional>
#include <string>
#include <vector>

/**
 * Every server setting the code reads, for the Settings page. The page is organised by what a setting is for
 * (categories, then sections), not by which .ini file holds it; each setting still knows its file.
 *
 * A setting has a type, the default the code uses when nothing sets it (not always what the shipped .ini says), valid
 * values, a unit, and whether servers only read it at startup. A setting or a whole section can depend on another
 * setting ("only used when database_type is mysql"); the page hides it while that isn't so.
 * Settings missing here still show on the page, as plain text. Normalize() is pure and unit tested.
 */
namespace SettingsCatalog {
	enum class eType {
		BOOL,     // stored as 1 or 0 (the servers check == "1")
		INT,
		FLOAT,
		TEXT,
		SECRET,   // text that is never shown back
		INT_LIST, // comma separated whole numbers
		CHOICE,   // one of choices
	};

	// What a text setting holds: picks the input and adds a check
	enum class eFormat { PLAIN, PATH, HOST, URL, EMAIL };

	// What the numbers in a list are, so the page can show their names
	enum class eListOf { NUMBER, ZONE, LOT, REWARD_CODE };

	// How a section lays out its settings
	enum class eLayout {
		ROWS,  // one setting per row
		GRID,  // short values side by side (e.g. event_1..event_8)
		PAIRS, // title/text pairs shown together (the help menu)
	};

	// "Only used when <file> <key> is one of <values>"
	struct Condition {
		std::string file;
		std::string key;
		std::vector<std::string> values;
	};

	struct Category {
		std::string id;
		std::string name;
		std::string description;
	};

	struct Section {
		std::string category; // Category::id
		std::string name;     // unique across categories
		std::string description;
		eLayout layout{ eLayout::ROWS };
		std::optional<Condition> condition;
	};

	struct Setting {
		std::string key;
		std::string file;         // e.g. worldconfig.ini
		std::string section;      // Section::name
		std::string title;
		std::string description;
		eType type{ eType::TEXT };
		std::string defaultValue; // what the code uses when the setting is empty or missing
		std::optional<double> min;
		std::optional<double> max;
		std::vector<std::string> choices;
		std::vector<std::string> choiceLabels; // what the page shows for each choice (same order); empty shows the values
		std::string unit;         // shown next to the input, e.g. "days"
		eFormat format{ eFormat::PLAIN };
		eListOf listOf{ eListOf::NUMBER };
		std::optional<Condition> condition;
		bool restart{};           // only read when a server starts
		bool unused{};            // in the shipped .ini but not read by this version
		bool multiline{};
	};

	const std::vector<Category>& Categories();
	const std::vector<Section>& Sections();
	const std::vector<Setting>& All();
	const Setting* Find(const std::string& file, const std::string& key);
	const Section* FindSection(const std::string& name);

	// Config files with a friendly name (for custom settings and the "file" shown on each row)
	const std::vector<std::pair<std::string, std::string>>& Files();

	std::string TypeName(eType type);
	std::string FormatName(eFormat format);
	std::string ListOfName(eListOf listOf);
	std::string LayoutName(eLayout layout);

	/**
	 * Check a value for a setting and put it in the form the servers read: booleans become 1/0, numbers are
	 * range-checked and written plainly, lists lose spaces and empty entries, URLs and email addresses are checked.
	 * Empty always means "unset". Returns the normalized value, or an error message in `error`.
	 */
	std::optional<std::string> Normalize(const Setting& setting, const std::string& value, std::string& error);
}
