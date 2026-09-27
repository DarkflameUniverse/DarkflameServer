#ifndef PROPERTYMESSAGES_H
#define PROPERTYMESSAGES_H

#include "GameMessages.h"

#include "NiQuaternion.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

class AMFArrayValue;

// Game messages for properties: renting, the property data, browsing and entering properties, placing and editing
// models, property behaviors, reports and the news screen's top properties.
// Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
// Messages received from a client hand their work to the property components (PropertyManagementComponent,
// PropertyVendorComponent, PropertyEntranceComponent, MultiZoneEntranceComponent) as the old handlers did.
namespace GameMessages {
	// Server -> client. No payload.
	struct OpenPropertyVendor : public NetGameMsg {
		OpenPropertyVendor() : NetGameMsg(MessageType::Game::OPEN_PROPERTY_VENDOR) {}
	};

	// Server -> client. No payload. Targets the property management object.
	struct OpenPropertyManagement : public NetGameMsg {
		OpenPropertyManagement() : NetGameMsg(MessageType::Game::OPEN_PROPERTY_MANAGEMENT) {}
	};

	// Server -> client. The client reads it with PropertyData::Deserialize (0x00c084f0).
	struct DownloadPropertyData : public NetGameMsg {
		DownloadPropertyData() : NetGameMsg(MessageType::Game::DOWNLOAD_PROPERTY_DATA) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// PropertyData::moderationStatus
		enum RejectionStatus : uint32_t {
			REJECTION_STATUS_APPROVED = 0,
			REJECTION_STATUS_PENDING = 1,
			REJECTION_STATUS_REJECTED = 2
		};

		LWOOBJID propertyId{};
		int32_t templateId{};
		uint16_t mapId{};
		uint16_t vendorMapId{};
		uint32_t cloneId{};
		std::u16string name{};
		std::u16string description{};
		std::u16string ownerName{};
		LWOOBJID ownerId{};
		uint32_t propertyType{};
		uint32_t zoneCode{};
		uint32_t rent{};
		uint32_t rentalPeriod{};
		uint64_t expirationDate{};
		uint32_t rentAmount{};
		uint64_t reputation{};
		std::u16string spawnName{};
		std::u16string templateName{};
		std::u16string templateDescription{};
		uint32_t rentDuration{};
		uint32_t votes{};
		uint32_t durationType{};
		uint8_t renew{};
		LWOOBJID ownerAccountID{};
		uint32_t moderationStatus{};
		std::u16string rejectionReason{};
		uint64_t lastLogoutTime{};
		uint32_t dayOfMonthPlaqueWasBought{};
		uint32_t repAchievementReq{};
		NiPoint3 zonePosition{};
		float maxBuildHeight{};
		uint64_t rentalDate{};
		uint8_t accessType{};
		std::vector<NiPoint3> pathPositions{}; // u32 count, then the points
	};

	// Server -> client.
	struct PropertyRentalResponse : public NetGameMsg {
		PropertyRentalResponse() : NetGameMsg(MessageType::Game::PROPERTY_RENTAL_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOCLONEID cloneid{};
		uint32_t code{};
		LWOOBJID propertyID{};
		int64_t rentdue{};
	};

	// Server -> client. No payload. Opens the property browser of a property launch pad.
	struct PropertyEntranceBegin : public NetGameMsg {
		PropertyEntranceBegin() : NetGameMsg(MessageType::Game::PROPERTY_ENTRANCE_BEGIN) {}
	};

	// Server -> client. One page of the property browser.
	struct PropertySelectQuery : public NetGameMsg {
		PropertySelectQuery() : NetGameMsg(MessageType::Game::PROPERTY_SELECT_QUERY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// One property, as the client's PropertyInfo.
		struct PropertyInfo {
			LWOCLONEID cloneId{ LWOCLONEID_INVALID };
			std::u16string ownerName{};
			std::u16string name{};
			std::u16string description{};
			float reputation{};
			bool isBff{};
			bool isFriend{};
			bool isModApproved{};
			bool isAlt{};
			bool isOwned{};
			uint32_t accessType{};
			uint64_t dateLastPublished{};
			float performanceCost{};
		};

		int32_t navOffset{};
		bool thereAreMore{};
		int32_t cloneId{};
		bool hasFeaturedProperty{};
		bool wasFriends{};
		std::vector<PropertyInfo> properties{}; // u32 count, then the entries
	};

	// Server -> client. Every model on the property: pairs of (model object, model item).
	struct GetModelsOnProperty : public NetGameMsg {
		GetModelsOnProperty() : NetGameMsg(MessageType::Game::GET_MODELS_ON_PROPERTY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::vector<std::pair<LWOOBJID, LWOOBJID>> models{}; // u32 count, then both IDs of each pair
	};

