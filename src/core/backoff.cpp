/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/backoff.hpp"

#include <algorithm>
#include <cmath>

namespace ums {

Backoff::Backoff(int baseSeconds, int maxSeconds, double factor, double jitter)
    : baseSeconds_(baseSeconds), maxSeconds_(maxSeconds), factor_(factor), jitter_(jitter)
{
}

int Backoff::delaySeconds(int attempt, double randomUnit) const
{
	if (attempt < 1)
		attempt = 1;
	if (attempt > 32)
		attempt = 32;

	const double raw = static_cast<double>(baseSeconds_) * std::pow(factor_, attempt - 1);
	double capped = std::min(raw, static_cast<double>(maxSeconds_));

	/* jitter in [-jitter_, +jitter_] */
	const double unit = std::min(std::max(randomUnit, 0.0), 1.0);
	const double offset = (unit * 2.0 - 1.0) * jitter_;
	capped *= (1.0 + offset);

	return std::max(1, std::min(maxSeconds_, static_cast<int>(std::lround(capped))));
}

int Backoff::next(double randomUnit)
{
	++attempt_;
	return delaySeconds(attempt_, randomUnit);
}

} // namespace ums
