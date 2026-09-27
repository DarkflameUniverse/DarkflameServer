#include "EffectsMessages.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/EffectsMessagesLegacy.h"

#include <functional>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	const std::vector<int32_t> g_Ints = { 0, 1, -1, 1727, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };
	const std::vector<float> g_Floats = { 0.0f, 1.0f, -1.0f, 0.4f, 0.8f, 3.5f };
	const std::vector<std::u16string> g_WStrings = { u"", u"a", u"camshake-bridge", u"été ☃", std::u16string(300, u'x') };
	const std::vector<std::string> g_Strings = { "", "a", "{9a24f1fa-3177-4745-a2df-fbd996d6e1e3}", std::string(300, 'y') };
	const std::vector<LWOOBJID> g_Ids = { LWOOBJID_EMPTY, 0x1000000000000001LL, -1 };

	PacketBytes Payload(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		return FromBitStream(bitStream);
	}

	// Serializes msg, then reads it with the legacy read sequence; both must consume exactly the same bits.
	template<typename Result>
	Result ReadWithLegacy(const GameMessages::NetGameMsg& msg, const std::function<Result(RakNet::BitStream&)>& read) {
		RakNet::BitStream wire;
		msg.Serialize(wire);
		RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
		auto result = read(legacyStream);
		EXPECT_EQ(legacyStream.GetReadOffset(), wire.GetNumberOfBitsUsed());
		return result;
	}

	// Round trip for messages carrying AMF arrays: the associative part is an unordered map, so a re-serialized
	// copy may list the keys in another order. Checks every bit is consumed and returns the copy.
	template<typename T>
	T AmfRoundTrip(const T& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		T copy;
		EXPECT_TRUE(copy.Deserialize(bitStream));
		EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
		RakNet::BitStream again;
		copy.Serialize(again);
		EXPECT_EQ(again.GetNumberOfBitsUsed(), bitStream.GetNumberOfBitsUsed());
		return copy;
	}

	// A few differently shaped AMF argument arrays (built fresh each time: AMFArrayValue is move only).
	AMFArrayValue MakeArgs(const int variant) {
		AMFArrayValue args;
		if (variant == 1) {
			args.Insert("visible", true);
			args.Insert("text", "hello");
		} else if (variant == 2) {
			args.Insert("state", "Lobby");
			args.Insert<int32_t>("amount", -5);
			args.Insert("ratio", 0.25);
			auto* nested = args.InsertArray("nested");
			nested->Push("one");
			nested->Push(false);
		}
		return args;
	}
}

class EffectsMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(EffectsMessagesTests, PlayAnimationMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto& animation : g_WStrings) {
			for (const auto priority : g_Floats) {
				for (const auto scale : g_Floats) {
					GameMessages::PlayAnimation msg(target, animation);
					msg.fPriority = priority;
					msg.fScale = scale;
					ExpectSameAsLegacy([&](const SystemAddress&) { LegacyGameMessages::SendPlayAnimation(&entity, animation, priority, scale); }, msg, SendMode::Broadcast);
					if (animation == GeneralUtils::ASCIIToUTF16(GeneralUtils::UTF16ToWTF8(animation))) {
						const auto copy = RoundTrip(msg);
						EXPECT_EQ(copy.animationID, animation);
						EXPECT_EQ(copy.fPriority, priority);
						EXPECT_EQ(copy.fScale, scale);
					}
				}
			}
		}
	}
}

TEST_F(EffectsMessagesTests, PlayNDAudioEmitterMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto& guid : g_Strings) {
			const GameMessages::PlayNDAudioEmitter msg(target, guid);
			// The legacy function always broadcast, whatever address it was given.
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendPlayNDAudioEmitter(&entity, a, guid); }, msg, SendMode::Broadcast);
			EXPECT_EQ(RoundTrip(msg).NDAudioEventGUID, guid);
		}
	}
	GameMessages::PlayNDAudioEmitter full;
	full.NDAudioCallbackMessageData = 5;
	full.NDAudioEmitterID = 7;
	full.NDAudioEventGUID = "guid";
	full.NDAudioMetaEventName = "meta";
	full.result = true;
	full.targetObjectIDForNDAudioCallbackMessages = 9;
	const auto copy = RoundTrip(full);
	EXPECT_EQ(copy.NDAudioCallbackMessageData, 5);
	EXPECT_EQ(copy.NDAudioEmitterID, 7);
	EXPECT_EQ(copy.NDAudioMetaEventName, "meta");
	EXPECT_EQ(copy.targetObjectIDForNDAudioCallbackMessages, 9);
	ExpectTruncatedFails(full);
}

