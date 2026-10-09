/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platform-picker-dialog.hpp"
#include "core/adapter-registry.hpp"
#include "ui/badges.hpp"
#include <QGridLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace ums
{
PlatformPickerDialog::PlatformPickerDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(tr("Add destination"));
	resize(650, 500);
	auto *layout = new QVBoxLayout(this);
	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	auto *cards = new QWidget(scroll);
	auto *grid = new QGridLayout(cards);
	auto &registry = AdapterRegistry::instance();
	registry.ensureBuiltins();
	int index = 0;
	for (const auto *adapter : registry.all()) {
		const auto info = adapter->info();
		QString text = info.displayName;
		for (auto feature :
		     {Feature::OAuth, Feature::StreamKeyRetrieval, Feature::ManualIngest})
			text += QStringLiteral("\n%1: %2")
				    .arg(featureName(feature),
					 featureStatusBadge(info.caps.status(feature)));
		auto *button = new QPushButton(platformIcon(info.id, QSize(32, 32)), text, cards);
		button->setMinimumHeight(100);
		button->setToolTip(info.description + QStringLiteral("\n") + info.ingestHint);
		button->setEnabled(!info.informationalOnly && info.canStream() &&
				   validatePlatformInfo(info).isEmpty());
		connect(button, &QPushButton::clicked, this, [this, id = info.id] {
			selectedId_ = id;
			accept();
		});
		grid->addWidget(button, index / 2, index % 2);
		++index;
	}
	scroll->setWidget(cards);
	layout->addWidget(scroll);
}
} // namespace ums
