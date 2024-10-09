#pragma once
#include "CppScripts.h"

class EnemyNjBuff : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override;

	void RunDieCallback(Entity* self);

private:

//	LootMatrix data
	std::vector<std::vector<int>> enemies = {
		{12000, 13068, 14491, 32335},
		{14007, 14024, 14025, 16835, 16836, 14008, 14026},		
		{12002, 14008, 12004, 14009, 14027, 14028, 14029, 16511, 16846, 16847, 16848, 16849, 16938},
		{13996, 14001, 13997, 13998, 13999, 14000, 14002, 16191, 16850, 32336, 12005, 13995},		
		{16047, 16048, 16049, 16050, 16851, 16852, 16853, 16854}
	};

	std::vector<std::unordered_map<std::string, int>> lootTablesByDifficulty = {
		{{"Earth", 32879}, {"Lightning", 32880}, {"Ice", 32881}, {"Fire", 32878}}, 
		{{"Earth", 32883}, {"Lightning", 32884}, {"Ice", 32885}, {"Fire", 32882}}, 
		{{"Earth", 32887}, {"Lightning", 32888}, {"Ice", 32889}, {"Fire", 32886}}, 
		{{"Earth", 32891}, {"Lightning", 32877}, {"Ice", 32875}, {"Fire", 32890}}, 
		{{"Earth", 32893}, {"Lightning", 32876}, {"Ice", 32874}, {"Fire", 32892}}  
	};
	
};

