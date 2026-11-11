#include <gtest/gtest.h>

#include <cstring>

#include "Sd0.h"
#include "ZCompression.h"
#include "ZoneFilePack.h"

using namespace ZoneFilePack;

namespace {
	template<typename T>
	void Put(std::string& out, T value) { out.append(reinterpret_cast<const char*>(&value), sizeof(T)); }

	std::string Wide(const std::string& text) {
		std::string out;
		for (const char c : text) { out += c; out += '\0'; }
		out.append(2, '\0');
		return out;
	}

	// "hello" as a client sends it: the sd0 header, one chunk's length, the zlib stream (level 9)
	const std::string HELLO_SD0("sd0\x01\xff" "\x0d\x00\x00\x00" "\x78\xda\xcb\x48\xcd\xc9\xc9\x07\x00\x06\x2c\x02\x15", 22);

	std::string EntryBytes(const std::string& name, const std::string& sd0) {
		std::string out;
		Put<uint32_t>(out, static_cast<uint32_t>((name.size() + 1) * 2));
		Put<uint32_t>(out, static_cast<uint32_t>(sd0.size()));
		return out + Wide(name) + sd0;
	}

	// One chunk of sd0 holding `data`, whatever its size
	std::string OneChunk(const std::string& data) {
		std::string compressed(ZCompression::GetMaxCompressedLength(static_cast<uint32_t>(data.size())), '\0');
		const auto size = ZCompression::Compress(reinterpret_cast<const uint8_t*>(data.data()), static_cast<uint32_t>(data.size()),
			reinterpret_cast<uint8_t*>(compressed.data()), static_cast<uint32_t>(compressed.size()));
		std::string out("sd0\x01\xff", 5);
		Put<uint32_t>(out, static_cast<uint32_t>(size));
		return out + compressed.substr(0, static_cast<size_t>(size));
	}
}

// What a client sends for one scene, byte for byte
TEST(ZoneFilePackTests, UnpacksOneFile) {
	std::string body;
	Put<uint32_t>(body, 12);  // "a.lvl" and its null, in UTF-16
	Put<uint32_t>(body, 22);  // the sd0 data
	body += std::string("a\0.\0l\0v\0l\0\0\0", 12);
	body += HELLO_SD0;
	ASSERT_EQ(body.size(), 8u + 12u + 22u);

	Files files;
	ASSERT_EQ(Unpack(eFileType::SCENE, body, {}, files), eError::NONE);
	ASSERT_EQ(files.scenes.size(), 1u);
	EXPECT_EQ(files.scenes[0].name, "a.lvl");
	EXPECT_EQ(files.scenes[0].data, "hello");
	EXPECT_FALSE(files.zone || files.terrain);

	// The other single kinds land in their own place
	ASSERT_EQ(Unpack(eFileType::TERRAIN, body, {}, files), eError::NONE);
	EXPECT_EQ(files.terrain->data, "hello");
	ASSERT_EQ(Unpack(eFileType::ZONE, body, {}, files), eError::NONE);
	EXPECT_EQ(files.zone->data, "hello");
	ASSERT_EQ(Unpack(eFileType::TRIGGERS, body, {}, files), eError::NONE);
	EXPECT_EQ(files.triggers[0].data, "hello");
}

