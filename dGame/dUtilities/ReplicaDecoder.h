#ifndef __REPLICADECODER__H__
#define __REPLICADECODER__H__

#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "dCommonVars.h"
#include "json.hpp"

enum class eReplicaComponentType : uint32_t;

/**
 * Reads recorded replica packets (ID_REPLICA_MANAGER_CONSTRUCTION, _SERIALIZE and _DESTRUCTION) for the capture
 * viewer. A construction is the object's header (Entity::WriteBaseReplicaData) followed by each component's data in
 * the order the client reads them (Entity::WriteComponents); a serialization is the same for an object constructed
 * earlier on the same connection, so a Session is fed a capture's packets in timeline order and remembers what each
 * network ID is.
 *
 * Each component reader mirrors its component's Serialize(bIsInitialUpdate) bit for bit (ReplicaDecoderTests checks
 * them against the server's own serializers). Which components an object has comes from the ComponentsRegistry rows
 * of its LOT, as the client decides it. Some objects have components their LOT doesn't list (a smashable or a moving
 * platform set up by the zone file): when the listed ones don't read the packet exactly, those variants are tried, and
 * when none fits the packet is shown as far as it read, with the rest as bits (never a guess).
 */
namespace ReplicaDecoder {
	// LOT -> the component types its ComponentsRegistry rows list
	using ComponentTable = std::unordered_map<LOT, std::vector<eReplicaComponentType>>;

	// Reads the ComponentsRegistry from the CDClient (on the thread that owns it). Returns how many LOTs it read.
	size_t LoadComponentTable(ComponentTable& table);

	// The components the client reads for a LOT, in reading order
	std::vector<eReplicaComponentType> ComponentsOf(LOT lot, const ComponentTable& table);

	class Session {
	public:
		explicit Session(const ComponentTable& table) : m_Table{ table } {}

		/**
		 * The packet's fields, or nullopt when it isn't a replica packet. `connection` tells apart the network IDs of
		 * different connections (worlds, characters): use the same value for every packet of one connection.
		 */
		std::optional<nlohmann::json> Decode(std::string_view bytes, uint64_t connection);

	private:
		struct Object {
			LWOOBJID objectId{};
			LOT lot{};
			std::vector<eReplicaComponentType> components;
			bool trigger{}; // its trigger component is read after the others
		};

		const ComponentTable& m_Table;
		std::map<std::pair<uint64_t, uint16_t>, Object> m_Objects;
	};
}

#endif  //!__REPLICADECODER__H__