TEST_F(EffectsMessagesTests, PlayEmbeddedEffectMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto& effectName : g_WStrings) {
			for (const auto from : g_Ids) {
				for (const auto radius : g_Floats) {
					GameMessages::PlayEmbeddedEffectOnAllClientsNearObject msg;
					msg.target = target;
					msg.effectName = effectName;
					msg.fromObjectID = from;
					msg.radius = radius;
					ExpectSameAsLegacy([&](const SystemAddress&) { LegacyGameMessages::SendPlayEmbeddedEffectOnAllClientsNearObject(&entity, effectName, from, radius); }, msg, SendMode::Broadcast);
					const auto copy = RoundTrip(msg);
					EXPECT_EQ(copy.effectName, effectName);
					EXPECT_EQ(copy.fromObjectID, from);
					EXPECT_EQ(copy.radius, radius);
				}
			}
		}
	}
}

TEST_F(EffectsMessagesTests, PlayFXEffectMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto effectID : g_Ints) {
			for (const auto& effectType : g_WStrings) {
				for (const auto& name : g_Strings) {
					for (const auto secondary : g_Ids) {
						for (const auto priority : { 1.0f, 0.4f, 1.07f }) {
							for (const auto scale : { 1.0f, 2.0f }) {
								for (const bool serialize : { false, true }) {
									GameMessages::PlayFXEffect msg(target, effectID, effectType, name);
									msg.secondary = secondary;
									msg.priority = priority;
									msg.scale = scale;
									msg.serialize = serialize;
									ExpectSameAsLegacy([&](const SystemAddress&) { LegacyGameMessages::SendPlayFXEffect(target, effectID, effectType, name, secondary, priority, scale, serialize); }, msg, SendMode::Broadcast);
									// The Entity* overload wrote the same thing.
									ExpectSameAsLegacy([&](const SystemAddress&) { LegacyGameMessages::SendPlayFXEffect(&entity, effectID, effectType, name, secondary, priority, scale, serialize); }, msg, SendMode::Broadcast);
									const auto copy = RoundTrip(msg);
									EXPECT_EQ(copy.effectID, effectID);
									EXPECT_EQ(copy.effectType, effectType);
									EXPECT_EQ(copy.name, name);
									EXPECT_EQ(copy.secondary, secondary);
									EXPECT_EQ(copy.priority, priority);
									EXPECT_EQ(copy.scale, scale);
									EXPECT_EQ(copy.serialize, serialize);
								}
							}
						}
					}
				}
			}
		}
	}
}

TEST_F(EffectsMessagesTests, StopFXEffectMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const bool killImmediate : { false, true }) {
			for (const auto& name : g_Strings) {
				const GameMessages::StopFXEffect msg(target, killImmediate, name);
				ExpectSameAsLegacy([&](const SystemAddress&) { LegacyGameMessages::SendStopFXEffect(&entity, killImmediate, name); }, msg, SendMode::Broadcast);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.killImmediate, killImmediate);
				EXPECT_EQ(copy.name, name);
			}
		}
	}
}

TEST_F(EffectsMessagesTests, BroadcastTextToChatboxMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto& attrs : g_WStrings) {
			for (const auto& text : g_WStrings) {
				GameMessages::BroadcastTextToChatbox msg;
				msg.target = target;
				msg.attrs = attrs;
				msg.wsText = text;
				// The legacy function always broadcast.
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendBroadcastTextToChatbox(&entity, a, attrs, text); }, msg, SendMode::Broadcast);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.attrs, attrs);
				EXPECT_EQ(copy.wsText, text);
			}
		}
	}
}