// A whole map: the zone, the terrain, a u16 count of scenes, a u16 count of triggers
TEST(ZoneFilePackTests, UnpacksAWholeMap) {
	std::string body = EntryBytes("z.luz", HELLO_SD0) + EntryBytes("z.raw", HELLO_SD0);
	Put<uint16_t>(body, 2);
	body += EntryBytes("a.lvl", HELLO_SD0) + EntryBytes("b.lvl", HELLO_SD0);
	Put<uint16_t>(body, 1);
	body += EntryBytes("a.lutriggers", HELLO_SD0);

	Files files;
	ASSERT_EQ(Unpack(eFileType::WHOLE_MAP, body, {}, files), eError::NONE);
	EXPECT_EQ(files.zone->name, "z.luz");
	EXPECT_EQ(files.terrain->name, "z.raw");
	ASSERT_EQ(files.scenes.size(), 2u);
	EXPECT_EQ(files.scenes[1].name, "b.lvl");
	ASSERT_EQ(files.triggers.size(), 1u);
	EXPECT_EQ(files.triggers[0].name, "a.lutriggers");
	EXPECT_EQ(files.Count(), 5u);

	// No triggers is a count of 0, not a missing count
	std::string none = EntryBytes("z.luz", HELLO_SD0) + EntryBytes("z.raw", HELLO_SD0);
	Put<uint16_t>(none, 0);
	EXPECT_EQ(Unpack(eFileType::WHOLE_MAP, none, {}, files), eError::TRUNCATED);
	Put<uint16_t>(none, 0);
	EXPECT_EQ(Unpack(eFileType::WHOLE_MAP, none, {}, files), eError::NONE);

	// A zone or a scene without data is not a whole map
	std::string empty = EntryBytes("z.luz", std::string("sd0\x01\xff", 5)) + EntryBytes("z.raw", HELLO_SD0);
	Put<uint16_t>(empty, 0);
	Put<uint16_t>(empty, 0);
	EXPECT_EQ(Unpack(eFileType::WHOLE_MAP, empty, {}, files), eError::INCOMPLETE);
}

// What the server sends back, byte for byte: the same framing, the data as sd0
TEST(ZoneFilePackTests, PacksOneFileAndAWholeMap) {
	Files files;
	files.zone = Entry{ "z.luz", "zone bytes" };
	files.terrain = Entry{ "z.raw", "terrain bytes" };
	files.scenes = { { "a.lvl", "hello" } };
	files.triggers = { { "a.lutriggers", "<triggers/>" } };

	const auto sd0 = [](const std::string& data) { return Sd0::Compress(data); };
	EXPECT_EQ(Pack(eFileType::SCENE, files), EntryBytes("a.lvl", sd0("hello")));
	EXPECT_EQ(Pack(eFileType::TERRAIN, files), EntryBytes("z.raw", sd0("terrain bytes")));
	EXPECT_EQ(Pack(eFileType::ZONE, files), EntryBytes("z.luz", sd0("zone bytes")));
	EXPECT_EQ(Pack(eFileType::TRIGGERS, files), EntryBytes("a.lutriggers", sd0("<triggers/>")));

	std::string whole = EntryBytes("z.luz", sd0("zone bytes")) + EntryBytes("z.raw", sd0("terrain bytes"));
	Put<uint16_t>(whole, 1);
	whole += EntryBytes("a.lvl", sd0("hello"));
	Put<uint16_t>(whole, 1);
	whole += EntryBytes("a.lutriggers", sd0("<triggers/>"));
	EXPECT_EQ(Pack(eFileType::WHOLE_MAP, files), whole);

	// The data is sd0: its header, a chunk length, a zlib stream
	const auto packed = Pack(eFileType::SCENE, files);
	EXPECT_EQ(packed.substr(8 + 12, 5), std::string("sd0\x01\xff", 5));
	EXPECT_EQ(static_cast<unsigned char>(packed[8 + 12 + 9]), 0x78);

	// And it comes back out
	Files read;
	ASSERT_EQ(Unpack(eFileType::WHOLE_MAP, whole, {}, read), eError::NONE);
	EXPECT_EQ(read.zone->data, "zone bytes");
	EXPECT_EQ(read.triggers[0].data, "<triggers/>");

	// A kind that is not there packs to nothing
	EXPECT_TRUE(Pack(eFileType::SCENE, Files{}).empty());
	EXPECT_TRUE(Pack(eFileType::WHOLE_MAP, Files{}).empty());
}

