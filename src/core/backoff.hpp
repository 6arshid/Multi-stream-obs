/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace ums {

/* Deterministic exponential backoff with a random jitter factor.
 * Used for API-side retries (rate limits, transient failures). The RTMP
 * layer uses libobs' own per-output reconnect backoff instead. */
class Backoff {
public:
	Backoff(int baseSeconds = 2, int maxSeconds = 600, double factor = 2.0, double jitter = 0.2);

	/* Seconds to wait before attempt `attempt` (1-based). */
	int delaySeconds(int attempt, double randomUnit) const;

	void reset() { attempt_ = 0; }
	int attempt() const { return attempt_; }

	/* Advances the counter and returns the delay for the next wait. */
	int next(double randomUnit);

private:
	int baseSeconds_;
	int maxSeconds_;
	double factor_;
	double jitter_;
	int attempt_ = 0;
};

} // namespace ums
