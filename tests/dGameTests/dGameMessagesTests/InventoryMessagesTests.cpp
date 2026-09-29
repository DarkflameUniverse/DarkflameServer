#include "InventoryMessages.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/InventoryMessagesLegacy.h"

#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "eReplicaComponentType.h"
#include "Inventory.h"
#include "Item.h"

#include <functional>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	const std::vector<int32_t> g_Ints = { 0, 1, -1, 1727, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };
	const std::vector<uint32_t> g_UInts = { 0, 1, 2, 999, std::numeric_limits<uint32_t>::max() };
	const std::vector<LWOOBJID> g_Ids = { LWOOBJID_EMPTY, 0x1000000000000001LL, -1 };
	const std::vector<eInventoryType> g_InventoryTypes = { eInventoryType::ITEMS, eInventoryType::VAULT_ITEMS, eInventoryType::MODELS, eInventoryType::INVALID };
	const std::vector<std::u16string> g_WStrings = { u"", u"a", u"ma=1:6416+1:8092", u"été ☃", std::u16string(300, u'x') };

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

	// Makes lot a valid item (ItemComponent componentID) with the given bind flags, without a CDClient database.
	void RegisterItemLot(const LOT lot, const uint32_t componentID, const bool isBOE, const bool isBOP) {
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		component.isBOE = isBOE;
		component.isBOP = isBOP;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}
}

class InventoryMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(InventoryMessagesTests, AddItemToInventoryClientSyncMatchesLegacy) {
	RegisterItemLot(1727, 90001, false, false);
	RegisterItemLot(6416, 90002, true, false);
	RegisterItemLot(8092, 90003, false, true);

	LwoNameValue emptyConfig;
	LwoNameValue config;
	config.Insert(u"assemblyPartLOTs", std::u16string(u"1:6416+1:8092"));
	config.Insert<int32_t>(u"userModelID", 42);

