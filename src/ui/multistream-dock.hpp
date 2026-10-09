/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

namespace ums {

/* The main dock widget: destination list, start/stop controls, account
 * status and a log footer. Registered with the OBS frontend. */
class MultistreamDock : public QWidget {
	Q_OBJECT
public:
	explicit MultistreamDock(QWidget *parent = nullptr);

	/* Creates the dock and registers it with OBS (UI thread). */
	static void createRegistered();
	/* Removes the dock (idempotent). */
	static void removeRegistered();
};

} // namespace ums
