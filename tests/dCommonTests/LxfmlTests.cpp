#include "gtest/gtest.h"

#include "Lxfml.h"
#include "TinyXmlUtils.h"
#include "dCommonDependencies.h"

#include <fstream>
#include <sstream>
#include <unordered_set>
#include <filesystem>

using namespace TinyXmlUtils;

static std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

TEST(LxfmlTests, SplitUsesAllBricksAndNoDuplicates) {
    // Read the sample test.lxfml included in tests. Resolve path relative to this source file.
    std::filesystem::path srcDir = std::filesystem::path(__FILE__).parent_path();
    std::filesystem::path filePath = srcDir / "LxfmlTestFiles" / "test.lxfml";
	std::ifstream in(filePath, std::ios::in | std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string data = ss.str();
    ASSERT_FALSE(data.empty()) << "Failed to read " << filePath.string();
    

    auto results = Lxfml::Split(data);
    ASSERT_GT(results.size(), 0);

    // Write split outputs to disk for manual inspection
    std::filesystem::path outDir = srcDir / "LxfmlTestFiles" / "lxfml_splits";
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    for (size_t i = 0; i < results.size(); ++i) {
        auto outPath = outDir / ("split_" + std::to_string(i) + ".lxfml");
        std::ofstream ofs(outPath, std::ios::out | std::ios::binary);
        ASSERT_TRUE(ofs) << "Failed to open output file: " << outPath.string();
        ofs << results[i].lxfml;
        ofs.close();
    }

    // parse original to count bricks
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(data.c_str()), tinyxml2::XML_SUCCESS);
    DocumentReader reader(doc);
    auto lxfml = reader["LXFML"];
    ASSERT_TRUE(lxfml);

    // Collect original RigidSystems and Groups (serialize each element string)
    auto serializeElement = [](tinyxml2::XMLElement* elem) {
        tinyxml2::XMLPrinter p;
        elem->Accept(&p);
        return std::string(p.CStr());
    };

    std::unordered_set<std::string> originalRigidSet;
    if (auto* rsParent = doc.FirstChildElement("LXFML")->FirstChildElement("RigidSystems")) {
        for (auto* rs = rsParent->FirstChildElement("RigidSystem"); rs; rs = rs->NextSiblingElement("RigidSystem")) {
            originalRigidSet.insert(serializeElement(rs));
        }
    }

    std::unordered_set<std::string> originalGroupSet;
    if (auto* gsParent = doc.FirstChildElement("LXFML")->FirstChildElement("GroupSystems")) {
        for (auto* gs = gsParent->FirstChildElement("GroupSystem"); gs; gs = gs->NextSiblingElement("GroupSystem")) {
            for (auto* g = gs->FirstChildElement("Group"); g; g = g->NextSiblingElement("Group")) {
                // collect this group and nested groups
                std::function<void(tinyxml2::XMLElement*)> collectGroups = [&](tinyxml2::XMLElement* grp) {
                    originalGroupSet.insert(serializeElement(grp));
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
    for (const auto& res : results) {
        tinyxml2::XMLDocument outDoc;
        ASSERT_EQ(outDoc.Parse(res.lxfml.c_str()), tinyxml2::XML_SUCCESS);
        DocumentReader outReader(outDoc);
        auto outLxfml = outReader["LXFML"];
        ASSERT_TRUE(outLxfml);
        // collect rigid systems in this output
        if (auto* rsParent = outDoc.FirstChildElement("LXFML")->FirstChildElement("RigidSystems")) {
            for (auto* rs = rsParent->FirstChildElement("RigidSystem"); rs; rs = rs->NextSiblingElement("RigidSystem")) {
                auto s = serializeElement(rs);
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
                        auto s = serializeElement(grp);
                        ASSERT_EQ(usedGroupSet.find(s), usedGroupSet.end()) << "Duplicate Group across splits";
                        usedGroupSet.insert(s);
                        for (auto* child = grp->FirstChildElement("Group"); child; child = child->NextSiblingElement("Group")) collectGroupsOut(child);
                    };
                    collectGroupsOut(g);
                }
            }
        }
        for (const auto& brick : outLxfml["Bricks"]) {
            const auto* ref = brick.Attribute("refID");
            if (ref) {
                // no duplicate allowed
                ASSERT_EQ(usedBricks.find(ref), usedBricks.end()) << "Duplicate brick ref across splits: " << ref;
                usedBricks.insert(ref);
            }
        }
    }

    // Every original brick must be used in one of the outputs
    for (const auto& bref : originalBricks) {
        ASSERT_NE(usedBricks.find(bref), usedBricks.end()) << "Brick not used in splits: " << bref;
    }

    // And usedBricks should not contain anything outside original
    for (const auto& ub : usedBricks) {
        ASSERT_NE(originalBricks.find(ub), originalBricks.end()) << "Split produced unknown brick: " << ub;
    }

    // Ensure all original rigid systems and groups were used exactly once
    ASSERT_EQ(originalRigidSet.size(), usedRigidSet.size()) << "RigidSystem count mismatch";
    for (const auto& s : originalRigidSet) ASSERT_NE(usedRigidSet.find(s), usedRigidSet.end()) << "RigidSystem missing in splits";

    ASSERT_EQ(originalGroupSet.size(), usedGroupSet.size()) << "Group count mismatch";
    for (const auto& s : originalGroupSet) ASSERT_NE(usedGroupSet.find(s), usedGroupSet.end()) << "Group missing in splits";
}

// Tests for invalid input handling - now working with the improved Split function

TEST(LxfmlTests, InvalidLxfmlHandling) {
    // Test LXFML with invalid transformation matrices
    std::string invalidTransformData = R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<LXFML versionMajor="5" versionMinor="0">
    <Meta>
        <Application name="LEGO Universe" versionMajor="0" versionMinor="0"/>
        <Brand name="LEGOUniverse"/>
        <BrickSet version="457"/>
    </Meta>
    <Bricks>
        <Brick refID="0" designID="74340">
            <Part refID="0" designID="3679" materials="23">
                <Bone refID="0" transformation="invalid,matrix,with,text,values,here,not,numbers,at,all,fails,parse"/>
            </Part>
        </Brick>
        <Brick refID="1" designID="41533">
            <Part refID="1" designID="41533" materials="23">
                <Bone refID="1" transformation="1,2,3"/>
            </Part>
        </Brick>
    </Bricks>
</LXFML>)";
    
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

TEST(LxfmlTests, InvalidTransformHandling) {
    // Test with various types of invalid transformation matrices
    std::vector<std::string> invalidTransformTests = {
        // LXFML with empty transformation
        R"(<?xml version="1.0"?><LXFML versionMajor="5" versionMinor="0"><Meta></Meta><Bricks><Brick refID="0" designID="74340"><Part refID="0" designID="3679"><Bone refID="0" transformation=""/></Part></Brick></Bricks></LXFML>)",
        
        // LXFML with too few transformation values (needs 12, has 6)
        R"(<?xml version="1.0"?><LXFML versionMajor="5" versionMinor="0"><Meta></Meta><Bricks><Brick refID="0" designID="74340"><Part refID="0" designID="3679"><Bone refID="0" transformation="1,0,0,0,1,0"/></Part></Brick></Bricks></LXFML>)",
        
        // LXFML with non-numeric transformation values
        R"(<?xml version="1.0"?><LXFML versionMajor="5" versionMinor="0"><Meta></Meta><Bricks><Brick refID="0" designID="74340"><Part refID="0" designID="3679"><Bone refID="0" transformation="a,b,c,d,e,f,g,h,i,j,k,l"/></Part></Brick></Bricks></LXFML>)",
        
        // LXFML with mixed valid/invalid transformation values
        R"(<?xml version="1.0"?><LXFML versionMajor="5" versionMinor="0"><Meta></Meta><Bricks><Brick refID="0" designID="74340"><Part refID="0" designID="3679"><Bone refID="0" transformation="1,0,invalid,0,1,0,0,0,1,10,20,30"/></Part></Brick></Bricks></LXFML>)",
        
        // LXFML with no Bricks section (should return empty gracefully)
        R"(<?xml version="1.0"?><LXFML versionMajor="5" versionMinor="0"><Meta></Meta></LXFML>)"
    };
    
    for (size_t i = 0; i < invalidTransformTests.size(); ++i) {
        std::vector<Lxfml::Result> results;
        EXPECT_NO_FATAL_FAILURE({
            results = Lxfml::Split(invalidTransformTests[i]);
        }) << "Split should not crash on invalid transform test case " << i;
        
        // The function should handle invalid transforms gracefully
        // May return empty results or skip invalid bricks
    }
}

TEST(LxfmlTests, MixedValidInvalidTransformsHandling) {
    // Test LXFML with mix of valid and invalid transformation data
    std::string mixedValidData = R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<LXFML versionMajor="5" versionMinor="0">
    <Meta>
        <Application name="LEGO Universe" versionMajor="0" versionMinor="0"/>
        <Brand name="LEGOUniverse"/>
        <BrickSet version="457"/>
    </Meta>
    <Bricks>
        <Brick refID="0" designID="74340">
            <Part refID="0" designID="3679" materials="23">
                <Bone refID="0" transformation="1,0,0,0,1,0,0,0,1,0,0,0"/>
            </Part>
        </Brick>
        <Brick refID="1" designID="41533">
            <Part refID="1" designID="41533" materials="23">
                <Bone refID="1" transformation="invalid,transform,here,bad,values,foo,bar,baz,qux,0,0,0"/>
            </Part>
        </Brick>
        <Brick refID="2" designID="74340">
            <Part refID="2" designID="3679" materials="23">
                <Bone refID="2" transformation="1,0,0,0,1,0,0,0,1,10,20,30"/>
            </Part>
        </Brick>
        <Brick refID="3" designID="41533">
            <Part refID="3" designID="41533" materials="23">
                <Bone refID="3" transformation="1,2,3"/>
            </Part>
        </Brick>
    </Bricks>
    <RigidSystems>
        <RigidSystem>
            <Rigid boneRefs="0,2"/>
        </RigidSystem>
        <RigidSystem>
            <Rigid boneRefs="1,3"/>
        </RigidSystem>
    </RigidSystems>
    <GroupSystems>
        <GroupSystem>
            <Group partRefs="0,2"/>
            <Group partRefs="1,3"/>
        </GroupSystem>
    </GroupSystems>
</LXFML>)";
    
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
    // Create a deeply nested XML structure that would exceed reasonable limits
    std::string deeplyNestedLxfml = R"(<?xml version="1.0" encoding="UTF-8"?>
<LXFML versionMajor="5" versionMinor="0">
    <Meta>
        <Application name="LEGO Universe" versionMajor="0" versionMinor="0"/>
        <Brand name="LEGOUniverse"/>
        <BrickSet version="457"/>
    </Meta>
    <Bricks>
        <Brick refID="0" designID="3001">
            <Part refID="0" designID="3001" materials="23">
                <Bone refID="0" transformation="1,0,0,0,1,0,0,0,1,0,0,0"/>
            </Part>
        </Brick>
    </Bricks>
    <RigidSystems>
    </RigidSystems>
    <GroupSystems>
        <GroupSystem>
            <Group partRefs="0">
                <Group partRefs="0">
                    <Group partRefs="0">
                        <Group partRefs="0">
                            <Group partRefs="0">
                                <Group partRefs="0">
                                    <Group partRefs="0">
                                        <Group partRefs="0">
                                            <Group partRefs="0">
                                                <Group partRefs="0">
                                                    <Group partRefs="0">
                                                        <Group partRefs="0">
                                                            <Group partRefs="0">
                                                                <Group partRefs="0">
                                                                    <Group partRefs="0"/>
                                                                </Group>
                                                            </Group>
                                                        </Group>
                                                    </Group>
                                                </Group>
                                            </Group>
                                        </Group>
                                    </Group>
                                </Group>
                            </Group>
                        </Group>
                    </Group>
                </Group>
            </Group>
        </GroupSystem>
    </GroupSystems>
</LXFML>)";
    
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