	// Server -> client. Laid out as the client's PlaceModelResponse::Deserialize (0x00dc0170) reads it: every field is
	// optional and the rotation is a w, x, y, z quaternion. response is 14 when a model was placed and 16 when one was
	// taken off the property (picked up or put away).
	struct PlaceModelResponse : public NetGameMsg {
		PlaceModelResponse() : NetGameMsg(MessageType::Game::PLACE_MODEL_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		NiPoint3 position{ NiPoint3Constant::ZERO }; // optional
		LWOOBJID propertyPlaqueID{ LWOOBJID_EMPTY }; // optional
		int32_t response{}; // optional
		NiQuaternion rotation{ QuatUtils::IDENTITY }; // optional
	};

	// Server -> client. No payload. Makes the client ask for the property's models again (PropertyContentsFromClient);
	// a live server sent it after a brick by brick save and after placing a model.
	struct RequeryPropertyModels : public NetGameMsg {
		RequeryPropertyModels() : NetGameMsg(MessageType::Game::REQUERY_PROPERTY_MODELS) {}
	};

	// Server -> client. Sent with id HANDLE_UGC_POST_CREATE_BASED_ON_EDIT_MODE (1301), which the client names
	// HandleUGCEquipPreCreateBasedOnEditMode.
	struct HandleUGCEquipPreCreateBasedOnEditMode : public NetGameMsg {
		HandleUGCEquipPreCreateBasedOnEditMode() : NetGameMsg(MessageType::Game::HANDLE_UGC_POST_CREATE_BASED_ON_EDIT_MODE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t modelCount{};
		LWOOBJID modelID{};
	};

	// Server -> client.
	struct HandleUGCEquipPostDeleteBasedOnEditMode : public NetGameMsg {
		HandleUGCEquipPostDeleteBasedOnEditMode() : NetGameMsg(MessageType::Game::HANDLE_UGC_POST_DELETE_BASED_ON_EDIT_MODE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID invItem{};
		int32_t itemsTotal{}; // optional
	};

	// Client -> server.
	struct SetPropertyAccess : public NetGameMsg {
		SetPropertyAccess() : NetGameMsg(MessageType::Game::SET_PROPERTY_ACCESS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		uint8_t accessType{}; // optional
		int32_t renew{}; // optional
	};

	// Client -> server. A new name and description for the property.
	struct UpdatePropertyOrModelForFilterCheck : public NetGameMsg {
		UpdatePropertyOrModelForFilterCheck() : NetGameMsg(MessageType::Game::UPDATE_PROPERTY_OR_MODEL_FOR_FILTER_CHECK) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool isProperty{};
		LWOOBJID ugcId{};
		LWOOBJID playerId{};
		LWOOBJID worldId{};
		std::u16string newDescription{};
		std::u16string newName{};
	};

	// Client -> server. No payload.
	struct QueryPropertyData : public NetGameMsg {
		QueryPropertyData() : NetGameMsg(MessageType::Game::QUERY_PROPERTY_DATA) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Client -> server.
	struct PropertyEditorBegin : public NetGameMsg {
		PropertyEditorBegin() : NetGameMsg(MessageType::Game::PROPERTY_EDITOR_BEGIN) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t distanceType{}; // optional
		LWOOBJID propertyObjectID{ LWOOBJID_EMPTY }; // optional
		int32_t startMode{ 1 }; // optional
		bool startPaused{};
	};

	// Client -> server. No payload.
	struct PropertyEditorEnd : public NetGameMsg {
		PropertyEditorEnd() : NetGameMsg(MessageType::Game::PROPERTY_EDITOR_END) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Client -> server.
	struct PropertyContentsFromClient : public NetGameMsg {
		PropertyContentsFromClient() : NetGameMsg(MessageType::Game::PROPERTY_CONTENTS_FROM_CLIENT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool queryDB{};
	};

	// Client -> server (DLU never sends it).
	struct ZonePropertyModelEquipped : public NetGameMsg {
		ZonePropertyModelEquipped() : NetGameMsg(MessageType::Game::ZONE_PROPERTY_MODEL_EQUIPPED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID playerID{ LWOOBJID_EMPTY }; // optional
		LWOOBJID propertyID{ LWOOBJID_EMPTY }; // optional
	};

	// Client -> server.
	struct ZonePropertyModelRotated : public NetGameMsg {
		ZonePropertyModelRotated() : NetGameMsg(MessageType::Game::ZONE_PROPERTY_MODEL_ROTATED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID playerID{ LWOOBJID_EMPTY }; // optional
		LWOOBJID propertyID{ LWOOBJID_EMPTY }; // optional
	};

