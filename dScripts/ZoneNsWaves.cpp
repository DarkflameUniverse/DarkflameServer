#include "ZoneNsWaves.h"

WaveConstants ZoneNsWaves::GetConstants() {
	return {
		60,
		2,
		6,
		2,
		"surprise",
		"intro"
	};
}

std::vector<std::string> ZoneNsWaves::GetSpawnerNames() {
	return {
		"Base_MobA",
		"Base_MobB",
		"Base_MobC",
		"MobA_01",
		"MobB_01",
		"MobC_01",
		"MobA_02",
		"MobB_02",
		"MobC_02",
		"MobA_03",
		"MobB_03",
		"MobC_03",
		"Reward_01",
		"Base_Reward",
		"Obstacle_01",
		"Boss",
		"Ape_Boss",
		"Geyser_01",
		"Treasure_01",
		"Cavalry_Boss",
		"Horseman_01",
		"Horseman_02",
		"Horseman_03",
		"Horseman_04"
	};
}

std::vector<WaveMission> ZoneNsWaves::GetWaveMissions() {
	return {
		{190, 7, 1242},
		{240, 7, 1226},
		{450, 15, 1243},
		{600, 15, 1227},
		{720, 22, 1244},
		{840, 22, 1228},
		{1080, 29, 1245},
		{1200, 29, 1229},
	};
}

