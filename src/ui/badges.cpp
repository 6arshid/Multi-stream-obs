/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "badges.hpp"

#include <QFont>
#include <QHash>
#include <QPainter>
#include <QPixmap>

namespace ums {

QColor platformColor(const QString &platformId)
{
	static const QHash<QString, QColor> colors = {
	    {QStringLiteral("youtube"), QColor(200, 16, 46)},
	    {QStringLiteral("twitch"), QColor(145, 71, 255)},
	    {QStringLiteral("kick"), QColor(83, 252, 24)},
	    {QStringLiteral("facebook"), QColor(24, 119, 242)},
	    {QStringLiteral("vimeo"), QColor(26, 163, 224)},
	    {QStringLiteral("restream"), QColor(0, 153, 153)},
	    {QStringLiteral("trovo"), QColor(108, 92, 231)},
	    {QStringLiteral("tiktok"), QColor(22, 24, 35)},
	    {QStringLiteral("instagram"), QColor(225, 48, 108)},
	    {QStringLiteral("x"), QColor(29, 161, 242)},
	    {QStringLiteral("linkedin"), QColor(10, 102, 194)},
	    {QStringLiteral("rumble"), QColor(255, 28, 28)},
	    {QStringLiteral("dlive"), QColor(20, 191, 149)},
	    {QStringLiteral("bigo"), QColor(0, 174, 239)},
	    {QStringLiteral("discord"), QColor(88, 101, 242)},
	    {QStringLiteral("streamyard"), QColor(103, 58, 183)},
	};
	const auto it = colors.constFind(platformId);
	return it != colors.constEnd() ? it.value() : QColor(96, 103, 113);
}

QIcon platformIcon(const QString &platformId, const QSize &size)
{
	QPixmap pixmap(size);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(platformColor(platformId));
	painter.drawRoundedRect(QRect(QPoint(0, 0), size), size.width() / 4.0,
	                        size.height() / 4.0);

	QFont font;
	font.setBold(true);
	font.setPixelSize(size.height() * 6 / 10);
	painter.setFont(font);
	painter.setPen(Qt::white);
	const QString letter =
	    platformId.isEmpty() ? QStringLiteral("?") : platformId.left(1).toUpper();
	painter.drawText(QRect(QPoint(0, 0), size), Qt::AlignCenter, letter);
	painter.end();

	return QIcon(pixmap);
}

} // namespace ums