	uint32_t slot = 0;
	for (const auto inventoryType : { eInventoryType::ITEMS, eInventoryType::MODELS }) {
		Inventory inventory(inventoryType, 400, {}, nullptr);
		for (const LOT lot : { 1727, 6416, 8092 }) {
			for (const uint32_t count : { 0u, 1u, 7u }) {
				for (const bool bound : { false, true }) {
					for (const auto* itemConfig : { &emptyConfig, &config }) {
						// The inventory owns (and deletes) the item.
						auto* item = new Item(0x5000 + slot, lot, &inventory, slot, count, bound, *itemConfig, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
						slot++;
						for (const auto target : g_Targets) {
							Entity entity(target, info);
							for (const LWOOBJID newID : { LWOOBJID_EMPTY, LWOOBJID{ 0x7777 } }) {
								for (const bool showFlyingLoot : { false, true }) {
									for (const int itemCount : { 1, 0, -1, 5 }) {
										for (const LWOOBJID subKey : { LWOOBJID_EMPTY, LWOOBJID{ 0x99 } }) {
											for (const auto lootSource : { eLootSourceType::NONE, eLootSourceType::QUICKBUILD }) {
												GameMessages::AddItemToInventoryClientSync msg;
												msg.target = target;
												msg.SetItem(*item);
												msg.eLootTypeSource = lootSource;
												msg.iSubkey = subKey;
												msg.itemCount = itemCount;
												msg.newObjID = newID;
												msg.showFlyingLoot = showFlyingLoot;
												ExpectSameAsLegacy([&](const SystemAddress& a) {
													LegacyGameMessages::SendAddItemToInventoryClientSync(&entity, a, item, newID, showFlyingLoot, itemCount, subKey, lootSource);
													}, msg, SendMode::SendToClient);
												const auto copy = RoundTrip(msg);
												EXPECT_EQ(copy.extraInfo, msg.extraInfo);
												EXPECT_EQ(copy.iObjTemplate, lot);
												EXPECT_EQ(copy.invType, inventoryType);
												EXPECT_EQ(copy.itemsTotal, count);
												EXPECT_EQ(copy.itemCount, itemCount);
												EXPECT_EQ(copy.iSubkey, subKey);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

TEST_F(InventoryMessagesTests, SmallMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		const auto toEntity = [&](const std::function<void()>& legacy, const GameMessages::NetGameMsg& msg) {
			// The legacy functions sent to entity->GetSystemAddress() only.
			const auto legacyPackets = Capture(legacy);
			const auto ours = Capture([&] { msg.SendToClient(entity.GetSystemAddress()); });
			ASSERT_EQ(legacyPackets.size(), 1);
			ASSERT_EQ(ours.size(), 1);
			EXPECT_PACKET_EQ(FromCapture(legacyPackets[0]), FromCapture(ours[0]));
			EXPECT_EQ(legacyPackets[0].sysAddr, ours[0].sysAddr);
			EXPECT_EQ(legacyPackets[0].broadcast, ours[0].broadcast);
			};

		for (const auto type : g_InventoryTypes) {
			for (const auto size : g_Ints) {
				GameMessages::SetInventorySize msg;
				msg.target = target;
				msg.inventoryType = type;
				msg.size = size;
				toEntity([&] { LegacyGameMessages::SendSetInventorySize(&entity, type, size); }, msg);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.inventoryType, type);
				EXPECT_EQ(copy.size, size);
			}
		}

		for (const auto item : g_Ids) {
			for (const bool success : { false, true }) {
				GameMessages::ConsumeClientItem consume;
				consume.target = target;
				consume.bSuccess = success;
				consume.item = item;
				toEntity([&] { LegacyGameMessages::SendConsumeClientItem(&entity, success, item); }, consume);
				const auto copy = RoundTrip(consume);
				EXPECT_EQ(copy.bSuccess, success);
				EXPECT_EQ(copy.item, item);
			}
		}

		for (const auto lot : g_Ints) {
			for (const bool success : { false, true }) {
				GameMessages::UseItemResult result;
				result.target = target;
				result.itemTemplateID = lot;
				result.useItemResult = success;
				toEntity([&] { LegacyGameMessages::SendUseItemResult(&entity, lot, success); }, result);
				const auto copy = RoundTrip(result);
				EXPECT_EQ(copy.itemTemplateID, lot);
				EXPECT_EQ(copy.useItemResult, success);
			}
		}

		for (const auto response : { eUseItemResponse::NoImaginationForPet, eUseItemResponse::FailedPrecondition, static_cast<eUseItemResponse>(0) }) {
			GameMessages::UseItemRequirementsResponse msg;
			msg.target = target;
			msg.eUseResponse = response;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendUseItemRequirementsResponse(target, a, response); }, msg, SendMode::SendToClient);
			EXPECT_EQ(RoundTrip(msg).eUseResponse, response);
		}

		for (const auto dst : g_InventoryTypes) {
			for (const auto src : g_InventoryTypes) {
				for (const auto code : { eReponseMoveItemBetweenInventoryTypeCode::SUCCESS, eReponseMoveItemBetweenInventoryTypeCode::FAIL_GENERIC, eReponseMoveItemBetweenInventoryTypeCode::FAIL_INV_FULL, eReponseMoveItemBetweenInventoryTypeCode::FAIL_CANT_MOVE_THINKING_HAT }) {
					GameMessages::ResponseMoveItemBetweenInventoryTypes msg;
					msg.target = target;
					msg.inventoryTypeDestination = dst;
					msg.inventoryTypeSource = src;
					msg.response = code;
					ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendResponseMoveItemBetweenInventoryTypes(target, a, dst, src, code); }, msg, SendMode::SendToClient);
					const auto copy = RoundTrip(msg);
					EXPECT_EQ(copy.inventoryTypeDestination, dst);
					EXPECT_EQ(copy.inventoryTypeSource, src);
					EXPECT_EQ(copy.response, code);
				}
			}
		}

		for (const auto slots : g_UInts) {
			for (const auto type : g_InventoryTypes) {
				GameMessages::NotifyNotEnoughInvSpace msg;
				msg.target = target;
				msg.freeSlotsNeeded = slots;
				msg.inventoryType = type;
				// WIRE FIX: the legacy bytes carry VEHICLE_NOTIFY_FINISHED_RACE (1396) as the message ID; everything
				// else is unchanged, so compare against them with the ID (bytes 16-17, after the header and object ID)
				// swapped for NOTIFY_NOT_ENOUGH_INV_SPACE (1516).
				const auto legacy = Capture([&] { LegacyGameMessages::SendNotifyNotEnoughInvSpace(target, slots, type, ClientAddress()); });
				ASSERT_FALSE(legacy.empty());
				auto expected = FromCapture(legacy[0]);
				ASSERT_EQ(expected.bytes[16] | (expected.bytes[17] << 8), 1396);
				expected.bytes[16] = 1516 & 0xFF;
				expected.bytes[17] = 1516 >> 8;
				EXPECT_PACKET_EQ(expected, StructPacket(msg));
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.freeSlotsNeeded, slots);
				EXPECT_EQ(copy.inventoryType, type);
			}
		}

		for (const bool active : { false, true }) {
			for (const auto type : { eUnequippableActiveType::INVALID, eUnequippableActiveType::PET, eUnequippableActiveType::MOUNT }) {
				for (const auto item : g_Ids) {
					GameMessages::MarkInventoryItemAsActive msg;
					msg.target = target;
					msg.bActive = active;
					msg.iType = type;
					msg.itemID = item;
					ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendMarkInventoryItemAsActive(target, active, type, item, a); }, msg);
					const auto copy = RoundTrip(msg);
					EXPECT_EQ(copy.bActive, active);
					EXPECT_EQ(copy.iType, type);
					EXPECT_EQ(copy.itemID, item);
				}
			}
		}

		GameMessages::UpdateInventoryUi updateUi;
		updateUi.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendUpdateInventoryUi(target, a); }, updateUi, SendMode::SendToClient);
	}
}

TEST_F(InventoryMessagesTests, RemoveItemFromInventoryMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto id : g_Ids) {
			for (const LOT lot : { LOT_NULL, LOT{ 1727 } }) {
				for (const auto type : g_InventoryTypes) {
					for (const auto stackCount : g_UInts) {
						for (const auto stackRemaining : g_UInts) {
							GameMessages::RemoveItemFromInventory msg;
							msg.target = target;
							msg.bConfirmed = true;
							msg.eInvType = type;
							msg.eLootTypeSource = LOOTTYPE_NONE;
							msg.iObjID = id;
							msg.iObjTemplate = lot;
							msg.iStackCount = stackCount;
							msg.iStackRemaining = stackRemaining;
							ExpectSameAsLegacy([&](const SystemAddress& a) {
								LegacyGameMessages::SendRemoveItemFromInventory(&entity, a, id, lot, type, stackCount, stackRemaining);
								}, msg, SendMode::SendToClient);

							// What the server sends reads back the same with the old handler's read sequence.
							const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyRemoveItem>(msg, LegacyGameMessages::ReadRemoveItemFromInventory);
							const auto copy = RoundTrip(msg);
							EXPECT_EQ(copy.eInvType, legacy.eInvType);
							EXPECT_EQ(copy.iObjID, legacy.iObjID);
							EXPECT_EQ(copy.iObjTemplate, legacy.iObjTemplate);
							EXPECT_EQ(copy.iStackCount, legacy.iStackCount);
							EXPECT_EQ(copy.iStackRemaining, legacy.iStackRemaining);
						}
					}
				}
			}
		}
	}
}

