#include "OverTimeBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "Game.h"
#include "dLogger.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"

/**
 * The OverTime behavior is very inconsistent in how it appears in the skill tree vs. how it should behave.
 * 
 * Items like "Doc in a Box" use an overtime behavior which you would expect have health & armor regen, but is only fallowed by a stun.
 * 
 * Due to this inconsistency, we have to implement a special case for some items.
 */

void OverTimeBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) 
{
    const auto originator = context->originator;

    auto* entity = EntityManager::Instance()->GetEntity(originator);

    if (entity == nullptr)
    {
        return;
    }

    for (size_t i = 0; i < m_NumIntervals; i++)
    {
        entity->AddCallbackTimer((i + 1) * m_Delay, [originator, branch, this]() {
            auto* entity = EntityManager::Instance()->GetEntity(originator);

            if (entity == nullptr)
            {
                return;
            }

            auto* skillComponent = entity->GetComponent<SkillComponent>();

            if (skillComponent == nullptr)
            {
                return;
            }

            skillComponent->CalculateBehavior(0, m_Action->m_behaviorId, branch.target, true, true);

            auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

            if (destroyableComponent == nullptr)
            {
                return;
            }
            
            /**
             * Special cases for inconsistent behavior.
             */

            switch (m_behaviorId)
            {
            case 26253: // "Doc in a Box", heal up to 6 health and regen up to 18 armor.
                destroyableComponent->Heal(1);
                destroyableComponent->Repair(3);
                break;
            }
        });
    }
}

void OverTimeBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) 
{
    
}

void OverTimeBehavior::Load() 
{
    m_Action = GetAction("action");
    m_Delay = GetFloat("delay");
    m_NumIntervals = GetInt("num_intervals");
}
