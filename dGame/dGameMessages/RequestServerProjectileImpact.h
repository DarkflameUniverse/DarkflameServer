#ifndef __REQUESTSERVERPROJECTILEIMPACT__H__
#define __REQUESTSERVERPROJECTILEIMPACT__H__

#include "dCommonVars.h"
#include "eGameMessageType.h"

/*  Notifying the server that a locally owned projectile impacted. Sent to the caster of the projectile
		should always be the local char. */
class RequestServerProjectileImpact {
public:
	RequestServerProjectileImpact() {
		i64LocalID = LWOOBJID_EMPTY;
		i64TargetID = LWOOBJID_EMPTY;
	}

	RequestServerProjectileImpact(std::string _sBitStream, LWOOBJID _i64LocalID = LWOOBJID_EMPTY, LWOOBJID _i64TargetID = LWOOBJID_EMPTY) {
		i64LocalID = _i64LocalID;
		i64TargetID = _i64TargetID;
		sBitStream = _sBitStream;
	}

	RequestServerProjectileImpact(RakNet::BitStream* stream) : RequestServerProjectileImpact() {
		Deserialize(stream);
	}

	~RequestServerProjectileImpact() {
	}

	void Serialize(RakNet::BitStream* stream) {
		stream->Write(eGameMessageType::REQUEST_SERVER_PROJECTILE_IMPACT);

		stream->Write(i64LocalID != LWOOBJID_EMPTY);
		if (i64LocalID != LWOOBJID_EMPTY) stream->Write(i64LocalID);

		stream->Write(i64TargetID != LWOOBJID_EMPTY);
		if (i64TargetID != LWOOBJID_EMPTY) stream->Write(i64TargetID);

		uint32_t sBitStreamLength = sBitStream.length();
		stream->Write(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			stream->Write(sBitStream[k]);
		}

	}

	bool Deserialize(RakNet::BitStream* stream) {
		bool i64LocalIDIsDefault{};
		stream->Read(i64LocalIDIsDefault);
		if (i64LocalIDIsDefault != 0) stream->Read(i64LocalID);

		bool i64TargetIDIsDefault{};
		stream->Read(i64TargetIDIsDefault);
		if (i64TargetIDIsDefault != 0) stream->Read(i64TargetID);

		uint32_t sBitStreamLength{};
		stream->Read(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			stream->Read(character);
			sBitStream.push_back(character);
		}


		return true;
	}

	LWOOBJID i64LocalID;
	LWOOBJID i64TargetID;
	std::string sBitStream;
};

#endif  //!__REQUESTSERVERPROJECTILEIMPACT__H__
