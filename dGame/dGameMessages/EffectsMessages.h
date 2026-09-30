#ifndef EFFECTSMESSAGES_H
#define EFFECTSMESSAGES_H

#include "GameMessages.h"
#include "Amf3.h"
#include "eAnimationFlags.h"
#include "eCinematicEvent.h"
#include "eEndBehavior.h"

#include <string>

// Game messages for effects, audio, animations, emotes, cinematics and UI text (message boxes, chat bubbles,
// billboards, UI messages, slash command feedback).
// Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
namespace GameMessages {
	// Server -> client, broadcast.
	struct PlayAnimation : public NetGameMsg {
		PlayAnimation() : NetGameMsg(MessageType::Game::PLAY_ANIMATION) {}
		PlayAnimation(const LWOOBJID _target, const std::u16string& _animationID) : PlayAnimation() {
			target = _target;
			animationID = _animationID;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// Written as a u32 length followed by that many UTF-16 characters, where the length is the size of the
		// name converted to UTF-8 (so a non-ASCII name is padded with null characters). This is what DLU has
		// always sent.
		std::u16string animationID{};
		bool bExpectAnimToExist{ true };
		bool bPlayImmediate{};
		bool bTriggerOnCompleteMsg{};
		float fPriority{ 0.0f }; // optional
		float fScale{ 1.0f }; // optional
	};

	// Server -> client, broadcast.
	struct PlayNDAudioEmitter : public NetGameMsg {
		PlayNDAudioEmitter() : NetGameMsg(MessageType::Game::PLAY_ND_AUDIO_EMITTER) {}
		PlayNDAudioEmitter(const LWOOBJID _target, const std::string& _NDAudioEventGUID) : PlayNDAudioEmitter() {
			target = _target;
			NDAudioEventGUID = _NDAudioEventGUID;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID NDAudioCallbackMessageData{}; // optional
		uint32_t NDAudioEmitterID{}; // optional
		std::string NDAudioEventGUID{};
		std::string NDAudioMetaEventName{};
		bool result{};
		LWOOBJID targetObjectIDForNDAudioCallbackMessages{ LWOOBJID_EMPTY }; // optional
	};

	// Server -> client, broadcast.
	struct PlayEmbeddedEffectOnAllClientsNearObject : public NetGameMsg {
		PlayEmbeddedEffectOnAllClientsNearObject() : NetGameMsg(MessageType::Game::PLAY_EMBEDDED_EFFECT_ON_ALL_CLIENTS_NEAR_OBJECT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string effectName{};
		LWOOBJID fromObjectID{};
		float radius{};
	};

	// Server -> client, broadcast.
	struct PlayFXEffect : public NetGameMsg {
		PlayFXEffect() : NetGameMsg(MessageType::Game::PLAY_FX_EFFECT) {}
		PlayFXEffect(const LWOOBJID _target, const int32_t _effectID, const std::u16string& _effectType, const std::string& _name) : PlayFXEffect() {
			target = _target;
			effectID = _effectID;
			effectType = _effectType;
			name = _name;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t effectID{ -1 }; // optional
		std::u16string effectType{};
		float scale{ 1.0f }; // optional
		std::string name{};
		float priority{ 1.0f }; // optional
		LWOOBJID secondary{ LWOOBJID_EMPTY }; // optional
		bool serialize{ true };
	};

	// Server -> client, broadcast.
	struct StopFXEffect : public NetGameMsg {
		StopFXEffect() : NetGameMsg(MessageType::Game::STOP_FX_EFFECT) {}
		StopFXEffect(const LWOOBJID _target, const bool _killImmediate, const std::string& _name) : StopFXEffect() {
			target = _target;
			killImmediate = _killImmediate;
			name = _name;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool killImmediate{};
		std::string name{};
	};

