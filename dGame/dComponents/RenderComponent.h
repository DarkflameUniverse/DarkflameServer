#ifndef RENDERCOMPONENT_H
#define RENDERCOMPONENT_H

#include <BitStream.h>
#include <vector>
#include <string>
#include <unordered_map>

#include "AMFFormat.h"
#include "Component.h"

class Entity;

/**
 * An effect that plays for an entity. This might seem a bit abstract so digging through the CDClient is recommended
 * here.
 */
struct Effect {
	Effect() { scale = 1.0f; }

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
	 * How scaled (enlarged) the effect is
	 */
	float scale = 1.0f;

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
	static const uint32_t ComponentType = COMPONENT_TYPE_RENDER;

	RenderComponent(Entity* entity);
	~RenderComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	void Update(float deltaTime) override;

	/**
	 * Adds an effect to this entity, if successful the effect is returned
	 * @param effectId the ID of the effect
	 * @param name the name of the effect
	 * @param type the type of the effect
	 * @return if successful, the effect that was created
	 */
	Effect* AddEffect(int32_t effectId, const std::string& name, const std::u16string& type);

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

private:

	/**
	 * List of currently active effects
	 */
	std::vector<Effect*> m_Effects;

	/**
	 * Cache of queries that look for the length of each effect, indexed by effect ID
	 */
	static std::unordered_map<int32_t, float> m_DurationCache;
};

#endif // RENDERCOMPONENT_H
