#ifndef UGCMANIFEST_H
#define UGCMANIFEST_H

#include "dCommonVars.h"
#include "eUgcResourceType.h"
#include "RakNetTypes.h"

/**
 * Answers the client's REQUEST_UGC_MANIFEST_INFO (sent when its UGCUSE3DSERVICES is off, the default): the MD5 and
 * size of a blueprint's file, which the client needs before it downloads the file from the UGC server
 * (BrickModels/UserMade/<id % 1000>/<id>.<ext>.sd0). The checksums are the ones the UGC server stored when it made the
 * files (ugc_file_checksums); a file it hasn't made yet is answered once it has (Update looks again every few seconds
 * for a while). Nothing is answered for files it never makes (LXFML, HKX), or unless `ugc_manifest` is 1: the client
 * then downloads from http://127.0.0.1:80/lwoclient/UserBrickModels/ whatever its boot.cfg says, and is logged out when
 * it can't connect there.
 *
 * Main thread only: each lookup is one indexed query and the answers are sent from here. See docs/UgcServer.md.
 */
namespace UgcManifest {
	void OnRequest(const SystemAddress& sysAddr, LWOOBJID blueprintId, eUgcResourceType resourceType);

	// Answers the waiting requests whose files have been made since, and forgets the ones waited on too long
	void Update();

	// A client left: its waiting requests are dropped
	void OnDisconnect(const SystemAddress& sysAddr);
}

#endif  //!UGCMANIFEST_H
