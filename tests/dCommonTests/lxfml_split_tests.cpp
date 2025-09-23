#include "gtest/gtest.h"

#include "Lxfml.h"
#include "TinyXmlUtils.h"

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

TEST(LxfmlSplitTests, SplitUsesAllBricksAndNoDuplicates) {
    // Read the sample test.lxfml included in tests. Resolve path relative to this source file.
    std::filesystem::path srcDir = std::filesystem::path(__FILE__).parent_path();
    std::filesystem::path filePath = srcDir / "test.lxfml";
    std::string data = ReadFile(filePath.string());
    ASSERT_FALSE(data.empty()) << "Failed to read " << filePath.string();
    

    auto results = Lxfml::Split(data);
    ASSERT_GT(results.size(), 0);

    // Write split outputs to disk for manual inspection
    std::filesystem::path outDir = srcDir / "lxfml_splits";
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