	// Client -> server.
	struct PlacePropertyModel : public NetGameMsg {
		PlacePropertyModel() : NetGameMsg(MessageType::Game::PLACE_PROPERTY_MODEL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID modelID{};
	};

	// Client -> server.
	struct UpdateModelFromClient : public NetGameMsg {
		UpdateModelFromClient() : NetGameMsg(MessageType::Game::UPDATE_MODEL_FROM_CLIENT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID modelID{};
		NiPoint3 position{};
		NiQuaternion rotation{ QuatUtils::IDENTITY }; // optional
	};

	// Client -> server.
	struct DeleteModelFromClient : public NetGameMsg {
		DeleteModelFromClient() : NetGameMsg(MessageType::Game::DELETE_MODEL_FROM_CLIENT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID modelID{ LWOOBJID_EMPTY }; // optional
		int32_t reason{}; // optional
	};

	// Client -> server. Asks a property launch pad for a page of properties.
	struct PropertyEntranceSync : public NetGameMsg {
		PropertyEntranceSync() : NetGameMsg(MessageType::Game::PROPERTY_ENTRANCE_SYNC) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool includeNullAddress{};
		bool includeNullDescription{};
		bool playersOwn{};
		bool updateUI{};
		int32_t numResults{};
		int32_t reputationTime{};
		int32_t sortMethod{};
		int32_t startIndex{};
		std::string filterText{};
	};

	// Client -> server. The player picked a property (or a world, on a multi zone entrance).
	struct EnterProperty1 : public NetGameMsg {
		EnterProperty1() : NetGameMsg(MessageType::Game::ENTER_PROPERTY1) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t index{};
		bool returnToZone{ true };
	};

	// Client -> server.
	struct UpdatePropertyPerformanceCost : public NetGameMsg {
		UpdatePropertyPerformanceCost() : NetGameMsg(MessageType::Game::UPDATE_PROPERTY_PERFORMANCE_COST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		float performanceCost{}; // optional
	};

	// Client -> server.
	struct ReportOffensiveModel : public NetGameMsg {
		ReportOffensiveModel() : NetGameMsg(MessageType::Game::REPORT_OFFENSIVE_MODEL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::u16string description{};
		LWOOBJID offensiveObjectID{};
	};

	// Client -> server.
	struct ReportOffensiveProperty : public NetGameMsg {
		ReportOffensiveProperty() : NetGameMsg(MessageType::Game::REPORT_OFFENSIVE_PROPERTY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::u16string description{};
		LWOOBJID propertyPlaqueObjectID{};
	};

	/**
	 * Client -> server. Every property behavior (model behavior editor) command.
	 * args is an AMF3 array; for AMF3 see https://rtmp.veriskope.com/pdf/amf3-file-format-spec.pdf
	 */
	struct ControlBehaviors : public NetGameMsg {
		ControlBehaviors();
		~ControlBehaviors() override;
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::unique_ptr<AMFArrayValue> args{};
		std::string command{};
	};

	// Client -> server. No payload. The news screen asks for today's top properties.
	struct GetHotPropertyData : public NetGameMsg {
		GetHotPropertyData() : NetGameMsg(MessageType::Game::GET_HOT_PROPERTY_DATA) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	/**
	 * Server -> client. The news screen's "Today's Top Properties" (GM 1510), the answer to GetHotPropertyData (GM 1511).
	 * Client: GameMessage::NewsSendHotPropertiesInfoToClient, entries read by NewsHotPropertyInfo::Deserialize
	 * (0x00c0cc20 in 1.10.64) and shown by LWOCharacterComponent::HotPropertyData (0x00cf83f0). See HotPropertySlots.h.
	 */
	struct NewsSendHotPropertiesInfoToClient : public NetGameMsg {
		NewsSendHotPropertiesInfoToClient() : NetGameMsg(MessageType::Game::SEND_HOT_PROPERTY_DATA) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		struct HotPropertyInfo {
			LWOOBJID propertyId{};
			LWOOBJID ownerId{};
			std::u16string ownerName;
			uint64_t reputation{};
			int32_t templateId{};        // PropertyTemplate id: picks the news screen slot
			std::u16string name;
			std::u16string description;
			float performanceCost{};
			uint64_t lastPublished{};    // unix time
			uint32_t cloneId{};
		};

		std::vector<HotPropertyInfo> properties;
	};

	struct PlayBehaviorSound : public NetGameMsg {
		PlayBehaviorSound() : NetGameMsg(MessageType::Game::PLAY_BEHAVIOR_SOUND) {}

		void Serialize(RakNet::BitStream& stream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t soundID{ -1 };
	};
};

#endif // PROPERTYMESSAGES_H