// A file larger than one chunk is several chunks, each at most 256 KiB
TEST(ZoneFilePackTests, LargeFilesAreChunked) {
	std::string big(700 * 1024, '\0');
	for (size_t i = 0; i < big.size(); ++i) big[i] = static_cast<char>((i * 7 + i / 513) & 0xFF);
	Files files;
	files.scenes = { { "big.lvl", big } };
	Files read;
	ASSERT_EQ(Unpack(eFileType::SCENE, Pack(eFileType::SCENE, files), {}, read), eError::NONE);
	EXPECT_TRUE(read.scenes[0].data == big);
}

TEST(ZoneFilePackTests, TruncatedEntries) {
	const auto whole = EntryBytes("a.lvl", HELLO_SD0);
	Files files;
	for (const size_t length : { size_t{ 0 }, size_t{ 3 }, size_t{ 8 }, size_t{ 15 }, size_t{ 25 }, whole.size() - 1 }) {
		EXPECT_EQ(Unpack(eFileType::SCENE, whole.substr(0, length), {}, files), eError::TRUNCATED) << length;
	}
	// Bytes after the one entry
	EXPECT_EQ(Unpack(eFileType::SCENE, whole + "x", {}, files), eError::TRUNCATED);
	// A data length far past the end
	std::string lying;
	Put<uint32_t>(lying, 12);
	Put<uint32_t>(lying, 0xFFFFFFF0);
	lying += Wide("a.lvl") + HELLO_SD0;
	EXPECT_EQ(Unpack(eFileType::SCENE, lying, {}, files), eError::TRUNCATED);
	// A chunk length past the end of the entry's data
	auto cut = HELLO_SD0;
	cut.resize(cut.size() - 4);
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("a.lvl", cut), {}, files), eError::TRUNCATED);
	// A whole map that stops after its terrain
	EXPECT_EQ(Unpack(eFileType::WHOLE_MAP, EntryBytes("z.luz", HELLO_SD0) + EntryBytes("z.raw", HELLO_SD0), {}, files), eError::TRUNCATED);
}

TEST(ZoneFilePackTests, DataWithoutTheSd0HeaderIsRefused) {
	Files files;
	// The bare zlib stream, and plain bytes
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("a.lvl", HELLO_SD0.substr(9)), {}, files), eError::NOT_SD0);
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("a.lvl", "hello"), {}, files), eError::NOT_SD0);
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("a.lvl", ""), {}, files), eError::NOT_SD0);
	// The header and a chunk that is not zlib
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("a.lvl", std::string("sd0\x01\xff\x04\x00\x00\x00junk", 13)), {}, files), eError::BAD_CHUNK);
}

TEST(ZoneFilePackTests, OversizedChunksAndFilesAreRefused) {
	Files files;
	// One chunk that inflates to a byte more than 256 KiB
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("a.lvl", OneChunk(std::string(Sd0::MAX_UNCOMPRESSED_CHUNK_SIZE, 'x'))), {}, files), eError::NONE);
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("a.lvl", OneChunk(std::string(Sd0::MAX_UNCOMPRESSED_CHUNK_SIZE + 1, 'x'))), {}, files), eError::BAD_CHUNK);
	// A chunk whose compressed length is itself too large
	std::string huge("sd0\x01\xff", 5);
	Put<uint32_t>(huge, 64 * 1024 * 1024);
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("a.lvl", huge), {}, files), eError::BAD_CHUNK);

	// Many small chunks that add up to more than a file may be
	Limits limits;
	limits.maxFileBytes = 12;
	std::string three = HELLO_SD0 + HELLO_SD0.substr(5) + HELLO_SD0.substr(5); // 15 bytes in 3 chunks
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("a.lvl", three), limits, files), eError::TOO_LARGE);
	limits.maxFileBytes = 15;
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("a.lvl", three), limits, files), eError::NONE);
	EXPECT_EQ(files.scenes[0].data, "hellohellohello");

	// All the files together, and how many there are
	std::string whole = EntryBytes("z.luz", HELLO_SD0) + EntryBytes("z.raw", HELLO_SD0);
	Put<uint16_t>(whole, 1);
	whole += EntryBytes("a.lvl", HELLO_SD0);
	Put<uint16_t>(whole, 0);
	limits = {};
	limits.maxTotalBytes = 14;
	EXPECT_EQ(Unpack(eFileType::WHOLE_MAP, whole, limits, files), eError::TOO_LARGE);
	limits = {};
	limits.maxFiles = 2;
	EXPECT_EQ(Unpack(eFileType::WHOLE_MAP, whole, limits, files), eError::TOO_LARGE);
	// A count that promises more entries than there are bytes
	std::string many = EntryBytes("z.luz", HELLO_SD0) + EntryBytes("z.raw", HELLO_SD0);
	Put<uint16_t>(many, 100);
	EXPECT_EQ(Unpack(eFileType::WHOLE_MAP, many, {}, files), eError::TRUNCATED);
}

