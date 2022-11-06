#include "BaseWavesGenericEnemy.h"
#include "BaseEnemyApe.h"

class WaveBossApe : public BaseEnemyApe, public BaseWavesGenericEnemy {
	uint32_t GetPoints() override { return 5000; }
	void OnStartup(Entity* self) override;
	void OnDie(Entity* self, Entity* killer) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
};
