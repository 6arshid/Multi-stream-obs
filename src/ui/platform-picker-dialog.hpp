/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

namespace ums {

/* Grid of platform cards with capability badges; returns the selected
 * platform id (empty when cancelled). */
class PlatformPickerDialog : public QDialog {
	Q_OBJECT
public:
	explicit PlatformPickerDialog(QWidget *parent = nullptr);

	QString selectedPlatformId() const { return selectedId_; }

private:
	QString selectedId_;
};

} // namespace ums
