#ifndef __DOCLIENTPROJECTILEIMPACT__H__
#define __DOCLIENTPROJECTILEIMPACT__H__

#include "dMessageIdentifiers.h"
#include "dCommonVars.h"

/*  Tell a client local projectile to impact */
class DoClientProjectileImpact {
	static const GAME_MSG MsgID = GAME_MSG_DO_CLIENT_PROJECTILE_IMPACT;

public:
	DoClientProjectileImpact() {
		i64OrgID = LWOOBJID_EMPTY;
		i64OwnerID = LWOOBJID_EMPTY;
		i64TargetID = LWOOBJID_EMPTY;
	}

	DoClientProjectileImpact(std::string _sBitStream, LWOOBJID _i64OrgID = LWOOBJID_EMPTY, LWOOBJID _i64OwnerID = LWOOBJID_EMPTY, LWOOBJID _i64TargetID = LWOOBJID_EMPTY) {
		i64OrgID = _i64OrgID;
		i64OwnerID = _i64OwnerID;
		i64TargetID = _i64TargetID;
		sBitStream = _sBitStream;
	}

	DoClientProjectileImpact(RakNet::BitStream* stream) : DoClientProjectileImpact() {
		Deserialize(stream);
	}

	~DoClientProjectileImpact() {
	}

	void Serialize(RakNet::BitStream* stream) {
		stream->Write(MsgID);

		stream->Write(i64OrgID != LWOOBJID_EMPTY);
		if (i64OrgID != LWOOBJID_EMPTY) stream->Write(i64OrgID);

		stream->Write(i64OwnerID != LWOOBJID_EMPTY);
		if (i64OwnerID != LWOOBJID_EMPTY) stream->Write(i64OwnerID);

		stream->Write(i64TargetID != LWOOBJID_EMPTY);
		if (i64TargetID != LWOOBJID_EMPTY) stream->Write(i64TargetID);

		uint32_t sBitStreamLength = sBitStream.length();
		stream->Write(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			stream->Write(sBitStream[k]);
		}

	}

	bool Deserialize(RakNet::BitStream* stream) {
		bool i64OrgIDIsDefault{};
		stream->Read(i64OrgIDIsDefault);
		if (i64OrgIDIsDefault != 0) stream->Read(i64OrgID);

		bool i64OwnerIDIsDefault{};
		stream->Read(i64OwnerIDIsDefault);
		if (i64OwnerIDIsDefault != 0) stream->Read(i64OwnerID);

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

	LWOOBJID i64OrgID;
	LWOOBJID i64OwnerID;
	LWOOBJID i64TargetID;
	std::string sBitStream;
};

#endif  //!__DOCLIENTPROJECTILEIMPACT__H__