TEST_F(EffectsMessagesTests, AmbientSoundsMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto& guid : g_Strings) {
			for (const bool result : { false, true }) {
				GameMessages::Play2DAmbientSound play;
				play.target = target;
				play.audioGUID = guid;
				play.result = result;
				// Sent to the entity's own address (the test entity has none, so both send nothing to a client).
				const auto legacyPlay = Capture([&] { LegacyGameMessages::SendPlay2DAmbientSound(&entity, guid, result); });
				const auto ourPlay = Capture([&] { play.SendToClient(entity.GetSystemAddress()); });
				ASSERT_EQ(legacyPlay.size(), 1);
				ASSERT_EQ(ourPlay.size(), 1);
				EXPECT_PACKET_EQ(FromCapture(legacyPlay[0]), FromCapture(ourPlay[0]));
				EXPECT_EQ(legacyPlay[0].sysAddr, ourPlay[0].sysAddr);
				EXPECT_EQ(legacyPlay[0].broadcast, ourPlay[0].broadcast);
				const auto playCopy = RoundTrip(play);
				EXPECT_EQ(playCopy.audioGUID, guid);
				EXPECT_EQ(playCopy.result, result);

				for (const bool force : { false, true }) {
					GameMessages::Stop2DAmbientSound stop;
					stop.target = target;
					stop.force = force;
					stop.audioGUID = guid;
					stop.result = result;
					const auto legacyStop = Capture([&] { LegacyGameMessages::SendStop2DAmbientSound(&entity, force, guid, result); });
					const auto ourStop = Capture([&] { stop.SendToClient(entity.GetSystemAddress()); });
					ASSERT_EQ(legacyStop.size(), 1);
					ASSERT_EQ(ourStop.size(), 1);
					EXPECT_PACKET_EQ(FromCapture(legacyStop[0]), FromCapture(ourStop[0]));
					EXPECT_EQ(legacyStop[0].sysAddr, ourStop[0].sysAddr);
					const auto stopCopy = RoundTrip(stop);
					EXPECT_EQ(stopCopy.force, force);
					EXPECT_EQ(stopCopy.audioGUID, guid);
				}
			}
		}
	}
}

TEST_F(EffectsMessagesTests, UIMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const int variant : { 0, 1, 2 }) {
			for (const auto& name : g_Strings) {
				GameMessages::UIMessageServerToSingleClient single;
				single.target = target;
				single.args = MakeArgs(variant);
				single.strMessageName = name;
				auto legacyArgs = MakeArgs(variant);
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendUIMessageServerToSingleClient(&entity, a, name, legacyArgs); }, single, SendMode::SendToClient);
				const auto singleCopy = AmfRoundTrip(single);
				EXPECT_EQ(singleCopy.strMessageName, name);
				EXPECT_EQ(singleCopy.args.GetAssociative().size(), single.args.GetAssociative().size());

				// The "single client by address" overload sent UIMessageServerToAllClients with an empty target.
				GameMessages::UIMessageServerToAllClients all;
				all.args = MakeArgs(variant);
				all.strMessageName = name;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendUIMessageServerToSingleClient(name, legacyArgs, a); }, all, SendMode::SendToClient);
				// And the broadcast one.
				ExpectSameAsLegacy([&](const SystemAddress&) { LegacyGameMessages::SendUIMessageServerToAllClients(name, legacyArgs); }, all, SendMode::Broadcast);
				const auto allCopy = AmfRoundTrip(all);
				EXPECT_EQ(allCopy.strMessageName, name);
				EXPECT_EQ(allCopy.args.GetAssociative().size(), all.args.GetAssociative().size());
				EXPECT_EQ(allCopy.args.GetDense().size(), all.args.GetDense().size());
			}
		}
	}
}

TEST_F(EffectsMessagesTests, StartCelebrationEffectMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto celebrationID : g_Ints) {
			GameMessages::StartCelebrationEffect msg;
			msg.target = target;
			msg.celebrationID = celebrationID;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendStartCelebrationEffect(&entity, a, celebrationID); }, msg, SendMode::SendToClient);
			EXPECT_EQ(RoundTrip(msg).celebrationID, celebrationID);
		}
	}
	GameMessages::StartCelebrationEffect full;
	full.animation = u"anim";
	full.backgroundObject = 1;
	full.cameraPathLOT = 2;
	full.celeLeadIn = 3.0f;
	full.celeLeadOut = 4.0f;
	full.celebrationID = 22;
	full.duration = 5.0f;
	full.iconID = 6;
	full.mainText = u"main";
	full.mixerProgram = "mixer";
	full.musicCue = "cue";
	full.pathNodeName = "node";
	full.soundGUID = "sound";
	full.subText = u"sub";
	const auto copy = RoundTrip(full);
	EXPECT_EQ(copy.backgroundObject, 1);
	EXPECT_EQ(copy.cameraPathLOT, 2);
	EXPECT_EQ(copy.celeLeadOut, 4.0f);
	EXPECT_EQ(copy.subText, u"sub");
	ExpectTruncatedFails(full);
}

