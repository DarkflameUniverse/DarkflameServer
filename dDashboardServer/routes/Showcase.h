#pragma once

/**
 * The property showcase: browse other players' properties that are public and approved by a moderator, and look
 * around them in the 3D viewer (read only). Needs the showcase_view permission (every player by default), or nothing
 * at all with showcase_public=1. Private, friends-only, unapproved and rejected properties are never shown.
 */
void RegisterShowcaseRoutes();
