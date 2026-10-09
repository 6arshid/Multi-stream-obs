/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

namespace ums {

/* Settings dialog for one destination (mode, credentials, account, encoder
 * overrides). Returns true when the configuration was applied. */
class DestinationPanel : public QDialog {
	Q_OBJECT
public:
	DestinationPanel(const QString &destinationId, QWidget *parent = nullptr);

	bool applied() const { return applied_; }

private:
	QString destinationId_;
	bool applied_ = false;
};

} // namespace ums
