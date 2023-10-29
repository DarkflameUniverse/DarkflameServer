#ifndef RENDERCOMPONENT_H
#define RENDERCOMPONENT_H

#include <BitStream.h>
#include <vector>
#include <string>
#include <unordered_map>

#include "Amf3.h"
#include "Component.h"
#include "eReplicaComponentType.h"

class Entity;

/**
 * An effect that plays for an entity. This might seem a bit abstract so digging through the CDClient is recommended
 * here.
 */
struct Effect {
	Effect() { priority = 1.0f; }

	/**
	 * The ID of the effect
	 */
	int32_t effectID = 0;

	/**
	 * The name of the effect
	 */
	std::string name = "";

	/**
	 * The type of the effect
	 */
	std::u16string type = u"";

	/**
	 * The importantness of the effect
	 */
	float priority = 1.0f;

	/**
	 * Some related entity that casted the effect
	 */
	uint64_t secondary = 0;

	/**
	 * The time that this effect plays for
	 */
	float time = 0;
};

/**
 * Determines that a component should be visibly rendered into the world, most entities have this. This component
 * also handles effects that play for entities.
 */
class RenderComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::RENDER;

	RenderComponent(Entity* entity, int32_t componentId = -1);
	~RenderComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) override;
	void Update(float deltaTime) override;

	/**
	 * Adds an effect to this entity, if successful the effect is returned
	 * @param effectId the ID of the effect
	 * @param name the name of the effect
	 * @param type the type of the effect
	 * @param priority the priority of the effect
	 * @return if successful, the effect that was created
	 */
	Effect* AddEffect(int32_t effectId, const std::string& name, const std::u16string& type, const float priority);

	/**
	 * Removes an effect for this entity
	 * @param name the name of the effect to remove
	 */
	void RemoveEffect(const std::string& name);

	/**
	 * Plays an effect, removes any effects under this name and plays the one according to these params
	 * @param effectId the ID of the effect
	 * @param effectType the type of the effect
	 * @param name the name of the effect
	 * @param secondary some entity that cast the effect
	 * @param priority effect priority (determines if the client will play it over other effects)
	 * @param scale effect scale
	 * @param serialize whether to serialize the change or not
	 */
	void PlayEffect(int32_t effectId, const std::u16string& effectType, const std::string& name, LWOOBJID secondary = LWOOBJID_EMPTY, float priority = 1, float scale = 1, bool serialize = true);

	/**
	 * Removes and stops the effect for a certain name
	 * @param name name of the effect to stop
	 * @param killImmediate whether ot not to immediately stop playing the effect or phase it out
	 */
	void StopEffect(const std::string& name, bool killImmediate = true);

	/**
	 * Returns the list of currently active effects
	 * @return
	 */
	std::vector<Effect*>& GetEffects();

	/**
	 * Verifies that an animation can be played on this entity by checking
	 * if it has the animation assigned to its group.  If it does, the animation is echo'd
	 * down to all clients to be played and the duration of the played animation is returned.
	 * If the animation did not exist or the function was called in an invalid state, 0 is returned.
	 *
	 * The logic here matches the exact client logic.
	 *
	 * @param self The entity that wants to play an animation
	 * @param animation The animation_type (animationID in the client) to be played.
	 * @param sendAnimation Whether or not to echo the animation down to all clients.
	 * @param priority The priority of the animation.  Only used if sendAnimation is true.
	 * @param scale	The scale of the animation.  Only used if sendAnimation is true.
	 *
	 * @return The duration of the animation that was played.
	 */
	static float DoAnimation(Entity* self, const std::string& animation, bool sendAnimation, float priority = 0.0f, float scale = 1.0f);

	static float PlayAnimation(Entity* self, const std::u16string& animation, float priority = 0.0f, float scale = 1.0f);
	static float PlayAnimation(Entity* self, const std::string& animation, float priority = 0.0f, float scale = 1.0f);
	static float GetAnimationTime(Entity* self, const std::string& animation);
	static float GetAnimationTime(Entity* self, const std::u16string& animation);

	const std::string& GetLastAnimationName() const { return m_LastAnimationName; };
	void SetLastAnimationName(const std::string& name) { m_LastAnimationName = name; };

private:

	/**
	 * List of currently active effects
	 */
	std::vector<Effect*> m_Effects;

	std::vector<int32_t> m_animationGroupIds;

	// The last animationName that was played
	std::string m_LastAnimationName;

	/**
	 * Cache of queries that look for the length of each effect, indexed by effect ID
	 */
	static std::unordered_map<int32_t, float> m_DurationCache;

	/**
	 * Cache for animation groups, indexed by the component ID
	 */
	static std::unordered_map<int32_t, std::vector<int32_t>> m_AnimationGroupCache;
};

#endif // RENDERCOMPONENT_H
