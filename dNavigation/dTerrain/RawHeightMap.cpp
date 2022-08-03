#include "RawHeightMap.h"

#include "BinaryIO.h"

RawHeightMap::RawHeightMap() {}

RawHeightMap::RawHeightMap(std::ifstream& stream, float height, float width) {
	// Read in height map data header and scale

	BinaryIO::BinaryRead(stream, m_Unknown1);
	BinaryIO::BinaryRead(stream, m_Unknown2);
	BinaryIO::BinaryRead(stream, m_Unknown3);
	BinaryIO::BinaryRead(stream, m_Unknown4);
	BinaryIO::BinaryRead(stream, m_ScaleFactor);

	// read all vertices in

	for (uint64_t i = 0; i < width * height; i++) {
		float value;
		BinaryIO::BinaryRead(stream, value);
		m_FloatMap.push_back(value);
	}
}

RawHeightMap::~RawHeightMap() {

}
