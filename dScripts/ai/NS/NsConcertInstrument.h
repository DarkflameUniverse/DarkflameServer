#pragma once
#include "CppScripts.h"

enum InstrumentLot {
	Guitar = 4039,
	Bass = 4040,
	Keyboard = 4041,
	Drum = 4042
};

class NsConcertInstrument : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnRebuildNotifyState(Entity* self, eRebuildState state) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string name) override;
private:
	static void StartPlayingInstrument(Entity* self, Entity* player);
	static void StopPlayingInstrument(Entity* self, Entity* player);
	static void EquipInstruments(Entity* self, Entity* player);
	static void UnEquipInstruments(Entity* self, Entity* player);
	static void RepositionPlayer(Entity* self, Entity* player);
	static InstrumentLot GetInstrumentLot(Entity* self);

	/**
	 * Animations played when using an instrument
	 */
	static const std::map<InstrumentLot, std::u16string> animations;

	/**
	 * Animation played when an instrument is smashed
	 */
	static const std::map<InstrumentLot, std::u16string> smashAnimations;

	/**
	 * Music to play while playing an instrument
	 */
	static const std::map<InstrumentLot, std::string> music;

	/**
	 * Cinematics to play while playing an instrument
	 */
	static const std::map<InstrumentLot, std::u16string> cinematics;

	/**
	 * Lot to equip in your left hand when playing an instrument
	 */
	static const std::map<InstrumentLot, LOT> instrumentLotLeft;

	/**
	 * Lot to play in your right hand when playing an instrument
	 */
	static const std::map<InstrumentLot, LOT> instrumentLotRight;

	/**
	 * Whether to hide the instrument or not when someone is playing it
	 */
	static const std::map<InstrumentLot, bool> hideInstrumentOnPlay;

	/**
	 * How long to wait before unequipping the instrument if the instrument was smashed
	 */
	static const std::map<InstrumentLot, float> instrumentEquipTime;

	/**
	 * How long the smash animation takes for each of the instruments
	 */
	static const std::map<InstrumentLot, float> instrumentSmashAnimationTime;

	/**
	 * Task ID of tasks of the Solo Artist 2 achievement
	 */
	static const std::map<InstrumentLot, uint32_t> achievementTaskID;

	/**
	 * How much imagination playing an instrument costs per interval
	 */
	static const uint32_t instrumentImaginationCost;

	/**
	 * The interval to deduct imagination at when playing an instrument
	 */
	static const float instrumentCostFrequency;

	/**
	 * The interval to check if the player still has enough imagination
	 */
	static const float updateFrequency;
};
