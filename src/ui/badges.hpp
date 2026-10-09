/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QColor>
#include <QIcon>
#include <QSize>
#include <QString>

namespace ums {

/* Trademark-free monogram badge colors per platform. */
QColor platformColor(const QString &platformId);
/* Rounded-rect badge with the platform's first letter. */
QIcon platformIcon(const QString &platformId, const QSize &size);

} // namespace ums
