#include "MastTeleport.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "Preconditions.h"

#ifdef _WIN32
#define _USE_MATH_DEFINES
#include <math.h>
#endif

void MastTeleport::OnStartup(Entity* self) {
	self->SetNetworkVar<std::string>(u"hookPreconditions", "154;44", UNASSIGNED_SYSTEM_ADDRESS);
}

void MastTeleport::OnRebuildComplete(Entity* self, Entity* target) {
	if (Preconditions::Check(target, 154) && Preconditions::Check(target, 44)) {
		self->SetVar<LWOOBJID>(u"userID", target->GetObjectID());

		GameMessages::SendSetStunned(target->GetObjectID(), PUSH, target->GetSystemAddress(),
			LWOOBJID_EMPTY, true, true, true, true, true, true, true
		);

		self->AddTimer("Start", 3);
	}
}

void MastTeleport::OnTimerDone(Entity* self, std::string timerName) {
	const auto playerId = self->GetVar<LWOOBJID>(u"userID");

	auto* player = EntityManager::Instance()->GetEntity(playerId);

	if (player == nullptr) return;

	if (timerName == "Start") {
		auto position = self->GetPosition();
		auto rotation = self->GetRotation();

		GameMessages::SendTeleport(playerId, position, rotation, player->GetSystemAddress(), true);

		// Hacky fix for odd rotations
		if (self->GetVar<std::u16string>(u"MastName") != u"Jail") {
			GameMessages::SendOrientToAngle(playerId, true, (M_PI / 180) * 140.0f, player->GetSystemAddress());
		} else {
			GameMessages::SendOrientToAngle(playerId, true, (M_PI / 180) * 100.0f, player->GetSystemAddress());
		}

		const auto cinematic = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"Cinematic"));
		const auto leanIn = self->GetVar<float>(u"LeanIn");

		if (!cinematic.empty()) {
			GameMessages::SendPlayCinematic(playerId, GeneralUtils::ASCIIToUTF16(cinematic), player->GetSystemAddress(),
				true, true, false, false, 0, false, leanIn
			);
		}

		GameMessages::SendPlayFXEffect(playerId, 6039, u"hook", "hook", LWOOBJID_EMPTY, 1, 1, true);

		GameMessages::SendPlayAnimation(player, u"crow-swing-no-equip");

		GameMessages::SendPlayAnimation(self, u"swing");

		self->AddTimer("PlayerAnimDone", 6.25f);
	} else if (timerName == "PlayerAnimDone") {
		GameMessages::SendStopFXEffect(player, true, "hook");

		auto forward = self->GetRotation().GetForwardVector();

		const auto degrees = -25.0f;

		const auto rads = degrees * (static_cast<float>(M_PI) / 180.0f);

		const Vector3 newPlayerRot = { 0, rads, 0 };

		auto position = self->GetPosition();

		position.x += (forward.x * 20.5f);
		position.y += 12;
		position.z += (forward.z * 20.5f);

		GameMessages::SendOrientToAngle(playerId, true, rads, player->GetSystemAddress());

		GameMessages::SendTeleport(playerId, position, NiQuaternion::IDENTITY, player->GetSystemAddress());

		GameMessages::SendSetStunned(playerId, POP, player->GetSystemAddress(),
			LWOOBJID_EMPTY, true, true, true, true, true, true, true
		);
	}
}
