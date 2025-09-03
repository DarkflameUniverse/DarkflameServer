#include "SimBehavior.h"
#include "SimUser.h"
#include <random>
#include <cmath>

SimBehavior::MovementPattern SimBehavior::GetRandomMovementPattern() {
    MovementPattern pattern;
    pattern.minDistance = 5.0f;
    pattern.maxDistance = 15.0f;
    pattern.moveSpeed = 5.0f;
    pattern.pauseTime = 2.0f;
    return pattern;
}

SimBehavior::MovementPattern SimBehavior::GetCircularMovementPattern(float radius) {
    MovementPattern pattern;
    pattern.minDistance = radius * 0.8f;
    pattern.maxDistance = radius * 1.2f;
    pattern.moveSpeed = 3.0f;
    pattern.pauseTime = 1.0f;
    return pattern;
}

SimBehavior::MovementPattern SimBehavior::GetLinearMovementPattern(float distance) {
    MovementPattern pattern;
    pattern.minDistance = distance;
    pattern.maxDistance = distance;
    pattern.moveSpeed = 4.0f;
    pattern.pauseTime = 0.5f;
    return pattern;
}

SimBehavior::ChatPattern SimBehavior::GetRandomChatPattern() {
    ChatPattern pattern;
    pattern.messages = {
        "Hello everyone!",
        "How's everyone doing?",
        "Nice day for building!",
        "Anyone want to race?",
        "This place looks cool!",
        "Great work on that build!",
        "Thanks for the help!",
        "See you around!",
        "What's your favorite zone?",
        "This game is awesome!"
    };
    pattern.intervalMin = 30.0f; // 30 seconds minimum
    pattern.intervalMax = 120.0f; // 2 minutes maximum
    return pattern;
}

SimBehavior::ChatPattern SimBehavior::GetSpamChatPattern() {
    ChatPattern pattern;
    pattern.messages = {
        "Test message 1",
        "Test message 2", 
        "Test message 3",
        "Spam test",
        "Load test message"
    };
    pattern.intervalMin = 1.0f; // 1 second minimum
    pattern.intervalMax = 3.0f; // 3 seconds maximum
    return pattern;
}

SimBehavior::ChatPattern SimBehavior::GetRoleplayChatPattern() {
    ChatPattern pattern;
    pattern.messages = {
        "*waves to everyone*",
        "*looks around curiously*",
        "*examines the nearby structures*",
        "*checks inventory*",
        "*stretches after a long journey*",
        "*admires the scenery*",
        "*practices building techniques*",
        "*takes a short break*",
        "*prepares for the next adventure*",
        "*organizes backpack*"
    };
    pattern.intervalMin = 45.0f; // 45 seconds minimum
    pattern.intervalMax = 180.0f; // 3 minutes maximum
    return pattern;
}

void SimBehavior::ExecuteRandomMovement(SimUser* user, const MovementPattern& pattern) {
    if (!user) return;
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
    static std::uniform_real_distribution<float> distDist(pattern.minDistance, pattern.maxDistance);
    
    // Generate random direction and distance
    float angle = angleDist(gen);
    float distance = distDist(gen);
    
    // Calculate target position
    NiPoint3 currentPos = user->GetPosition();
    NiPoint3 targetPos;
    targetPos.x = currentPos.x + distance * std::cos(angle);
    targetPos.y = currentPos.y;
    targetPos.z = currentPos.z + distance * std::sin(angle);
    
    // Simulate movement (this would be implemented in SimUser)
    // user->MoveTo(targetPos, pattern.moveSpeed);
}

void SimBehavior::ExecuteChat(SimUser* user, const ChatPattern& pattern) {
    if (!user || pattern.messages.empty()) return;
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<size_t> msgDist(0, pattern.messages.size() - 1);
    
    // Select random message
    size_t messageIndex = msgDist(gen);
    const std::string& message = pattern.messages[messageIndex];
    
    user->SendChatMessage(message);
}

void SimBehavior::ExecuteIdleBehavior(SimUser* user) {
    if (!user) return;
    
    // Simple idle behavior - just send occasional heartbeats
    // The user's Update method handles most of this
}

void SimBehavior::ExecuteStressTest(SimUser* user) {
    if (!user) return;
    
    // Stress test behavior - rapid actions
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> actionDist(1, 4);
    
    int action = actionDist(gen);
    
    switch (action) {
        case 1:
            // Rapid movement
            ExecuteRandomMovement(user, GetRandomMovementPattern());
            break;
            
        case 2:
            // Rapid chat
            ExecuteChat(user, GetSpamChatPattern());
            break;
            
        case 3:
            // Multiple position updates
            for (int i = 0; i < 5; i++) {
                user->SimulateMovement();
            }
            break;
            
        case 4:
            // Just maintain connection
            ExecuteIdleBehavior(user);
            break;
    }
}