TEST_F(EffectsMessagesTests, MessageBoxAndChatBubbleMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const bool show : { false, true }) {
			for (const auto callback : g_Ids) {
				for (const auto& identifier : g_WStrings) {
					for (const auto imageID : g_Ints) {
						GameMessages::DisplayMessageBox msg;
						msg.target = target;
						msg.bShow = show;
						msg.callbackClient = callback;
						msg.identifier = identifier;
						msg.imageID = imageID;
						msg.text = identifier + u"!";
						msg.userData = u"data";
						ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendDisplayMessageBox(target, show, callback, identifier, imageID, identifier + u"!", u"data", a); }, msg);
						const auto copy = RoundTrip(msg);
						EXPECT_EQ(copy.bShow, show);
						EXPECT_EQ(copy.callbackClient, callback);
						EXPECT_EQ(copy.identifier, identifier);
						EXPECT_EQ(copy.imageID, imageID);
						EXPECT_EQ(copy.text, identifier + u"!");
						EXPECT_EQ(copy.userData, u"data");
					}
				}
			}
		}
		for (const auto& text : g_WStrings) {
			GameMessages::DisplayChatBubble bubble;
			bubble.target = target;
			bubble.wsText = text;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendDisplayChatBubble(target, text, a); }, bubble);
			EXPECT_EQ(RoundTrip(bubble).wsText, text);
		}
	}
}

TEST_F(EffectsMessagesTests, ChangeIdleFlagsMatchesLegacy) {
	const auto flags = { eAnimationFlags::IDLE_NONE, eAnimationFlags::IDLE_COMBAT, static_cast<eAnimationFlags>(0xFFFFFFFF) };
	for (const auto target : g_Targets) {
		for (const auto on : flags) {
			for (const auto off : flags) {
				GameMessages::ChangeIdleFlags msg;
				msg.target = target;
				msg.flagsOn = on;
				msg.flagsOff = off;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendChangeIdleFlags(target, on, off, a); }, msg, SendMode::Broadcast);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.flagsOn, on);
				EXPECT_EQ(copy.flagsOff, off);
			}
		}
	}
}

TEST_F(EffectsMessagesTests, BillboardsMatchLegacy) {
	for (const auto target : g_Targets) {
		GameMessages::SetNameBillboardState state;
		state.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendSetNamebillboardState(a, target); }, state);
		GameMessages::ShowBillboardInteractIcon icon;
		icon.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendShowBillboardInteractIcon(a, target); }, icon);
	}
}

