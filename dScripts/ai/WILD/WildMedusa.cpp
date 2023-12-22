#include "WildMedusa.h"
#include "EntityInfo.h"
#include "DestroyableComponent.h"
#include "RenderComponent.h"

void WildMedusa::OnStartup(Entity* self) {
	self->AddTimer("SnakeStartup", 0.2);
	self->AddToGroup("snakesonaplane");

	auto pos = self->GetPosition();

	EntityInfo snake1{};
	snake1.lot = 6564;
	snake1.spawnerID = self->GetObjectID();
	snake1.pos = NiPoint3(pos.x, pos.y, pos.z + 5);
	auto* snake1_entity = Game::entityManager->CreateEntity(snake1);
	Game::entityManager->ConstructEntity(snake1_entity);

	EntityInfo snake2{};
	snake2.lot = 6565;
	snake2.spawnerID = self->GetObjectID();
	snake2.pos = NiPoint3(pos.x, pos.y, pos.z - 5);
	auto* snake2_entity = Game::entityManager->CreateEntity(snake2);
	Game::entityManager->ConstructEntity(snake2_entity);
	
	EntityInfo snake3{};
	snake3.lot = 6566;
	snake3.spawnerID = self->GetObjectID();
	snake3.pos = NiPoint3(pos.x + 5, pos.y, pos.z);
	auto* snake3_entity = Game::entityManager->CreateEntity(snake3);
	Game::entityManager->ConstructEntity(snake3_entity);

	EntityInfo snake4{};
	snake4.lot = 6567;
	snake4.spawnerID = self->GetObjectID();
	snake4.pos = NiPoint3(pos.x - 5, pos.y, pos.z);
	auto* snake4_entity = Game::entityManager->CreateEntity(snake4);
	Game::entityManager->ConstructEntity(snake4_entity);

	EntityInfo snake5{};
	snake5.lot = 6568;
	snake5.spawnerID = self->GetObjectID();
	snake5.pos = NiPoint3(pos.x + 5, pos.y, pos.z - 5);
	auto* snake5_entity = Game::entityManager->CreateEntity(snake5);
	Game::entityManager->ConstructEntity(snake5_entity);
}

void WildMedusa:: OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	if (name == "next5"){
		auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
		if (destroyableComponent) destroyableComponent->SetFaction(4);
		auto* renderComponent = self->GetComponent<RenderComponent>();
		if (renderComponent) renderComponent->PlayEffect(634, u"medusa", "");
	}
}