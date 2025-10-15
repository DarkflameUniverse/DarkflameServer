#include "gtest/gtest.h"

#include "Lxfml.h"
#include "TinyXmlUtils.h"
#include "dCommonDependencies.h"

#include <fstream>
#include <sstream>
#include <unordered_set>
#include <filesystem>

using namespace TinyXmlUtils;

static std::string ReadFile(const std::string& filename) {
	std::ifstream in(filename, std::ios::in | std::ios::binary);
	if (!in.is_open()) {
		return "";
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

std::string SerializeElement(tinyxml2::XMLElement* elem) {
	tinyxml2::XMLPrinter p;
	elem->Accept(&p);
	return std::string(p.CStr());
};

// Helper function to test splitting functionality
static void TestSplitUsesAllBricksAndNoDuplicatesHelper(const std::string& filename) {
	// Read the LXFML file
	std::string data = ReadFile(filename);
	ASSERT_FALSE(data.empty()) << "Failed to read " << filename << " from build directory";

	std::cout << "\n=== Testing LXFML splitting for: " << filename << " ===" << std::endl;
	
	auto results = Lxfml::Split(data);
	ASSERT_GT(results.size(), 0) << "Split results should not be empty for " << filename;
	
	std::cout << "Split produced " << results.size() << " output(s)" << std::endl;

	// parse original to count bricks
	tinyxml2::XMLDocument doc;
	ASSERT_EQ(doc.Parse(data.c_str()), tinyxml2::XML_SUCCESS) << "Failed to parse " << filename;
	DocumentReader reader(doc);
	auto lxfml = reader["LXFML"];
	ASSERT_TRUE(lxfml) << "No LXFML element found in " << filename;

	std::unordered_set<std::string> originalRigidSet;
	if (auto* rsParent = doc.FirstChildElement("LXFML")->FirstChildElement("RigidSystems")) {
		for (auto* rs = rsParent->FirstChildElement("RigidSystem"); rs; rs = rs->NextSiblingElement("RigidSystem")) {
			originalRigidSet.insert(SerializeElement(rs));
		}
	}

	std::unordered_set<std::string> originalGroupSet;
	if (auto* gsParent = doc.FirstChildElement("LXFML")->FirstChildElement("GroupSystems")) {
		for (auto* gs = gsParent->FirstChildElement("GroupSystem"); gs; gs = gs->NextSiblingElement("GroupSystem")) {
			for (auto* g = gs->FirstChildElement("Group"); g; g = g->NextSiblingElement("Group")) {
				// collect this group and nested groups
				std::function<void(tinyxml2::XMLElement*)> collectGroups = [&](tinyxml2::XMLElement* grp) {
					originalGroupSet.insert(SerializeElement(grp));
					for (auto* child = grp->FirstChildElement("Group"); child; child = child->NextSiblingElement("Group")) collectGroups(child);
				};
				collectGroups(g);
			}
		}
	}

	std::unordered_set<std::string> originalBricks;
	for (const auto& brick : lxfml["Bricks"]) {
		const auto* ref = brick.Attribute("refID");
		if (ref) originalBricks.insert(ref);
	}
	ASSERT_GT(originalBricks.size(), 0);

	// Collect bricks across all results and ensure no duplicates and all used
	std::unordered_set<std::string> usedBricks;
	// Track used rigid systems and groups (serialized strings)
	std::unordered_set<std::string> usedRigidSet;
	std::unordered_set<std::string> usedGroupSet;
	
	std::cout << "Original file contains " << originalBricks.size() << " bricks: ";
	for (const auto& brick : originalBricks) {
		std::cout << brick << " ";
	}
	std::cout << std::endl;
	
	int splitIndex = 0;
	std::filesystem::path baseFilename = std::filesystem::path(filename).stem();

	for (const auto& res : results) {
		splitIndex++;
		std::cout << "\n--- Split " << splitIndex << " ---" << std::endl;
		
		tinyxml2::XMLDocument outDoc;
		ASSERT_EQ(outDoc.Parse(res.lxfml.c_str()), tinyxml2::XML_SUCCESS);
		DocumentReader outReader(outDoc);
		auto outLxfml = outReader["LXFML"];
		ASSERT_TRUE(outLxfml);
		// collect rigid systems in this output
		if (auto* rsParent = outDoc.FirstChildElement("LXFML")->FirstChildElement("RigidSystems")) {
			for (auto* rs = rsParent->FirstChildElement("RigidSystem"); rs; rs = rs->NextSiblingElement("RigidSystem")) {
				auto s = SerializeElement(rs);
				// no duplicate allowed across outputs
				ASSERT_EQ(usedRigidSet.find(s), usedRigidSet.end()) << "Duplicate RigidSystem across splits";
				usedRigidSet.insert(s);
			}
		}
		// collect groups in this output
		if (auto* gsParent = outDoc.FirstChildElement("LXFML")->FirstChildElement("GroupSystems")) {
			for (auto* gs = gsParent->FirstChildElement("GroupSystem"); gs; gs = gs->NextSiblingElement("GroupSystem")) {
				for (auto* g = gs->FirstChildElement("Group"); g; g = g->NextSiblingElement("Group")) {
					std::function<void(tinyxml2::XMLElement*)> collectGroupsOut = [&](tinyxml2::XMLElement* grp) {
						auto s = SerializeElement(grp);
						ASSERT_EQ(usedGroupSet.find(s), usedGroupSet.end()) << "Duplicate Group across splits";
						usedGroupSet.insert(s);
						for (auto* child = grp->FirstChildElement("Group"); child; child = child->NextSiblingElement("Group")) collectGroupsOut(child);
					};
					collectGroupsOut(g);
				}
			}
		}
		
		// Collect and display bricks in this split
		std::vector<std::string> splitBricks;
		for (const auto& brick : outLxfml["Bricks"]) {
			const auto* ref = brick.Attribute("refID");
			if (ref) {
				// no duplicate allowed
				ASSERT_EQ(usedBricks.find(ref), usedBricks.end()) << "Duplicate brick ref across splits: " << ref;
				usedBricks.insert(ref);
				splitBricks.push_back(ref);
			}
		}
		
		std::cout << "Contains " << splitBricks.size() << " bricks: ";
		for (const auto& brick : splitBricks) {
			std::cout << brick << " ";
		}
		std::cout << std::endl;
		
		// Count rigid systems and groups
		int rigidCount = 0;
		if (auto* rsParent = outDoc.FirstChildElement("LXFML")->FirstChildElement("RigidSystems")) {
			for (auto* rs = rsParent->FirstChildElement("RigidSystem"); rs; rs = rs->NextSiblingElement("RigidSystem")) {
				rigidCount++;
			}
		}
		
		int groupCount = 0;
		if (auto* gsParent = outDoc.FirstChildElement("LXFML")->FirstChildElement("GroupSystems")) {
			for (auto* gs = gsParent->FirstChildElement("GroupSystem"); gs; gs = gs->NextSiblingElement("GroupSystem")) {
				for (auto* g = gs->FirstChildElement("Group"); g; g = g->NextSiblingElement("Group")) {
					groupCount++;
				}
			}
		}
		
		std::cout << "Contains " << rigidCount << " rigid systems and " << groupCount << " groups" << std::endl;
	}

	// Every original brick must be used in one of the outputs
	for (const auto& bref : originalBricks) {
		ASSERT_NE(usedBricks.find(bref), usedBricks.end()) << "Brick not used in splits: " << bref << " in " << filename;
	}

	// And usedBricks should not contain anything outside original
	for (const auto& ub : usedBricks) {
		ASSERT_NE(originalBricks.find(ub), originalBricks.end()) << "Split produced unknown brick: " << ub << " in " << filename;
	}

	// Ensure all original rigid systems and groups were used exactly once
	ASSERT_EQ(originalRigidSet.size(), usedRigidSet.size()) << "RigidSystem count mismatch in " << filename;
	for (const auto& s : originalRigidSet) ASSERT_NE(usedRigidSet.find(s), usedRigidSet.end()) << "RigidSystem missing in splits in " << filename;

	ASSERT_EQ(originalGroupSet.size(), usedGroupSet.size()) << "Group count mismatch in " << filename;
	for (const auto& s : originalGroupSet) ASSERT_NE(usedGroupSet.find(s), usedGroupSet.end()) << "Group missing in splits in " << filename;
}

TEST(LxfmlTests, SplitGroupIssueFile) {
	// Specific test for the group issue file
	TestSplitUsesAllBricksAndNoDuplicatesHelper("group_issue.lxfml");
}

TEST(LxfmlTests, SplitTestFile) {
	// Specific test for the larger test file
	TestSplitUsesAllBricksAndNoDuplicatesHelper("test.lxfml");
}

TEST(LxfmlTests, SplitComplexGroupingFile) {
	// Test for the complex grouping file - should produce only one split
	// because all groups are connected via rigid systems
	std::string data = ReadFile("complex_grouping.lxfml");
	ASSERT_FALSE(data.empty()) << "Failed to read complex_grouping.lxfml from build directory";
	
	std::cout << "\n=== Testing complex grouping file ===" << std::endl;
	
	auto results = Lxfml::Split(data);
	ASSERT_GT(results.size(), 0) << "Split results should not be empty";
	
	// The complex grouping file should produce exactly ONE split
	// because all groups share bricks through rigid systems
	if (results.size() != 1) {
		FAIL() << "Complex grouping file produced " << results.size() 
		       << " splits instead of 1 (all groups should be merged)";
	}
	
	std::cout << "✓ Correctly produced 1 merged split" << std::endl;
	
	// Verify the split contains all the expected elements
	tinyxml2::XMLDocument doc;
	ASSERT_EQ(doc.Parse(results[0].lxfml.c_str()), tinyxml2::XML_SUCCESS);
	
	auto* lxfml = doc.FirstChildElement("LXFML");
	ASSERT_NE(lxfml, nullptr);
	
	// Count bricks
	int brickCount = 0;
	if (auto* bricks = lxfml->FirstChildElement("Bricks")) {
		for (auto* brick = bricks->FirstChildElement("Brick"); brick; brick = brick->NextSiblingElement("Brick")) {
			brickCount++;
		}
	}
	std::cout << "Contains " << brickCount << " bricks" << std::endl;
	
	// Count rigid systems
	int rigidCount = 0;
	if (auto* rigidSystems = lxfml->FirstChildElement("RigidSystems")) {
		for (auto* rs = rigidSystems->FirstChildElement("RigidSystem"); rs; rs = rs->NextSiblingElement("RigidSystem")) {
			rigidCount++;
		}
	}
	std::cout << "Contains " << rigidCount << " rigid systems" << std::endl;
	EXPECT_GT(rigidCount, 0) << "Should contain rigid systems";
	
	// Count groups
	int groupCount = 0;
	if (auto* groupSystems = lxfml->FirstChildElement("GroupSystems")) {
		for (auto* gs = groupSystems->FirstChildElement("GroupSystem"); gs; gs = gs->NextSiblingElement("GroupSystem")) {
			for (auto* g = gs->FirstChildElement("Group"); g; g = g->NextSiblingElement("Group")) {
				groupCount++;
			}
		}
	}
	std::cout << "Contains " << groupCount << " groups" << std::endl;
	EXPECT_GT(groupCount, 1) << "Should contain multiple groups (all merged into one split)";
}

// Tests for invalid input handling - now working with the improved Split function

TEST(LxfmlTests, InvalidLxfmlHandling) {
	// Test LXFML with invalid transformation matrices
	std::string invalidTransformData = ReadFile("invalid_transform.lxfml");
	ASSERT_FALSE(invalidTransformData.empty()) << "Failed to read invalid_transform.lxfml from build directory";
	
	// The Split function should handle invalid transformation matrices gracefully
	std::vector<Lxfml::Result> results;
	EXPECT_NO_FATAL_FAILURE({
		results = Lxfml::Split(invalidTransformData);
	}) << "Split should not crash on invalid transformation matrices";
	
	// Function should handle invalid transforms gracefully, possibly returning empty or partial results
	// The exact behavior depends on how the function handles invalid numeric parsing
}

TEST(LxfmlTests, EmptyLxfmlHandling) {
	// Test with completely empty input
	std::string emptyData = "";
	std::vector<Lxfml::Result> results;
	
	EXPECT_NO_FATAL_FAILURE({
		results = Lxfml::Split(emptyData);
	}) << "Split should not crash on empty input";
	
	EXPECT_EQ(results.size(), 0) << "Empty input should return empty results";
}

TEST(LxfmlTests, EmptyTransformHandling) {
	// Test LXFML with empty transformation matrix
	std::string testData = ReadFile("empty_transform.lxfml");
	ASSERT_FALSE(testData.empty()) << "Failed to read empty_transform.lxfml from build directory";
	
	std::vector<Lxfml::Result> results;
	EXPECT_NO_FATAL_FAILURE({
		results = Lxfml::Split(testData);
	}) << "Split should not crash on empty transformation matrix";
	
	// The function should handle empty transforms gracefully
	// May return empty results or skip invalid bricks
}

TEST(LxfmlTests, TooFewValuesTransformHandling) {
	// Test LXFML with too few transformation values (needs 12, has fewer)
	std::string testData = ReadFile("too_few_values.lxfml");
	ASSERT_FALSE(testData.empty()) << "Failed to read too_few_values.lxfml from build directory";
	
	std::vector<Lxfml::Result> results;
	EXPECT_NO_FATAL_FAILURE({
		results = Lxfml::Split(testData);
	}) << "Split should not crash on transformation matrix with too few values";
	
	// The function should handle incomplete transforms gracefully
	// May return empty results or skip invalid bricks
}

TEST(LxfmlTests, NonNumericTransformHandling) {
	// Test LXFML with non-numeric transformation values
	std::string testData = ReadFile("non_numeric_transform.lxfml");
	ASSERT_FALSE(testData.empty()) << "Failed to read non_numeric_transform.lxfml from build directory";
	
	std::vector<Lxfml::Result> results;
	EXPECT_NO_FATAL_FAILURE({
		results = Lxfml::Split(testData);
	}) << "Split should not crash on non-numeric transformation values";
	
	// The function should handle non-numeric transforms gracefully
	// May return empty results or skip invalid bricks
}

TEST(LxfmlTests, MixedInvalidTransformHandling) {
	// Test LXFML with mixed valid/invalid transformation values within a matrix
	std::string testData = ReadFile("mixed_invalid_transform.lxfml");
	ASSERT_FALSE(testData.empty()) << "Failed to read mixed_invalid_transform.lxfml from build directory";
	
	std::vector<Lxfml::Result> results;
	EXPECT_NO_FATAL_FAILURE({
		results = Lxfml::Split(testData);
	}) << "Split should not crash on mixed valid/invalid transformation values";
	
	// The function should handle mixed valid/invalid transforms gracefully
	// May return empty results or skip invalid bricks
}

TEST(LxfmlTests, NoBricksHandling) {
	// Test LXFML with no Bricks section (should return empty gracefully)
	std::string testData = ReadFile("no_bricks.lxfml");
	ASSERT_FALSE(testData.empty()) << "Failed to read no_bricks.lxfml from build directory";
	
	std::vector<Lxfml::Result> results;
	EXPECT_NO_FATAL_FAILURE({
		results = Lxfml::Split(testData);
	}) << "Split should not crash on LXFML with no Bricks section";
	
	// Should return empty results gracefully when no bricks are present
	EXPECT_EQ(results.size(), 0) << "LXFML with no bricks should return empty results";
}

TEST(LxfmlTests, MixedValidInvalidTransformsHandling) {
	// Test LXFML with mix of valid and invalid transformation data
	std::string mixedValidData = ReadFile("mixed_valid_invalid.lxfml");
	ASSERT_FALSE(mixedValidData.empty()) << "Failed to read mixed_valid_invalid.lxfml from build directory";
	
	// The Split function should handle mixed valid/invalid transforms gracefully
	std::vector<Lxfml::Result> results;
	EXPECT_NO_FATAL_FAILURE({
		results = Lxfml::Split(mixedValidData);
	}) << "Split should not crash on mixed valid/invalid transforms";
	
	// Should process valid bricks and handle invalid ones gracefully
	if (results.size() > 0) {
		EXPECT_NO_FATAL_FAILURE({
			for (size_t i = 0; i < results.size(); ++i) {
				// Each result should have valid LXFML structure
				tinyxml2::XMLDocument doc;
				auto parseResult = doc.Parse(results[i].lxfml.c_str());
				EXPECT_EQ(parseResult, tinyxml2::XML_SUCCESS) 
					<< "Result " << i << " should produce valid XML";
					
				if (parseResult == tinyxml2::XML_SUCCESS) {
					auto* lxfml = doc.FirstChildElement("LXFML");
					EXPECT_NE(lxfml, nullptr) << "Result " << i << " should have LXFML root element";
				}
			}
		}) << "Mixed valid/invalid transform processing should not cause fatal errors";
	}
}

TEST(LxfmlTests, DeepCloneDepthProtection) {
	// Test that deep cloning has protection against excessive nesting
	std::string deeplyNestedLxfml = ReadFile("deeply_nested.lxfml");
	ASSERT_FALSE(deeplyNestedLxfml.empty()) << "Failed to read deeply_nested.lxfml from build directory";
	
	// The Split function should handle deeply nested structures without hanging
	std::vector<Lxfml::Result> results;
	EXPECT_NO_FATAL_FAILURE({
		results = Lxfml::Split(deeplyNestedLxfml);
	}) << "Split should not hang or crash on deeply nested XML structures";
	
	// Should still produce valid output despite depth limitations
	EXPECT_GT(results.size(), 0) << "Should produce at least one result even with deep nesting";
	
	if (results.size() > 0) {
		// Verify the result is still valid XML
		tinyxml2::XMLDocument doc;
		auto parseResult = doc.Parse(results[0].lxfml.c_str());
		EXPECT_EQ(parseResult, tinyxml2::XML_SUCCESS) << "Result should still be valid XML";
		
		if (parseResult == tinyxml2::XML_SUCCESS) {
			auto* lxfml = doc.FirstChildElement("LXFML");
			EXPECT_NE(lxfml, nullptr) << "Result should have LXFML root element";
			
			// Verify that bricks are still included despite group nesting issues
			auto* bricks = lxfml->FirstChildElement("Bricks");
			EXPECT_NE(bricks, nullptr) << "Bricks element should be present";
			if (bricks) {
				auto* brick = bricks->FirstChildElement("Brick");
				EXPECT_NE(brick, nullptr) << "At least one brick should be present";
			}
		}
	}
}