TEST_F(InventoryMessagesTests, InboundReadsLikeLegacy) {
	// RemoveItemFromInventory as the client sends it (every optional field both ways is covered by the
	// optional reads; the "always sent" flags only matter for what the server writes).
	for (const bool flag : { false, true }) {
		for (const auto& extraInfo : g_WStrings) {
			for (const auto id : g_Ids) {
				GameMessages::RemoveItemFromInventory msg;
				msg.bConfirmed = flag;
				msg.bDeleteItem = !flag;
				msg.bOutSuccess = flag;
				msg.extraInfo = extraInfo;
				msg.forceDeletion = flag;
				msg.iLootTypeSource = id;
				msg.iObjID = id;
				msg.iRequestingObjID = id;
				msg.iSubkey = id;
				msg.iTradeID = id;
				const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyRemoveItem>(msg, LegacyGameMessages::ReadRemoveItemFromInventory);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.bConfirmed, legacy.bConfirmed);
				EXPECT_EQ(copy.bDeleteItem, legacy.bDeleteItem);
				EXPECT_EQ(copy.bOutSuccess, legacy.bOutSuccess);
				EXPECT_EQ(copy.extraInfo, legacy.extraInfo);
				EXPECT_EQ(copy.forceDeletion, legacy.forceDeletion);
				EXPECT_EQ(copy.iLootTypeSource, legacy.iLootTypeSource);
				EXPECT_EQ(copy.iRequestingObjID, legacy.iRequestingObjID);
				EXPECT_EQ(copy.iSubkey, legacy.iSubkey);
				EXPECT_EQ(copy.iTradeID, legacy.iTradeID);
				ExpectTruncatedFails(msg);
			}
		}
	}
	// The same message with every optional flag clear, as a client that relies on the defaults would send it.
	{
		RakNet::BitStream wire;
		wire.Write1(); // bConfirmed
		wire.Write1(); // bDeleteItem
		wire.Write0(); // bOutSuccess
		wire.Write0(); // eInvType
		wire.Write0(); // eLootTypeSource
		wire.Write<uint32_t>(0); // extraInfo
		wire.Write1(); // forceDeletion
		for (int i = 0; i < 8; i++) wire.Write0(); // the eight optional ids and counts
		RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
		const auto legacy = LegacyGameMessages::ReadRemoveItemFromInventory(legacyStream);
		GameMessages::RemoveItemFromInventory msg;
		ASSERT_TRUE(msg.Deserialize(wire));
		EXPECT_EQ(msg.eInvType, legacy.eInvType);
		EXPECT_EQ(msg.eLootTypeSource, legacy.eLootTypeSource);
		EXPECT_EQ(msg.iObjTemplate, legacy.iObjTemplate);
		EXPECT_EQ(msg.iStackCount, legacy.iStackCount);
		EXPECT_EQ(msg.iStackRemaining, legacy.iStackRemaining);
	}

	for (const auto id : g_Ids) {
		for (const bool flag : { false, true }) {
			GameMessages::EquipInventory equip;
			equip.bIgnoreCooldown = !flag;
			equip.bOutSuccess = flag;
			equip.itemToEquip = id;
			const auto legacyEquip = ReadWithLegacy<LegacyGameMessages::LegacyEquip>(equip, LegacyGameMessages::ReadEquipItem);
			EXPECT_EQ(RoundTrip(equip).itemToEquip, legacyEquip.objectID);
			ExpectTruncatedFails(equip);

			GameMessages::UnEquipInventory unequip;
			unequip.bEvenIfDead = flag;
			unequip.bIgnoreCooldown = !flag;
			unequip.bOutSuccess = flag;
			unequip.itemToUnequip = id;
			// The legacy read stopped before the optional replacementObjectID (its flag bit is left over)
			RakNet::BitStream unequipWire;
			unequip.Serialize(unequipWire);
			RakNet::BitStream legacyUnequipStream(unequipWire.GetData(), unequipWire.GetNumberOfBytesUsed(), false);
			const auto legacyUnequip = LegacyGameMessages::ReadUnequipItem(legacyUnequipStream);
			EXPECT_EQ(legacyUnequipStream.GetReadOffset() + 1, unequipWire.GetNumberOfBitsUsed());
			EXPECT_EQ(RoundTrip(unequip).itemToUnequip, legacyUnequip.objectID);
			ExpectTruncatedFails(unequip);

			for (const auto value : g_Ints) {
				GameMessages::MoveItemInInventory move;
				move.destInvType = flag ? value : static_cast<int32_t>(eInventoryType::INVALID);
				move.iObjID = id;
				move.inventoryType = value;
				move.responseCode = -value;
				move.slot = value / 2;
				const auto legacyMove = ReadWithLegacy<LegacyGameMessages::LegacyMoveItemInInventory>(move, LegacyGameMessages::ReadMoveItemInInventory);
				const auto moveCopy = RoundTrip(move);
				EXPECT_EQ(moveCopy.destInvType, legacyMove.destInvType);
				EXPECT_EQ(moveCopy.iObjID, legacyMove.iObjID);
				EXPECT_EQ(moveCopy.inventoryType, legacyMove.inventoryType);
				EXPECT_EQ(moveCopy.responseCode, legacyMove.responseCode);
				EXPECT_EQ(moveCopy.slot, legacyMove.slot);
				ExpectTruncatedFails(move);

				GameMessages::RequestMoveItemBetweenInventoryTypes request;
				request.bAllowPartial = flag;
				request.destSlot = value;
				request.iStackCount = -value;
				request.invTypeDst = flag ? eInventoryType::VAULT_ITEMS : eInventoryType::ITEMS;
				request.invTypeSrc = flag ? eInventoryType::ITEMS : eInventoryType::MODELS;
				request.itemID = id;
				request.showFlyingLoot = !flag;
				request.subkey = id;
				request.itemLOT = value;
				const auto legacyRequest = ReadWithLegacy<LegacyGameMessages::LegacyRequestMove>(request, LegacyGameMessages::ReadRequestMoveItemBetweenInventoryTypes);
				const auto requestCopy = RoundTrip(request);
				EXPECT_EQ(requestCopy.bAllowPartial, legacyRequest.bAllowPartial);
				EXPECT_EQ(requestCopy.destSlot, legacyRequest.destSlot);
				EXPECT_EQ(requestCopy.iStackCount, legacyRequest.iStackCount);
				EXPECT_EQ(requestCopy.invTypeDst, legacyRequest.invTypeDst);
				EXPECT_EQ(requestCopy.invTypeSrc, legacyRequest.invTypeSrc);
				EXPECT_EQ(requestCopy.itemID, legacyRequest.itemID);
				EXPECT_EQ(requestCopy.showFlyingLoot, legacyRequest.showFlyingLoot);
				EXPECT_EQ(requestCopy.subkey, legacyRequest.subkey);
				if (value != LOT_NULL) EXPECT_EQ(requestCopy.itemLOT, legacyRequest.itemLOT); // unused; the old default was 0
				ExpectTruncatedFails(request);
			}

			for (const auto count : g_UInts) {
				for (const LOT lot : { LOT_NULL, LOT{ 1727 } }) {
					GameMessages::MoveItemBetweenInventoryTypes move;
					move.inventoryTypeA = eInventoryType::MODELS;
					move.inventoryTypeB = flag ? eInventoryType::VAULT_MODELS : eInventoryType::TEMP_MODELS;
					move.objectID = id;
					move.showFlyingLoot = flag;
					move.stackCount = count;
					move.templateID = lot;
					const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyMoveItemBetweenInventoryTypes>(move, LegacyGameMessages::ReadMoveItemBetweenInventoryTypes);
					const auto copy = RoundTrip(move);
					EXPECT_EQ(copy.inventoryTypeA, legacy.inventoryTypeA);
					EXPECT_EQ(copy.inventoryTypeB, legacy.inventoryTypeB);
					EXPECT_EQ(copy.objectID, legacy.objectID);
					EXPECT_EQ(copy.showFlyingLoot, legacy.showFlyingLoot);
					EXPECT_EQ(copy.stackCount, legacy.stackCount);
					EXPECT_EQ(copy.templateID, legacy.templateID);
					ExpectTruncatedFails(move);
				}
			}
		}

		GameMessages::ClientItemConsumed consumed;
		consumed.item = id;
		EXPECT_EQ(ReadWithLegacy<LWOOBJID>(consumed, LegacyGameMessages::ReadSingle<LWOOBJID>), id);
		EXPECT_EQ(RoundTrip(consumed).item, id);
		ExpectTruncatedFails(consumed);

		GameMessages::UseNonEquipmentItem use;
		use.itemToUse = id;
		EXPECT_EQ(ReadWithLegacy<LWOOBJID>(use, LegacyGameMessages::ReadSingle<LWOOBJID>), id);
		EXPECT_EQ(RoundTrip(use).itemToUse, id);
		ExpectTruncatedFails(use);
	}

	for (const auto lot : g_Ints) {
		GameMessages::SetConsumableItem consumable;
		consumable.itemTemplateID = lot;
		EXPECT_EQ(ReadWithLegacy<LOT>(consumable, LegacyGameMessages::ReadSingle<LOT>), lot);
		EXPECT_EQ(RoundTrip(consumable).itemTemplateID, lot);
		ExpectTruncatedFails(consumable);
	}

	for (const std::string action : std::vector<std::string>{ "ADD", "MODIFY", "REMOVE", "", "bogus" }) {
		for (const std::string& groupID : std::vector<std::string>{ "", "user_group1", std::string(200, 'g') }) {
			for (const auto& groupName : g_WStrings) {
				for (const auto type : { eInventoryType::ITEMS, eInventoryType::BRICKS, eInventoryType::MODELS }) {
					for (const bool locked : { false, true }) {
						GameMessages::UpdateInventoryGroup group;
						group.action = action;
						group.groupID = groupID;
						group.groupName = groupName;
						group.inventoryType = type;
						group.locked = locked;
						const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyGroupUpdate>(group, LegacyGameMessages::ReadUpdateInventoryGroup);
						ASSERT_TRUE(legacy.ok);
						const auto copy = RoundTrip(group);
						EXPECT_EQ(copy.action, legacy.action);
						EXPECT_EQ(copy.groupID, legacy.groupId);
						EXPECT_EQ(copy.groupName, legacy.groupName);
						EXPECT_EQ(copy.inventoryType, legacy.inventory);
						EXPECT_EQ(copy.locked, legacy.locked);
						ExpectTruncatedFails(group);
					}
				}
				for (const auto lot : g_Ints) {
					GameMessages::UpdateInventoryGroupContents contents;
					contents.action = action;
					contents.groupID = groupID;
					contents.inventoryType = eInventoryType::BRICKS;
					contents.lot = lot;
					const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyGroupUpdate>(contents, LegacyGameMessages::ReadUpdateInventoryGroupContents);
					ASSERT_TRUE(legacy.ok);
					const auto copy = RoundTrip(contents);
					EXPECT_EQ(copy.action, legacy.action);
					EXPECT_EQ(copy.groupID, legacy.groupId);
					EXPECT_EQ(copy.inventoryType, legacy.inventory);
					EXPECT_EQ(copy.lot, legacy.lot);
					ExpectTruncatedFails(contents);
				}
			}
		}
	}

	// A group name longer than the old handler allowed is rejected.
	RakNet::BitStream tooLong;
	tooLong.Write<uint32_t>(0);
	tooLong.Write<uint32_t>(0);
	tooLong.Write<uint32_t>(MAX_MESSAGE_LENGTH / 2 + 1);
	GameMessages::UpdateInventoryGroup group;
	EXPECT_FALSE(group.Deserialize(tooLong));
}

