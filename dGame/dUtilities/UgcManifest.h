#ifndef UGCMANIFEST_H
#define UGCMANIFEST_H

#include <optional>
#include <string>
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
 * * Served meshes with the client's own physics: a client given the LXFML of a model whose mesh is served builds its
 *   own NIF and HKX, then (once it has loaded and a moment after) gets the served NIF's checksum and
 *   NotifyClientUGCModelReady for that model. It drops what it cached for the model and asks again: the NIF's checksum
 *   is now the served one, so it downloads the served mesh; the HKX's is still its own, so it keeps its collision.
 *
 * Main thread only: each lookup is one indexed query and the answers are sent from here. See docs/UgcServer.md.
 */
namespace UgcManifest {
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

	// ServesModels() and the UGC server made this model's mesh: a client that built the model from its LXFML (for its
	// physics) is then switched to the served mesh (ScheduleServedMesh)
	bool ServesMesh(LWOOBJID blueprintId);

	// The client was sent this model's LXFML: when the mesh is served, once the client has loaded (and a moment after,
	// for it to build the model) it gets the served NIF's checksum and NotifyClientUGCModelReady. At most a few times
	// per client and model (a client that keeps asking for the HKX isn't switched back and forth for ever). Returns
	// whether the mesh is served.
	bool ScheduleServedMesh(const SystemAddress& sysAddr, LWOOBJID blueprintId);

	void OnRequest(const SystemAddress& sysAddr, LWOOBJID blueprintId, eUgcResourceType resourceType);

	// Answers the waiting requests whose files have been made since, forgets the ones waited on too long, and switches
	// clients to served meshes that are due
	void Update();

	// A client left (or went back to character select): its waiting requests and pending switches are dropped
	void OnDisconnect(const SystemAddress& sysAddr);

	// The UGC server made these models' meshes (again, with a new checksum): each one placed in this world is sent the
	// new checksum and NotifyClientUGCModelReady, so clients load the served mesh
	void OnModelsMade(const std::vector<LWOOBJID>& blueprintIds);
}

#endif  //!UGCMANIFEST_H