std::vector<Wave> ZoneNsWaves::GetWaves() {
	return {
		// Wave 1
		Wave {
			std::vector<MobDefinition> {
				{ SpawnLOTS::stromling_minifig, 8, GetSpawnerName(SpawnerName::interior_A) },
				{ SpawnLOTS::stromling_minifig, 2, GetSpawnerName(SpawnerName::ag_A) },
				{ SpawnLOTS::stromling_minifig, 2, GetSpawnerName(SpawnerName::concert_A) },
				{ SpawnLOTS::stromling_minifig, 2, GetSpawnerName(SpawnerName::gf_A) },
			}
		},

		// Wave 2
		Wave {
			std::vector<MobDefinition> {
				{ SpawnLOTS::stromling, 8, GetSpawnerName(SpawnerName::interior_A) },
				{ SpawnLOTS::stromling, 2, GetSpawnerName(SpawnerName::ag_A) },
				{ SpawnLOTS::stromling, 2, GetSpawnerName(SpawnerName::concert_A) },
				{ SpawnLOTS::stromling, 2, GetSpawnerName(SpawnerName::gf_A) },
			}
		},

			// Wave 3
			Wave {
				std::vector<MobDefinition> {
					{ SpawnLOTS::stromling, 4, GetSpawnerName(SpawnerName::interior_A) },
					{ SpawnLOTS::mech, 2, GetSpawnerName(SpawnerName::interior_B) },
					{ SpawnLOTS::stromling, 3, GetSpawnerName(SpawnerName::ag_A) },
					{ SpawnLOTS::stromling, 3, GetSpawnerName(SpawnerName::concert_A) },
					{ SpawnLOTS::stromling, 3, GetSpawnerName(SpawnerName::gf_A) },
				},
			},

			// Wave 4
			Wave {
				std::vector<MobDefinition> {
					{ SpawnLOTS::stromling, 3, GetSpawnerName(SpawnerName::interior_A) },
					{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::interior_B) },
					{ SpawnLOTS::stromling, 2, GetSpawnerName(SpawnerName::ag_A) },
					{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::ag_B) },
					{ SpawnLOTS::stromling, 2, GetSpawnerName(SpawnerName::concert_A) },
					{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::concert_B) },
					{ SpawnLOTS::stromling, 2, GetSpawnerName(SpawnerName::gf_A) },
					{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::gf_B) },
				}
			},

						// Wave 5
						Wave {
							std::vector<MobDefinition> {
								{ SpawnLOTS::stromling, 2, GetSpawnerName(SpawnerName::interior_A) },
								{ SpawnLOTS::spiderling, 1, GetSpawnerName(SpawnerName::interior_C) },
								{ SpawnLOTS::hammerling_melee, 2, GetSpawnerName(SpawnerName::ag_A) },
								{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::ag_B) },
								{ SpawnLOTS::stromling, 1, GetSpawnerName(SpawnerName::concert_A) },
								{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::concert_B) },
								{ SpawnLOTS::stromling, 1, GetSpawnerName(SpawnerName::gf_A) },
								{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::gf_B) },
							}
						},

				// Wave 6
				Wave {
					std::vector<MobDefinition> {
						{ SpawnLOTS::hammerling_melee, 1, GetSpawnerName(SpawnerName::interior_A) },
						{ SpawnLOTS::mech, 2, GetSpawnerName(SpawnerName::interior_B) },
						{ SpawnLOTS::spiderling, 1, GetSpawnerName(SpawnerName::interior_C) },
						{ SpawnLOTS::hammerling_melee, 2, GetSpawnerName(SpawnerName::ag_A) },
						{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::ag_B) },
						{ SpawnLOTS::spiderling, 1, GetSpawnerName(SpawnerName::ag_C) },
						{ SpawnLOTS::stromling, 2, GetSpawnerName(SpawnerName::concert_A) },
						{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::concert_B) },
						{ SpawnLOTS::stromling, 2, GetSpawnerName(SpawnerName::gf_A) },
						{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::gf_B) },
					}
				},

							// Wave 7
							Wave {
								std::vector<MobDefinition> {
									{ SpawnLOTS::stromling_boss, 1, GetSpawnerName(SpawnerName::Boss) },
								},
								{1885},
								{},
								"Stromling_Boss",
								5.0f
							},

					// Wave 8
					Wave {
						std::vector<MobDefinition> {
							{SpawnLOTS::mushroom, 6, GetSpawnerName(SpawnerName::Reward_01) },
							{SpawnLOTS::mushroom, 3, GetSpawnerName(SpawnerName::interior_Reward) },
						}, {}, {}, "", -1.0f,
						25,
					},

										// Wave 9
										Wave {
											std::vector<MobDefinition> {
												{ SpawnLOTS::pirate, 4, GetSpawnerName(SpawnerName::interior_A) },
												{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::concert_A) },
												{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::gf_A) },
												{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::gf_B) },
											}
										},

								// Wave 10
								Wave {
									std::vector<MobDefinition> {
										{ SpawnLOTS::pirate, 4, GetSpawnerName(SpawnerName::interior_A) },
										{ SpawnLOTS::mech, 2, GetSpawnerName(SpawnerName::interior_B) },
										{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::ag_A) },
										{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::ag_B) },
										{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::concert_A) },
										{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::concert_B) },
										{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::gf_A) },
										{ SpawnLOTS::admiral, 2, GetSpawnerName(SpawnerName::gf_B) },
									}
								},

											// Wave 11
											Wave {
												std::vector<MobDefinition> {
													{ SpawnLOTS::pirate, 4, GetSpawnerName(SpawnerName::interior_A) },
													{ SpawnLOTS::spiderling, 2, GetSpawnerName(SpawnerName::interior_C) },
													{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::ag_A) },
													{ SpawnLOTS::spiderling, 1, GetSpawnerName(SpawnerName::ag_C) },
													{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::concert_A) },
													{ SpawnLOTS::spiderling, 1, GetSpawnerName(SpawnerName::concert_C) },
													{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::gf_A) },
													{ SpawnLOTS::spiderling, 1, GetSpawnerName(SpawnerName::gf_C) },
												}
											},

									// Wave 12
									Wave {
										std::vector<MobDefinition> {
											{ SpawnLOTS::pirate, 4, GetSpawnerName(SpawnerName::interior_A) },
											{ SpawnLOTS::hammerling, 2, GetSpawnerName(SpawnerName::interior_B) },
											{ SpawnLOTS::spiderling, 2, GetSpawnerName(SpawnerName::interior_C) },
											{ SpawnLOTS::mech, 2, GetSpawnerName(SpawnerName::ag_B) },
											{ SpawnLOTS::spiderling, 1, GetSpawnerName(SpawnerName::ag_C) },
											{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::concert_A) },
											{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::concert_C) },
											{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::gf_A) },
											{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::gf_C) },
										}
									},

												// Wave 13
												Wave {
													std::vector<MobDefinition> {
														{ SpawnLOTS::pirate, 3, GetSpawnerName(SpawnerName::interior_A) },
														{ SpawnLOTS::admiral, 2, GetSpawnerName(SpawnerName::interior_B) },
														{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::ag_A) },
														{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::ag_B) },
														{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::concert_A) },
														{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::concert_B) },
														{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::gf_A) },
														{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::gf_B) },
													}
												},

										// Wave 14
										Wave {
											std::vector<MobDefinition> {
												{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::interior_A) },
												{ SpawnLOTS::admiral, 2, GetSpawnerName(SpawnerName::interior_B) },
												{ SpawnLOTS::mech, 2, GetSpawnerName(SpawnerName::interior_C) },
												{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::ag_A) },
												{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::ag_B) },
												{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::ag_C) },
												{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::concert_A) },
												{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::concert_B) },
												{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::concert_C) },
												{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::gf_A) },
												{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::gf_B) },
												{ SpawnLOTS::mech, 1, GetSpawnerName(SpawnerName::gf_C) },
											}
										},

													// Wave 15
													Wave {
														std::vector<MobDefinition> {
															{ SpawnLOTS::ape_boss, 1, GetSpawnerName(SpawnerName::Ape_Boss) },

														},
														{1886},
														{},
														"Gorilla_Boss",
														5.0f
													},

											// Wave 16
											Wave {
												std::vector<MobDefinition> {
													{SpawnLOTS::outhouse, 3, GetSpawnerName(SpawnerName::interior_Reward) },
													{SpawnLOTS::mushroom, 6, GetSpawnerName(SpawnerName::Reward_01) },
												}, {}, {}, "", -1.0f,
												25,
											},

																// Wave 17
																Wave {
																	std::vector<MobDefinition> {
																		{ SpawnLOTS::hammerling_melee, 2, GetSpawnerName(SpawnerName::interior_A) },
																		{ SpawnLOTS::hammerling_melee, 2, GetSpawnerName(SpawnerName::interior_B) },
																		{ SpawnLOTS::hammerling_melee, 1, GetSpawnerName(SpawnerName::ag_A) },
																		{ SpawnLOTS::hammerling_melee, 2, GetSpawnerName(SpawnerName::ag_B) },
																		{ SpawnLOTS::hammerling_melee, 1, GetSpawnerName(SpawnerName::concert_A) },
																		{ SpawnLOTS::hammerling_melee, 2, GetSpawnerName(SpawnerName::concert_B) },
																		{ SpawnLOTS::hammerling_melee, 1, GetSpawnerName(SpawnerName::gf_A) },
																		{ SpawnLOTS::hammerling_melee, 2, GetSpawnerName(SpawnerName::gf_B) },
																	}
																},

														// Wave 18
														Wave {
															std::vector<MobDefinition> {
																{ SpawnLOTS::hammerling_melee, 4, GetSpawnerName(SpawnerName::interior_A) },
																{ SpawnLOTS::hammerling, 2, GetSpawnerName(SpawnerName::interior_B) },
																{ SpawnLOTS::hammerling_melee, 2, GetSpawnerName(SpawnerName::concert_A) },
																{ SpawnLOTS::hammerling, 1, GetSpawnerName(SpawnerName::concert_B) },
																{ SpawnLOTS::hammerling_melee, 2, GetSpawnerName(SpawnerName::ag_A) },
																{ SpawnLOTS::hammerling, 1, GetSpawnerName(SpawnerName::ag_B) },
																{ SpawnLOTS::hammerling_melee, 2, GetSpawnerName(SpawnerName::gf_A) },
																{ SpawnLOTS::hammerling, 1, GetSpawnerName(SpawnerName::gf_B) },
															}
														},

																	// Wave 19
																	Wave {
																		std::vector<MobDefinition> {
																			{ SpawnLOTS::hammerling, 4, GetSpawnerName(SpawnerName::interior_A) },
																			{ SpawnLOTS::sentry, 2, GetSpawnerName(SpawnerName::interior_B) },
																			{ SpawnLOTS::hammerling, 2, GetSpawnerName(SpawnerName::ag_A) },
																			{ SpawnLOTS::sentry, 1, GetSpawnerName(SpawnerName::ag_B) },
																			{ SpawnLOTS::hammerling, 2, GetSpawnerName(SpawnerName::concert_A) },
																			{ SpawnLOTS::sentry, 1, GetSpawnerName(SpawnerName::concert_B) },
																			{ SpawnLOTS::hammerling, 2, GetSpawnerName(SpawnerName::gf_A) },
																			{ SpawnLOTS::sentry, 1, GetSpawnerName(SpawnerName::gf_B) },
																		}
																	},

															// Wave 20
															Wave {
																std::vector<MobDefinition> {
																	{ SpawnLOTS::ronin, 3, GetSpawnerName(SpawnerName::interior_A) },
																	{ SpawnLOTS::sentry, 2, GetSpawnerName(SpawnerName::interior_B) },
																	{ SpawnLOTS::spiderling_ve, 1, GetSpawnerName(SpawnerName::interior_C) },
																	{ SpawnLOTS::hammerling, 1, GetSpawnerName(SpawnerName::ag_A) },
																	{ SpawnLOTS::sentry, 1, GetSpawnerName(SpawnerName::ag_B) },
																	{ SpawnLOTS::spiderling_ve, 1, GetSpawnerName(SpawnerName::ag_C) },
																	{ SpawnLOTS::hammerling, 1, GetSpawnerName(SpawnerName::concert_A) },
																	{ SpawnLOTS::sentry, 1, GetSpawnerName(SpawnerName::concert_B) },
																	{ SpawnLOTS::spiderling_ve, 1, GetSpawnerName(SpawnerName::concert_C) },
																	{ SpawnLOTS::hammerling, 1, GetSpawnerName(SpawnerName::gf_A) },
																	{ SpawnLOTS::sentry, 1, GetSpawnerName(SpawnerName::gf_B) },
																	{ SpawnLOTS::spiderling_ve, 1, GetSpawnerName(SpawnerName::gf_C) },
																}
															},

																		// Wave 21
																		Wave {
																			std::vector<MobDefinition> {
																				{ SpawnLOTS::admiral, 2, GetSpawnerName(SpawnerName::interior_A) },
																				{ SpawnLOTS::ronin, 2, GetSpawnerName(SpawnerName::interior_B) },
																				{ SpawnLOTS::spiderling_ve, 2, GetSpawnerName(SpawnerName::interior_C) },
																				{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::ag_A) },
																				{ SpawnLOTS::ronin, 1, GetSpawnerName(SpawnerName::ag_B) },
																				{ SpawnLOTS::spiderling_ve, 1, GetSpawnerName(SpawnerName::ag_C) },
																				{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::concert_A) },
																				{ SpawnLOTS::ronin, 1, GetSpawnerName(SpawnerName::concert_B) },
																				{ SpawnLOTS::spiderling_ve, 1, GetSpawnerName(SpawnerName::concert_C) },
																				{ SpawnLOTS::admiral, 1, GetSpawnerName(SpawnerName::gf_A) },
																				{ SpawnLOTS::ronin, 1, GetSpawnerName(SpawnerName::gf_B) },
																				{ SpawnLOTS::spiderling_ve, 1, GetSpawnerName(SpawnerName::gf_C) },
																			}
																		},

																// Wave 22
																Wave {
																	std::vector<MobDefinition> {
																		{ SpawnLOTS::spiderling_boss, 1, GetSpawnerName(SpawnerName::Cavalry_Boss) },
																	},
																	{1887},
																	{},
																	"Spiderling_Boss",
																	5.0f
																},

																			// Wave 23
																			Wave {
																				std::vector<MobDefinition> {
																					{ SpawnLOTS::outhouse, 6, GetSpawnerName(SpawnerName::Reward_01) },
																					{ SpawnLOTS::outhouse, 3, GetSpawnerName(SpawnerName::interior_Reward) },
																					{ SpawnLOTS::maelstrom_chest, 4, GetSpawnerName(SpawnerName::Obstacle) },
																				}, {}, {}, "", -1.0f,
																				25,
																			},

																			// Wave 24
																			Wave {
																				std::vector<MobDefinition> {
																					{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::gf_A) },
																					{ SpawnLOTS::pirate, 2, GetSpawnerName(SpawnerName::concert_A) },
																					{ SpawnLOTS::pirate, 3, GetSpawnerName(SpawnerName::ag_A) },
																					{ SpawnLOTS::ronin, 3, GetSpawnerName(SpawnerName::interior_A) },
																					{ SpawnLOTS::ronin, 2, GetSpawnerName(SpawnerName::interior_B) },
																				}
																			},

																						// Wave 25
																						Wave {
																							std::vector<MobDefinition> {
																								{ SpawnLOTS::cavalry, 2, GetSpawnerName(SpawnerName::interior_A) },
																								{ SpawnLOTS::cavalry, 1, GetSpawnerName(SpawnerName::interior_B) },
																								{ SpawnLOTS::admiral_cp, 1, GetSpawnerName(SpawnerName::ag_B) },
																								{ SpawnLOTS::admiral_cp, 1, GetSpawnerName(SpawnerName::gf_B) },
																								{ SpawnLOTS::admiral_cp, 1, GetSpawnerName(SpawnerName::concert_B) },
																								{ SpawnLOTS::spiderling, 2, GetSpawnerName(SpawnerName::gf_A) },
																								{ SpawnLOTS::spiderling, 2, GetSpawnerName(SpawnerName::concert_A) },
																								{ SpawnLOTS::spiderling, 1, GetSpawnerName(SpawnerName::ag_A) },
																							}
																						},

																				// Wave 26
																				Wave {
																					std::vector<MobDefinition> {
																						{ SpawnLOTS::ronin, 3, GetSpawnerName(SpawnerName::interior_A) },
																						{ SpawnLOTS::ronin, 3, GetSpawnerName(SpawnerName::interior_B) },
																						{ SpawnLOTS::spiderling_ve, 1, GetSpawnerName(SpawnerName::ag_B) },
																						{ SpawnLOTS::spiderling_ve, 1, GetSpawnerName(SpawnerName::gf_B) },
																						{ SpawnLOTS::spiderling_ve, 1, GetSpawnerName(SpawnerName::concert_B) },
																						{ SpawnLOTS::admiral_cp, 2, GetSpawnerName(SpawnerName::gf_C) },
																						{ SpawnLOTS::admiral_cp, 2, GetSpawnerName(SpawnerName::ag_C) },
																						{ SpawnLOTS::admiral_cp, 1, GetSpawnerName(SpawnerName::concert_C) },
																					}
																				},

																							// Wave 27
																							Wave {
																								std::vector<MobDefinition> {
																									{ SpawnLOTS::ronin, 5, GetSpawnerName(SpawnerName::interior_A) },
																									{ SpawnLOTS::ronin, 4, GetSpawnerName(SpawnerName::interior_B) },
																									{ SpawnLOTS::cavalry, 1, GetSpawnerName(SpawnerName::ag_C) },
																									{ SpawnLOTS::cavalry, 1, GetSpawnerName(SpawnerName::gf_C) },
																									{ SpawnLOTS::cavalry, 1, GetSpawnerName(SpawnerName::concert_C) },
																									{ SpawnLOTS::admiral_cp, 1, GetSpawnerName(SpawnerName::ag_B) },
																									{ SpawnLOTS::admiral_cp, 1, GetSpawnerName(SpawnerName::gf_B) },
																									{ SpawnLOTS::admiral_cp, 1, GetSpawnerName(SpawnerName::concert_B) },
																								}
																							},

																					// Wave 28
																					Wave {
																						std::vector<MobDefinition> {
																							{ SpawnLOTS::dragon_statue, 12, GetSpawnerName(SpawnerName::Reward_01) },
																						}, {}, {}, "", -1.0f,
																						30,
																					},

																								// Wave 29
																								Wave {
																									std::vector<MobDefinition> {
																										{ SpawnLOTS::horseman_boss01, 1, GetSpawnerName(SpawnerName::Horseman_01) },
																										{ SpawnLOTS::horseman_boss02, 1, GetSpawnerName(SpawnerName::Horseman_02) },
																										{ SpawnLOTS::horseman_boss03, 1, GetSpawnerName(SpawnerName::Horseman_03) },
																										{ SpawnLOTS::horseman_boss04, 1, GetSpawnerName(SpawnerName::Horseman_04) },
																									},
																									{1888},
																									{1236, 1237, 1249},
																									"Horsemen_Boss",
																									5.0f
																								},

																								// Wave 30 (treasure)
																								Wave {
																									std::vector<MobDefinition> {
																										{ SpawnLOTS::treasure_chest, 1, GetSpawnerName(SpawnerName::Treasure_01) },
																									}, {}, {},
																									"Treasure_Camera",
																									5.0f,
																									(uint32_t)-1,
																									true,
																									30,
																								},
	};
}

