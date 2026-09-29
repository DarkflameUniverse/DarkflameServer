#ifndef UGCMANIFEST_H
#define UGCMANIFEST_H

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "dCommonVars.h"
#include "eUgcResourceType.h"
#include "IUgc.h"
#include "RakNetTypes.h"

/**
 * Answers the client's REQUEST_UGC_MANIFEST_INFO (sent when its UGCUSE3DSERVICES is off, the default): the MD5 and
 * size of a blueprint's file, which the client needs before it uses or downloads the file from the UGC server
 * (BrickModels/UserMade/<id % 1000>/<id>.<ext>.sd0). Nothing is answered unless `ugc_manifest` is 1.
 *
 * * Icons (DDS): the checksum the UGC server stored when it made the icon (ugc_file_checksums); one it hasn't made yet
 *   is answered once it has (Update looks again every few seconds for a while).
 * * Player models (NIF, HKX, LXFML): a placed model's client always asks for all three (its BlueprintComponent loads the
 *   blueprint's NIF and HKX, its ModelBehaviorComponent the LXFML) and waits for the answers without a timeout. With
 *   `ugc_manifest_models` 1 and the model's mesh made, the NIF is answered with the UGC server's checksum (the client
 *   downloads the served mesh) and the LXFML with the stored LXFML's. Otherwise, and always for the HKX (the UGC server
 *   makes no physics), the world sends that client the model's LXFML (BlueprintSaveResponse): the client builds the NIF
 *   and HKX itself and caches their checksums, which answers its own requests, so a model is never left waiting.
 * * Served meshes with the client's own physics: the client shows the served NIF it downloaded, and asks for the HKX,
 *   which is answered with the model's LXFML: it builds the model from it and loads its own HKX. The mesh it has drawn
 *   stays the served one.
 * * A client building a model from its LXFML writes its own .nif over the served one and caches that file's MD5 as the
 *   NIF's manifest info when it's done (LWOBBBInterface::GenerateModelFromLxfml, 0x00b6c220;
 *   MainThread_ProcessModelResponse, 0x00b5a1e0). So a client is switched to a served mesh only once it can't still be
 *   building that model (ServedMeshSwitches).
 *
 * Main thread only: each lookup is one indexed query and the answers are sent from here. See docs/UgcServer.md.
 */
namespace UgcManifest {
	using Clock = std::chrono::steady_clock;

	// How long after a client was sent a model's LXFML it may still be building the model from it
	constexpr auto BUILD_SETTLE = std::chrono::seconds(30);

	// Which clients get switched to which served meshes, and when: at once, or BUILD_SETTLE after the model's LXFML was
	// last sent to that client (a switch made while it builds is undone by its build). Main thread only.
	class ServedMeshSwitches {
	public:
		struct Switch {
			SystemAddress sysAddr;
			LWOOBJID blueprintId{};
		};

		// The model's LXFML was sent to the client: a switch waiting for it waits BUILD_SETTLE from now
		void LxfmlSent(const SystemAddress& sysAddr, LWOOBJID blueprintId, Clock::time_point now);
		// The model's LXFML was sent to the client less than `within` ago
		bool SentWithin(const SystemAddress& sysAddr, LWOOBJID blueprintId, Clock::duration within, Clock::time_point now) const;
		// The client is to be switched to the model's served mesh once it can't be building it any more
		void Schedule(const SystemAddress& sysAddr, LWOOBJID blueprintId, Clock::time_point now);
		// The switches that are due, removed from the pending ones
		std::vector<Switch> TakeDue(Clock::time_point now);
		// The client left
		void Forget(const SystemAddress& sysAddr);
		size_t Pending() const { return m_Due.size(); }

	private:
		using Key = std::pair<SystemAddress, LWOOBJID>;
		std::map<Key, Clock::time_point> m_LxfmlSent;
		std::map<Key, Clock::time_point> m_Due;
	};

	// What a request gets
	enum class eAction {
		NONE,          // not answered (ugc_manifest off, or not a UGC type)
		ANSWER,        // the stored (or, for LXFML, computed) checksum
		ANSWER_UNKNOWN,// valid 0: the client uses the file it has, or downloads it
		SEND_LXFML,    // the model's LXFML, for the client to build the model itself
		WAIT,          // icons not made yet: answered once they are
	};

	// The rule, for a request of `type` when the UGC server has (`meshMade`) or hasn't made the blueprint's mesh
	eAction Decide(eUgcResourceType type, bool manifestOn, bool modelsOn, bool meshMade);

	// The MD5 (lowercase hex) and size of a stored LXFML (ugc.lxfml: sd0, or plain XML) as the client has it after
	// downloading and inflating it; nothing when it can't be read
	std::optional<IUgc::FileChecksum> LxfmlChecksum(const std::string& stored);

	// ugc_manifest=1 and ugc_manifest_models=1: placed models' meshes come from the UGC server when made
	bool ServesModels();

	// ServesModels() and the UGC server made this model's mesh: the model's LXFML isn't sent when a property loads, the
	// client downloads the served mesh, and builds the model's physics from the LXFML sent when it asks for the HKX
	bool ServesMesh(LWOOBJID blueprintId);

	void OnRequest(const SystemAddress& sysAddr, LWOOBJID blueprintId, eUgcResourceType resourceType);

	// Answers the waiting requests whose files have been made since, forgets the ones waited on too long, and switches
	// clients to served meshes that are due
	void Update();

	// /reprocessproperty: every model placed on the property is made again by the UGC server; once none is waiting
	// (or after 15 minutes), every player in this world is sent the new mesh checksums and transferred back into it,
	// so their client loads the property again with the new meshes. Returns how many models were queued.
	size_t ReprocessProperty(LWOOBJID propertyId);

	// A client left (or went back to character select): its waiting requests and pending switches are dropped
	void OnDisconnect(const SystemAddress& sysAddr);

	// The UGC server made these models' meshes (again, with a new checksum): every client this world shows one of them
	// to is switched to the served mesh (SwitchClient), at once or once it can't be building that model any more
	void OnModelsMade(const std::vector<LWOOBJID>& blueprintIds);

	// A player is loading a property (before its models are constructed for them): the client is sent the served NIF
	// checksum of every placed model whose mesh the UGC server made. Its cached one can be its own build's (the LXFML it
	// was sent for the HKX, or before the model was made, replaced the served .nif and its checksum), which the client
	// would use as it is; with the served checksum it downloads the served mesh again. Returns how many were sent.
	size_t OnPropertyLoading(const SystemAddress& sysAddr, LWOOBJID propertyId);

	// A model's LXFML was sent to a client another way (a brick by brick save, a property load): it builds the model
	void OnLxfmlSent(const SystemAddress& sysAddr, LWOOBJID blueprintId);

	// Switches one client to a model's served mesh: the served NIF's checksum (the client's cached one is its own build's),
	// then NotifyClientUGCModelReady to each of `modelIds` (flushes the cached NIF, HKX and LXFML and preloads the NIF,
	// which is downloaded, and the HKX, still the client's own), then each model taken down and constructed again for
	// that client (Update) so its render loads the served NIF
	void SwitchClient(const SystemAddress& sysAddr, LWOOBJID blueprintId, const IUgc::FileChecksum& checksum, const std::vector<LWOOBJID>& modelIds);
}

#endif  //!UGCMANIFEST_H