TEST_F(EffectsMessagesTests, CinematicsMatchLegacy) {
	const auto endBehaviors = { eEndBehavior::RETURN, eEndBehavior::WAIT };
	for (const auto target : g_Targets) {
		for (const auto& path : g_WStrings) {
			for (const auto leadIn : { -1.0f, 0.0f, 2.0f }) {
				for (const auto endBehavior : endBehaviors) {
					for (uint32_t bools = 0; bools < (1u << 10); bools += 37) {
						const auto bit = [&](int i) { return ((bools >> i) & 1) != 0; };
						GameMessages::PlayCinematic msg;
						msg.target = target;
						msg.pathName = path;
						msg.allowGhostUpdates = bit(0);
						msg.bCloseMultiInteract = bit(1);
						msg.bSendServerNotify = bit(2);
						msg.bUseControlledObjectForAudioListener = bit(3);
						msg.endBehavior = endBehavior;
						msg.hidePlayerDuringCine = bit(4);
						msg.leadIn = leadIn;
						msg.leavePlayerLockedWhenFinished = bit(5);
						msg.lockPlayer = bit(6);
						msg.result = bit(7);
						msg.skipIfSamePath = bit(8);
						msg.startTimeAdvance = bit(9) ? 1.5f : 0.0f;
						ExpectSameAsLegacy([&](const SystemAddress& a) {
							LegacyGameMessages::SendPlayCinematic(target, path, a, bit(0), bit(1), bit(2), bit(3), endBehavior, bit(4), leadIn, bit(5), bit(6), bit(7), bit(8), bit(9) ? 1.5f : 0.0f);
							}, msg);
						const auto copy = RoundTrip(msg);
						EXPECT_EQ(copy.pathName, path);
						EXPECT_EQ(copy.endBehavior, endBehavior);
						EXPECT_EQ(copy.leadIn, leadIn);
						EXPECT_EQ(copy.lockPlayer, bit(6));
					}
				}
			}
			for (const auto leadOut : { -1.0f, 1.0f }) {
				for (const bool locked : { false, true }) {
					GameMessages::EndCinematic end;
					end.target = target;
					end.pathName = path;
					end.leadOut = leadOut;
					end.leavePlayerLocked = locked;
					ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendEndCinematic(target, path, a, leadOut, locked); }, end);
					const auto copy = RoundTrip(end);
					EXPECT_EQ(copy.leadOut, leadOut);
					EXPECT_EQ(copy.leavePlayerLocked, locked);
					EXPECT_EQ(copy.pathName, path);
				}
			}
		}
	}
}

TEST_F(EffectsMessagesTests, SlashCommandTextFeedbackMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto& text : g_WStrings) {
			const GameMessages::SlashCommandTextFeedback msg(target, text);
			const auto legacy = Capture([&] { LegacyGameMessages::SendSlashCommandFeedbackText(&entity, text); });
			const auto ours = Capture([&] { msg.SendToClient(entity.GetSystemAddress()); });
			ASSERT_EQ(legacy.size(), 1);
			ASSERT_EQ(ours.size(), 1);
			EXPECT_PACKET_EQ(FromCapture(legacy[0]), FromCapture(ours[0]));
			EXPECT_EQ(legacy[0].sysAddr, ours[0].sysAddr);
			EXPECT_EQ(RoundTrip(msg).text, text);
		}
	}
}

TEST_F(EffectsMessagesTests, EmotesMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto emoteID : g_Ints) {
			for (const auto emoteTarget : g_Ids) {
				GameMessages::PlayEmote msg;
				msg.target = target;
				msg.emoteID = emoteID;
				msg.targetID = emoteTarget;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendPlayEmote(target, emoteID, emoteTarget, a); }, msg);
				const auto legacyRead = ReadWithLegacy<LegacyGameMessages::LegacyPlayEmote>(msg, LegacyGameMessages::ReadPlayEmote);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.emoteID, legacyRead.emoteID);
				EXPECT_EQ(copy.targetID, legacyRead.targetID);
				ExpectTruncatedFails(msg);
			}
			for (const bool lock : { false, true }) {
				GameMessages::SetEmoteLockState lockState;
				lockState.target = target;
				lockState.bLock = lock;
				lockState.emoteID = emoteID;
				const auto legacy = Capture([&] { LegacyGameMessages::SendSetEmoteLockState(&entity, lock, emoteID); });
				const auto ours = Capture([&] { lockState.SendToClient(entity.GetSystemAddress()); });
				ASSERT_EQ(legacy.size(), 1);
				ASSERT_EQ(ours.size(), 1);
				EXPECT_PACKET_EQ(FromCapture(legacy[0]), FromCapture(ours[0]));
				const auto copy = RoundTrip(lockState);
				EXPECT_EQ(copy.bLock, lock);
				EXPECT_EQ(copy.emoteID, emoteID);
			}
		}
	}
}

