#include "Play.h"

#include "Scene.h"

#include "EntityManager.h"

using namespace Cinema;

void Cinema::Play::Conclude() {
	auto* player = Game::entityManager->GetEntity(this->player);

	if (player == nullptr) {
		return;
	}

	scene->Conclude(player);
}

void Cinema::Play::SetupCheckForAudience() {
	if (m_CheckForAudience) {
		return;
	}

	m_CheckForAudience = true;

	CheckForAudience();
}

void Cinema::Play::CheckForAudience() {
	auto* player = Game::entityManager->GetEntity(this->player);

	LOG("Checking for audience");

	if (player == nullptr) {
		CleanUp();

		return;
	}
	
	if (!scene->IsPlayerInBounds(player)) {
		Conclude();

		CleanUp();

		return;
	}

	// Still don't care
	Game::entityManager->GetZoneControlEntity()->AddCallbackTimer(1.0f, [this]() {
		CheckForAudience();
	});
}

void Cinema::Play::CleanUp() {
	LOG("Cleaning up play with %d entities", entities.size());

	for (const auto& entity : entities) {
		Game::entityManager->DestroyEntity(entity);
	}
}

void Cinema::Play::SetupBarrier(const std::string& barrier, std::function<void()> callback) {
	// Add the callback to the barrier
	if (m_Barriers.find(barrier) == m_Barriers.end()) {
		m_Barriers[barrier] = std::vector<std::function<void()>>();
	}

	m_Barriers[barrier].push_back(callback);
}

void Cinema::Play::SignalBarrier(const std::string& barrier) {
	if (m_Barriers.find(barrier) == m_Barriers.end()) {
		LOG("Barrier %s does not exist", barrier.c_str());
		return;
	}

	for (const auto& callback : m_Barriers[barrier]) {
		callback();
	}

	m_Barriers.erase(barrier);
}


