#pragma once

/**
 * Prometheus metrics at /metrics (and /api/metrics for API tokens): players and world instances, whether auth, chat and
 * master are up, memory per server, chat, today's economy, moderation queues, strikes and scheduled task results.
 *
 * Off until metrics_enabled is set. A scraper then sends either an API token of an account with metrics_view, or the
 * shared metrics_token, as Authorization: Bearer; metrics_allowed_ips can limit where requests come from. The text is
 * built at most once every metrics_cache_seconds however often it is fetched, so scrapes never add up to heavy queries.
 * The exposition format itself is in MetricsFormat.h.
 */
namespace PrometheusMetrics {
	void RegisterRoutes();
}