// MoveInventoryBatch had no oracle (the old function was never called); it follows the client's layout
// (legouniverse.exe 1.10.64, LWOInventoryComponent_Common::msgMoveInventoryBatch reads moveLOT and moveSubKey).
TEST_F(InventoryMessagesTests, MoveInventoryBatchRoundTrips) {
	GameMessages::MoveInventoryBatch batch;
	EXPECT_PACKET_EQ(FromHex("00 00", 9), Payload(batch));
	batch.bAllowPartial = true;
	batch.count = 3;
	batch.dstBag = eInventoryType::VAULT_ITEMS;
	batch.moveLOT = 1727;
	batch.moveSubkey = 0x22;
	batch.showFlyingLoot = true;
	batch.srcBag = eInventoryType::MODELS;
	batch.startObjectID = 0x33;
	const auto copy = RoundTrip(batch);
	EXPECT_EQ(copy.count, 3);
	EXPECT_EQ(copy.dstBag, eInventoryType::VAULT_ITEMS);
	EXPECT_EQ(copy.moveLOT, 1727);
	EXPECT_EQ(copy.moveSubkey, 0x22);
	EXPECT_EQ(copy.srcBag, eInventoryType::MODELS);
	EXPECT_EQ(copy.startObjectID, 0x33);
	ExpectTruncatedFails(batch);
}

// Independent of the legacy code: hand computed payloads (RakNet writes MSB first).
TEST_F(InventoryMessagesTests, GoldenBytes) {
	GameMessages::EquipInventory equip;
	equip.bOutSuccess = true;
	equip.itemToEquip = 0x11;
	// 0, 1, then 11 00 00 00 00 00 00 00
	EXPECT_PACKET_EQ(FromHex("44 40 00 00 00 00 00 00 00", 66), Payload(equip));

	GameMessages::ResponseMoveItemBetweenInventoryTypes response;
	response.inventoryTypeDestination = eInventoryType::VAULT_ITEMS; // 1
	response.response = eReponseMoveItemBetweenInventoryTypeCode::SUCCESS; // 0
	// 1 + 01 00 00 00, 0, 1 + 00 00 00 00
	EXPECT_PACKET_EQ(FromHex("80 80 00 00 20 00 00 00 00", 67), Payload(response));

	GameMessages::SetInventorySize size;
	size.inventoryType = eInventoryType::ITEMS;
	size.size = 20;
	EXPECT_PACKET_EQ(FromHex("00 00 00 00 14 00 00 00"), Payload(size));
}
