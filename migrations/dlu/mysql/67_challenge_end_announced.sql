/* challenges.end_announced_at: when the dashboard told players in game that the challenge was complete (or over),
   0 while that is still to do. A challenge that ends while the dashboard can't reach the worlds (it is restarting) is
   announced once it can, instead of never. */
ALTER TABLE challenges ADD COLUMN end_announced_at BIGINT NOT NULL DEFAULT 0;
