#include "Play.h"

#include "Scene.h"

#include "EntityManager.h"

using namespace Cinema;

void Play::Conclude() {
	auto* player = Game::entityManager->GetEntity(this->player);

	if (player == nullptr) {
		return;
	}

	scene->Conclude(player);
}

void Play::SetupCheckForAudience() {
	if (m_CheckForAudience) {
		return;
	}

	m_CheckForAudience = true;

	CheckForAudience();
}

void Play::CheckForAudience() {
	auto* player = Game::entityManager->GetEntity(this->player);

	if (player == nullptr) {
		CleanUp();

		return;
	}

	if (scene->IsPlayerInBounds(player)) {
		SignalBarrier("audience");
		
		m_PlayerHasBeenInsideBounds = true;
	}
	
	if (!scene->IsPlayerInShowingDistance(player)) {
		if (m_PlayerHasBeenInsideBounds) {
			Conclude();
		}

		CleanUp();

		return;
	}

	// As the scene isn't associated with a specifc objects, we'll use the zone control entity to setup a callback.
	Game::entityManager->GetZoneControlEntity()->AddCallbackTimer(1.0f, [this]() {
		CheckForAudience();
	});
}

void Play::CleanUp() {
	LOG("Cleaning up play with %d entities", entities.size());

	for (const auto& entity : entities) {
		Game::entityManager->DestroyEntity(entity);
	}
}

void Play::SetupBarrier(const std::string& barrier, const std::function<void()>& callback) {
	// Add the callback to the barrier
	m_Barriers[barrier].push_back(callback);
}

void Play::SignalBarrier(const std::string& barrier) {
	const auto& it = m_Barriers.find(barrier);

	if (it == m_Barriers.end()) {
		return;
	}

	for (const auto& callback : it->second) {
		callback();
	}

	m_Barriers.erase(it);
}


