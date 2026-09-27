/* UGC server: models with no bricks have nothing to make. They are a state of their own (is_optimized = 3, "empty"),
   not a failure: the ones marked failed only because they had no bricks move to it. */
UPDATE ugc SET is_optimized = 3, process_error = '' WHERE is_optimized = 2 AND process_error = 'the LXFML has no bricks';