	// Server -> client, broadcast.
	struct BroadcastTextToChatbox : public NetGameMsg {
		BroadcastTextToChatbox() : NetGameMsg(MessageType::Game::BROADCAST_TEXT_TO_CHATBOX) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// Name value text: u32 length, the characters, then a null character that DLU always writes (even when empty).
		std::u16string attrs{};
		std::u16string wsText{};
	};

	// Server -> client.
	struct Play2DAmbientSound : public NetGameMsg {
		Play2DAmbientSound() : NetGameMsg(MessageType::Game::PLAY2_D_AMBIENT_SOUND) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::string audioGUID{};
		bool result{};
	};

	// Server -> client.
	struct Stop2DAmbientSound : public NetGameMsg {
		Stop2DAmbientSound() : NetGameMsg(MessageType::Game::STOP2_D_AMBIENT_SOUND) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool force{};
		std::string audioGUID{};
		bool result{};
	};

	// Server -> client.
	struct UIMessageServerToSingleClient : public NetGameMsg {
		UIMessageServerToSingleClient() : NetGameMsg(MessageType::Game::UI_MESSAGE_SERVER_TO_SINGLE_CLIENT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		AMFArrayValue args{};
		std::string strMessageName{};
	};

	// Server -> client. Broadcast, or sent to one client (DLU uses it to reach a client that may not have an
	// entity yet; the target is then LWOOBJID_EMPTY).
	struct UIMessageServerToAllClients : public NetGameMsg {
		UIMessageServerToAllClients() : NetGameMsg(MessageType::Game::UI_MESSAGE_SERVER_TO_ALL_CLIENTS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		AMFArrayValue args{};
		std::string strMessageName{};
	};

	// Server -> client.
	struct StartCelebrationEffect : public NetGameMsg {
		StartCelebrationEffect() : NetGameMsg(MessageType::Game::START_CELEBRATION_EFFECT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string animation{};
		LOT backgroundObject{ 11164 }; // optional
		LOT cameraPathLOT{ 12458 }; // optional
		float celeLeadIn{ 1.0f }; // optional
		float celeLeadOut{ 0.8f }; // optional
		int32_t celebrationID{ -1 }; // optional; DLU always writes it, even when it has the default value
		float duration{};
		uint32_t iconID{};
		std::u16string mainText{};
		std::string mixerProgram{};
		std::string musicCue{};
		std::string pathNodeName{};
		std::string soundGUID{};
		std::u16string subText{};
	};

	// Server -> client.
	struct DisplayMessageBox : public NetGameMsg {
		DisplayMessageBox() : NetGameMsg(MessageType::Game::DISPLAY_MESSAGE_BOX) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bShow{};
		LWOOBJID callbackClient{};
		std::u16string identifier{};
		int32_t imageID{};
		std::u16string text{};
		std::u16string userData{};
	};

	// Client -> server. Sent to the callbackClient of a DisplayMessageBox when the player presses a button.
	struct MessageBoxRespond : public NetGameMsg {
		MessageBoxRespond() : NetGameMsg(MessageType::Game::MESSAGE_BOX_RESPOND) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t iButton{};
		std::u16string identifier{};
		std::u16string userData{};
	};

	// Client -> server.
	struct ChoiceBoxRespond : public NetGameMsg {
		ChoiceBoxRespond() : NetGameMsg(MessageType::Game::CHOICE_BOX_RESPOND) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::u16string buttonIdentifier{};
		int32_t iButton{};
		std::u16string identifier{};
	};

	// Server -> client.
	struct DisplayChatBubble : public NetGameMsg {
		DisplayChatBubble() : NetGameMsg(MessageType::Game::DISPLAY_CHAT_BUBBLE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string wsText{};
	};

	// Server -> client, broadcast.
	struct ChangeIdleFlags : public NetGameMsg {
		ChangeIdleFlags() : NetGameMsg(MessageType::Game::CHANGE_IDLE_FLAGS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eAnimationFlags flagsOff{ eAnimationFlags::IDLE_NONE }; // optional
		eAnimationFlags flagsOn{ eAnimationFlags::IDLE_NONE }; // optional
	};

