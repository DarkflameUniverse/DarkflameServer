#ifndef ESCENETYPE_H
#define ESCENETYPE_H

#include <cstdint>

// A zone scene's layer (the second half of the client's SceneAndLayer); zones split into a scene per layer
enum class eSceneType : uint32_t {
	General = 0, // the scene's objects ("Global Scene" and the named scenes)
	Audio = 1,   // *_audio.lvl scenes, named "Audio"
	FX = 2,      // *_fxs.lvl scenes, named "FXs" (Nexus Tower)
};

#endif //!ESCENETYPE_H
