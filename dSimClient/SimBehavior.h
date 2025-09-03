#pragma once

#include <vector>
#include <string>

/**
 * @brief Defines and manages different test behaviors for simulated clients
 */
class SimBehavior {
public:
    enum class BehaviorType {
        IDLE,
        RANDOM_MOVEMENT,
        CHAT_SPAM,
        RAPID_RECONNECT,
        INVENTORY_ACTIONS,
        SKILL_USAGE,
        BUILDING_ACTIONS,
        RACING_ACTIONS
    };

    struct MovementPattern {
        float minDistance;
        float maxDistance;
        float moveSpeed;
        float pauseTime;
    };

    struct ChatPattern {
        std::vector<std::string> messages;
        float intervalMin;
        float intervalMax;
    };

    static MovementPattern GetRandomMovementPattern();
    static MovementPattern GetCircularMovementPattern(float radius);
    static MovementPattern GetLinearMovementPattern(float distance);
    
    static ChatPattern GetRandomChatPattern();
    static ChatPattern GetSpamChatPattern();
    static ChatPattern GetRoleplayChatPattern();
    
    // Behavior executors
    static void ExecuteRandomMovement(class SimUser* user, const MovementPattern& pattern);
    static void ExecuteChat(class SimUser* user, const ChatPattern& pattern);
    static void ExecuteIdleBehavior(class SimUser* user);
    static void ExecuteStressTest(class SimUser* user);
    
private:
    SimBehavior() = delete;
};