	// Server -> client. The client's message has two bools (overrideDefault, state), but the client never reads
	// them, so DLU sends no payload; the message only turns the name billboard off.
	struct SetNameBillboardState : public NetGameMsg {
		SetNameBillboardState() : NetGameMsg(MessageType::Game::SET_NAME_BILLBOARD_STATE) {}
	};

	// Server -> client. No payload.
	struct ShowBillboardInteractIcon : public NetGameMsg {
		ShowBillboardInteractIcon() : NetGameMsg(MessageType::Game::SHOW_BILLBOARD_INTERACT_ICON) {}
	};

	// Server -> client.
	struct PlayCinematic : public NetGameMsg {
		PlayCinematic() : NetGameMsg(MessageType::Game::PLAY_CINEMATIC) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool allowGhostUpdates{ true };
		bool bCloseMultiInteract{ true };
		bool bSendServerNotify{};
		bool bUseControlledObjectForAudioListener{};
		eEndBehavior endBehavior{ eEndBehavior::RETURN }; // optional
		bool hidePlayerDuringCine{};
		float leadIn{ -1.0f }; // optional
		bool leavePlayerLockedWhenFinished{};
		bool lockPlayer{ true };
		std::u16string pathName{};
		bool result{};
		bool skipIfSamePath{};
		float startTimeAdvance{};
	};

	// Server -> client.
	struct EndCinematic : public NetGameMsg {
		EndCinematic() : NetGameMsg(MessageType::Game::END_CINEMATIC) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		float leadOut{ -1.0f }; // optional
		bool leavePlayerLocked{};
		std::u16string pathName{};
	};

	// Client -> server.
	struct CinematicUpdate : public NetGameMsg {
		CinematicUpdate() : NetGameMsg(MessageType::Game::CINEMATIC_UPDATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		eCinematicEvent event{ eCinematicEvent::STARTED }; // optional
		float overallTime{ -1.0f }; // optional
		std::u16string pathName{};
		float pathTime{ -1.0f }; // optional
		int32_t waypoint{ -1 }; // optional
	};

	// Server -> client.
	struct SlashCommandTextFeedback : public NetGameMsg {
		SlashCommandTextFeedback() : NetGameMsg(MessageType::Game::SLASH_COMMAND_TEXT_FEEDBACK) {}
		SlashCommandTextFeedback(const LWOOBJID _target, const std::u16string& _text) : SlashCommandTextFeedback() {
			target = _target;
			text = _text;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string text{};
	};

	// Both directions: the client asks to play an emote (handled here), and the server tells clients an object
	// plays one.
	struct PlayEmote : public NetGameMsg {
		PlayEmote() : NetGameMsg(MessageType::Game::PLAY_EMOTE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t emoteID{};
		LWOOBJID targetID{};
	};

	// Server -> client.
	struct SetEmoteLockState : public NetGameMsg {
		SetEmoteLockState() : NetGameMsg(MessageType::Game::SET_EMOTE_LOCK_STATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bLock{};
		int32_t emoteID{};
	};

	struct DisplayTooltip : public NetGameMsg {
		DisplayTooltip() : NetGameMsg(MessageType::Game::DISPLAY_TOOLTIP) {}
		bool doOrDie{};
		bool noRepeat{};
		bool noRevive{};
		bool isPropertyTooltip{};
		bool show{};
		bool translate{};
		int32_t time{};
		std::u16string id{};
		LwoNameValue localizeParams{};
		std::u16string imageName{};
		std::u16string text{};
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	struct EmotePlayed : public NetGameMsg {
		EmotePlayed() : NetGameMsg(MessageType::Game::EMOTE_PLAYED), emoteID(0), targetID(0) {}

		void Serialize(RakNet::BitStream& stream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t emoteID;
		LWOOBJID targetID;
	};
};

#endif // EFFECTSMESSAGES_H
