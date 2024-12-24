#include "PetComponent.h"
#include "MovementAIComponent.h"
#include "MissionComponent.h"
#include "eMissionState.h"

using Pet = PetComponent;
using Mission = MissionComponent;
using MovementAI = MovementAIComponent;

struct Position : NiPoint3 {};
struct Treasure {};

namespace decs {
    template <typename... Cs>
    class System {
    public:
        template <typename Fn>
        void each(Fn&& fn) {
            fn();
        }
    };

    class Scene {
    public:
        template <typename... Cs>
        System<Cs...> system() {
            return System<Cs...>{};
        }
    };
}

void run() {
    auto scene = decs::Scene{};

    scene.system<Pet>()
        .each([](Pet& pet) {

        });

    scene.system<Pet, MovementAI>()
        .each([](Pet& pet, MovementAI& move) {

        });

    scene.system<Pet, const Mission, const Position>()
        .each([](Pet& pet, Mission const& mission, Position const& pos) {
            auto const digUnlocked = mission.GetMissionState(842) == eMissionState::COMPLETE;
            auto const treasurePos = PetDigServer::GetClosestTreasure(pos);


        });
}