// A name's folder part is dropped, as the client drops it: only the base name is ever looked at
TEST(ZoneFilePackTests, NamesAreBaseNamesOnly) {
	EXPECT_EQ(BaseName("a.lvl"), "a.lvl");
	EXPECT_EQ(BaseName("maps/zone/a.lvl"), "a.lvl");
	EXPECT_EQ(BaseName("..\\..\\windows\\a.lvl"), "a.lvl");
	EXPECT_EQ(BaseName("../../../etc/passwd"), "passwd");
	EXPECT_EQ(BaseName("C:\\maps\\a.lvl"), "a.lvl");
	EXPECT_FALSE(BaseName(""));
	EXPECT_FALSE(BaseName(".."));
	EXPECT_FALSE(BaseName("maps/.."));
	EXPECT_FALSE(BaseName("maps/"));
	EXPECT_FALSE(BaseName("maps\\."));
	EXPECT_FALSE(BaseName("C:a.lvl"));
	EXPECT_FALSE(BaseName("a.lvl."));
	EXPECT_FALSE(BaseName(std::string("a\0.lvl", 6)));
	EXPECT_FALSE(BaseName("a\n.lvl"));
	EXPECT_FALSE(BaseName("\xC3\xA9.lvl"));

	Files files;
	ASSERT_EQ(Unpack(eFileType::SCENE, EntryBytes("..\\other_zone/a.lvl", HELLO_SD0), {}, files), eError::NONE);
	EXPECT_EQ(files.scenes[0].name, "a.lvl");
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("maps/..", HELLO_SD0), {}, files), eError::BAD_NAME);
	EXPECT_EQ(Unpack(eFileType::SCENE, EntryBytes("", HELLO_SD0), {}, files), eError::BAD_NAME);

	// A name that is not UTF-16 with its null, or not ASCII
	std::string odd;
	Put<uint32_t>(odd, 5);
	Put<uint32_t>(odd, 22);
	odd += std::string("a\0b\0c", 5) + HELLO_SD0;
	EXPECT_EQ(Unpack(eFileType::SCENE, odd, {}, files), eError::BAD_NAME);
	std::string unterminated;
	Put<uint32_t>(unterminated, 4);
	Put<uint32_t>(unterminated, 22);
	unterminated += std::string("a\0b\0", 4) + HELLO_SD0;
	EXPECT_EQ(Unpack(eFileType::SCENE, unterminated, {}, files), eError::BAD_NAME);
	std::string wide;
	Put<uint32_t>(wide, 4);
	Put<uint32_t>(wide, 22);
	wide += std::string("\x42\x04\0\0", 4) + HELLO_SD0;
	EXPECT_EQ(Unpack(eFileType::SCENE, wide, {}, files), eError::BAD_NAME);
	std::string embedded;
	Put<uint32_t>(embedded, 6);
	Put<uint32_t>(embedded, 22);
	embedded += std::string("a\0\0\0\0\0", 6) + HELLO_SD0;
	EXPECT_EQ(Unpack(eFileType::SCENE, embedded, {}, files), eError::BAD_NAME);

	EXPECT_TRUE(ValidType(0));
	EXPECT_TRUE(ValidType(4));
	EXPECT_FALSE(ValidType(5));
}
