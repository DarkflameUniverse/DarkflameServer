/* UGC server: models with no bricks are a state of their own. See the MySQL migration. */
UPDATE ugc SET is_optimized = 3, process_error = '' WHERE is_optimized = 2 AND process_error = 'the LXFML has no bricks';
