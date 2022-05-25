BEGIN TRANSACTION;

UPDATE ComponentsRegistry SET component_id = 1901 WHERE id = 12916 AND component_type = 39;
INSERT INTO ActivityRewards (objectTemplate, ActivityRewardIndex, activityRating, LootMatrixIndex, CurrencyIndex, ChallengeRating, description) VALUES (1901, 166, -1, 598, 1, 4, 'NT Foot Race');

COMMIT;