std::string ZoneNsWaves::GetSpawnerName(SpawnerName spawnerName) {
	switch (spawnerName) {
	case interior_A:
		return "Base_MobA";
	case interior_B:
		return "Base_MobB";
	case interior_C:
		return "Base_MobC";
	case gf_A:
		return "MobA_01";
	case gf_B:
		return "MobB_01";
	case gf_C:
		return "MobC_01";
	case concert_A:
		return "MobA_02";
	case concert_B:
		return "MobB_02";
	case concert_C:
		return "MobC_02";
	case ag_A:
		return "MobA_03";
	case ag_B:
		return "MobB_03";
	case ag_C:
		return "MobC_03";
	case Reward_01:
		return "Reward_01";
	case interior_Reward:
		return "Base_Reward";
	case Obstacle:
		return "Obstacle_01";
	case Boss:
		return "Boss";
	case Ape_Boss:
		return "Ape_Boss";
	case Geyser:
		return "Geyser_01";
	case Treasure_01:
		return "Treasure_01";
	case Cavalry_Boss:
		return "Cavalry_Boss";
	case Horseman_01:
		return "Horseman_01";
	case Horseman_02:
		return "Horseman_02";
	case Horseman_03:
		return "Horseman_03";
	case Horseman_04:
		return "Horseman_04";
	default:
		return "";
	}
}