TEST_F(EffectsMessagesTests, InboundReadsLikeLegacy) {
	for (const auto button : g_Ints) {
		for (const auto& identifier : g_WStrings) {
			GameMessages::MessageBoxRespond box;
			box.iButton = button;
			box.identifier = identifier;
			box.userData = u"user";
			const auto legacyBox = ReadWithLegacy<LegacyGameMessages::LegacyMessageBoxResponse>(box, LegacyGameMessages::ReadMessageBoxResponse);
			const auto boxCopy = RoundTrip(box);
			EXPECT_TRUE(legacyBox.ok);
			EXPECT_EQ(boxCopy.iButton, legacyBox.iButton);
			EXPECT_EQ(boxCopy.identifier, legacyBox.identifier);
			EXPECT_EQ(boxCopy.userData, legacyBox.userData);
			ExpectTruncatedFails(box);

			GameMessages::ChoiceBoxRespond choice;
			choice.buttonIdentifier = u"btn";
			choice.iButton = button;
			choice.identifier = identifier;
			const auto legacyChoice = ReadWithLegacy<LegacyGameMessages::LegacyChoiceBoxRespond>(choice, LegacyGameMessages::ReadChoiceBoxRespond);
			const auto choiceCopy = RoundTrip(choice);
			EXPECT_TRUE(legacyChoice.ok);
			EXPECT_EQ(choiceCopy.buttonIdentifier, legacyChoice.buttonIdentifier);
			EXPECT_EQ(choiceCopy.iButton, legacyChoice.iButton);
			EXPECT_EQ(choiceCopy.identifier, legacyChoice.identifier);
			ExpectTruncatedFails(choice);
		}
	}

	for (const auto event : { eCinematicEvent::STARTED, eCinematicEvent::WAYPOINT, eCinematicEvent::ENDED }) {
		for (const auto time : { -1.0f, 0.0f, 2.5f }) {
			for (const auto waypoint : { -1, 0, 3 }) {
				for (const auto& path : g_WStrings) {
					GameMessages::CinematicUpdate update;
					update.event = event;
					update.overallTime = time;
					update.pathName = path;
					update.pathTime = time * 2;
					update.waypoint = waypoint;
					const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyCinematicUpdate>(update, LegacyGameMessages::ReadCinematicUpdate);
					const auto copy = RoundTrip(update);
					EXPECT_EQ(copy.event, legacy.event);
					EXPECT_EQ(copy.overallTime, legacy.overallTime);
					EXPECT_EQ(copy.pathName, legacy.pathName);
					EXPECT_EQ(copy.pathTime, legacy.pathTime);
					EXPECT_EQ(copy.waypoint, legacy.waypoint);
					ExpectTruncatedFails(update);
				}
			}
		}
	}

	// A length above MAX_MESSAGE_LENGTH is rejected, as the old handler did.
	RakNet::BitStream tooLong;
	tooLong.Write<int32_t>(0);
	tooLong.Write<uint32_t>(MAX_MESSAGE_LENGTH + 1);
	GameMessages::MessageBoxRespond box;
	EXPECT_FALSE(box.Deserialize(tooLong));
}

// Independent of the legacy code: hand computed payloads (RakNet writes MSB first).
TEST_F(EffectsMessagesTests, GoldenBytes) {
	// effectID 7 (flag 1), "a" (u32 1, 0x61 0x00), scale default (0), "b" (u32 1, 0x62), priority default (0),
	// secondary default (0), serialize true (1).
	GameMessages::PlayFXEffect fx(0, 7, u"a", "b");
	EXPECT_PACKET_EQ(FromHex("83 80 00 00 00 80 00 00 30 80 00 40 00 00 18 88", 125), Payload(fx));

	GameMessages::StopFXEffect stop(0, true, "ab");
	EXPECT_PACKET_EQ(FromHex("81 00 00 00 30 b1 00", 49), Payload(stop));

	GameMessages::PlayAnimation animation(0, u"ab");
	// u32 2, "a\0b\0", then expect(1) immediate(0) complete(0) priority(0) scale(0)
	EXPECT_PACKET_EQ(FromHex("02 00 00 00 61 00 62 00 80", 69), Payload(animation));

	GameMessages::ChangeIdleFlags idle;
	idle.flagsOn = static_cast<eAnimationFlags>(1);
	// off default (0), on flag (1) then 01 00 00 00
	EXPECT_PACKET_EQ(FromHex("40 40 00 00 00", 34), Payload(idle));

	GameMessages::EndCinematic end;
	EXPECT_PACKET_EQ(FromHex("00 00 00 00 00", 34), Payload(end));
}
