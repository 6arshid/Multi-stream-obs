/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "multistream-dock.hpp"
#include "badges.hpp"
#include "core/adapter-registry.hpp"
#include "core/stream-controller.hpp"
#include "destination-panel.hpp"
#include "platform-picker-dialog.hpp"
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QResizeEvent>
#include <QSpinBox>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <obs-frontend-api.h>
#include <obs-module.h>

namespace ums
{
namespace
{
class DestinationList : public QListWidget {
public:
	using QListWidget::QListWidget;
protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QListWidget::resizeEvent(event);
		for (int index = 0; index < count(); ++index) {
			auto *entry = item(index);
			if (auto *row = itemWidget(entry)) {
				row->setFixedWidth(viewport()->width());
				entry->setSizeHint(QSize(viewport()->width(), row->sizeHint().height()));
			}
		}
	}
};
constexpr const char *kDockId = "obs-universal-multistream";
MultistreamDock *g_dock = nullptr;
QString stateText(OutputState state)
{
	switch (state) {
	case OutputState::Idle:
		return MultistreamDock::tr("Idle");
	case OutputState::Starting:
		return MultistreamDock::tr("Starting");
	case OutputState::Active:
		return MultistreamDock::tr("Active");
	case OutputState::Reconnecting:
		return MultistreamDock::tr("Reconnecting");
	case OutputState::Stopping:
		return MultistreamDock::tr("Stopping");
	case OutputState::Stopped:
		return MultistreamDock::tr("Stopped");
	}
	return {};
}
} // namespace
MultistreamDock::MultistreamDock(QWidget *parent) : QWidget(parent)
{
	if (QString::fromUtf8(obs_get_locale()).startsWith("fa"))
		setLayoutDirection(Qt::RightToLeft);
	auto &controller = StreamController::instance();
	auto *layout = new QVBoxLayout(this);
	auto *toolbar = new QHBoxLayout;
	layout->addLayout(toolbar);
	auto button = [this, toolbar](const QString &text) {
		auto *b = new QPushButton(text, this);
		toolbar->addWidget(b);
		return b;
	};
	auto *add = button(tr("Add"));
	add->setObjectName("ums-add");
	auto *start = button(tr("Start All"));
	auto *stop = button(tr("Stop All"));
	auto *settings = button(tr("Settings"));
	auto *list = new DestinationList(this);
	list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	list->setMinimumWidth(440);
	layout->addWidget(list);
	auto *log = new QTextEdit(this);
	log->setReadOnly(true);
	log->setMaximumHeight(140);
	log->document()->setMaximumBlockCount(300);
	layout->addWidget(log);
	connect(&controller, &StreamController::notify, this,
		[log](StreamController::NotifyLevel, const QString &message) {
			log->append(message.toHtmlEscaped());
		});
	auto rebuild = [this, list] {
		list->clear();
		auto &c = StreamController::instance();
		for (const auto &destination : c.config().destinations) {
			auto *item = new QListWidgetItem(list);
			auto *row = new QWidget(list);
			auto *rowLayout = new QVBoxLayout(row);
			auto *line = new QHBoxLayout;
			auto *actions = new QHBoxLayout;
			rowLayout->addLayout(line);
			rowLayout->setSizeConstraint(QLayout::SetNoConstraint);
			auto *enabled = new QCheckBox(row);
			enabled->setChecked(destination.enabled);
			line->addWidget(enabled);
			auto *icon = new QLabel(row);
			icon->setPixmap(
			    platformIcon(destination.platform, QSize(24, 24)).pixmap(24, 24));
			line->addWidget(icon);
			auto *label = new QLabel(c.labelFor(destination), row);
			label->setWordWrap(true);
			line->addWidget(label, 1);
			auto *status = new QLabel(row);
			status->setObjectName("state-" + destination.id);
			line->addWidget(status);
			auto *stats = new QLabel(row);
			stats->setObjectName("stats-" + destination.id);
			stats->setWordWrap(true);
			rowLayout->addWidget(stats);
			rowLayout->addLayout(actions);
			actions->addStretch();
			auto *toggle = new QPushButton(tr("Start / Stop"), row);
			actions->addWidget(toggle);
			auto *edit = new QPushButton(tr("Edit"), row);
			actions->addWidget(edit);
			auto *remove = new QPushButton(tr("Remove"), row);
			actions->addWidget(remove);
			connect(enabled, &QCheckBox::toggled, this,
				[this, id = destination.id](bool value) {
					QTimer::singleShot(0, this, [id, value] {
						StreamController::instance().setDestinationEnabled(
						    id, value);
					});
				});
			connect(toggle, &QPushButton::clicked, this, [id = destination.id] {
				auto &c = StreamController::instance();
				if (c.isDestinationActive(id))
					c.stopDestination(id);
				else
					c.startDestination(id);
			});
			connect(edit, &QPushButton::clicked, this, [this, id = destination.id] {
				DestinationPanel dialog(id, this);
				dialog.exec();
			});
			connect(remove, &QPushButton::clicked, this, [this, id = destination.id] {
				if (QMessageBox::question(
					this, tr("Remove destination"),
					tr("Remove this destination and its stored stream key?")) ==
				    QMessageBox::Yes)
					StreamController::instance().removeDestination(id);
			});
			status->setText(QString(QChar(0x25cf)) + " " +
					stateText(c.destinationState(destination.id)));
			auto s = c.destinationStats(destination.id);
			stats->setText(tr("%1 kbps / %2% dropped")
					   .arg(s.kbps, 0, 'f', 0)
					   .arg(s.droppedPercent));
			row->setFixedWidth(list->viewport()->width());
			item->setSizeHint(QSize(list->viewport()->width(), row->sizeHint().height()));
			list->setItemWidget(item, row);
		}
	};
	connect(&controller, &StreamController::destinationsChanged, this,
		[this, rebuild] { QTimer::singleShot(0, this, rebuild); });
	connect(&controller, &StreamController::destinationStateChanged, this,
		[list](const QString &id, OutputState state, const QString &) {
			if (auto *label = list->findChild<QLabel *>("state-" + id)) {
				label->setText(QString(QChar(0x25cf)) + " " + stateText(state));
				label->setStyleSheet(state == OutputState::Active ? "color: #329c65"
										  : "");
			}
		});
	connect(&controller, &StreamController::destinationStatsChanged, this,
		[list](const QString &id, const OutputStats &stats) {
			if (auto *label = list->findChild<QLabel *>("stats-" + id))
				label->setText(tr("%1 kbps / %2% dropped")
						   .arg(stats.kbps, 0, 'f', 0)
						   .arg(stats.droppedPercent));
		});
	connect(start, &QPushButton::clicked, &controller, &StreamController::startAll);
	connect(stop, &QPushButton::clicked, &controller, &StreamController::stopAll);
	connect(add, &QPushButton::clicked, this, [this] {
		PlatformPickerDialog picker(this);
		if (picker.exec() != QDialog::Accepted)
			return;
		auto &c = StreamController::instance();
		QString id = c.addDestination(picker.selectedPlatformId());
		if (id.isEmpty())
			return;
		DestinationPanel panel(id, this);
		panel.exec();
		if (!panel.applied())
			c.removeDestination(id);
	});
	connect(&controller, &StreamController::oauthOpenBrowser, this, [this](const QString &url) {
		QDesktopServices::openUrl(QUrl(url));
		auto &c = StreamController::instance();
		for (const auto *adapter : AdapterRegistry::instance().all()) {
			QString id = adapter->info().id;
			if (!c.oauthUsesManualRedirect(id))
				continue;
			QUrlQuery query{QUrl(url)};
			if (query.queryItemValue("client_id") != c.platformClientId(id))
				continue;
			bool ok = false;
			QString redirect = QInputDialog::getText(
			    this, tr("Complete sign-in"),
			    tr("Paste the full redirect URL from your browser"),
			    QLineEdit::Password, {}, &ok);
			if (ok)
				c.handleManualRedirect(id, redirect);
			else
				c.cancelConnect();
			break;
		}
	});
	connect(&controller, &StreamController::oauthDevicePrompt, this,
		[this](const QString &, const QString &uri, const QString &code, int seconds) {
			auto *prompt =
			    new QMessageBox(QMessageBox::Information, tr("Device sign-in"),
					    tr("Open %1 and enter code %2. Expires in %3 seconds.")
						.arg(uri, code)
						.arg(seconds),
					    QMessageBox::Close, this);
			prompt->setAttribute(Qt::WA_DeleteOnClose);
			prompt->setTextInteractionFlags(Qt::TextSelectableByMouse);
			prompt->show();
			QDesktopServices::openUrl(QUrl(uri));
		});
	connect(settings, &QPushButton::clicked, this, [this] {
		auto &c = StreamController::instance();
		auto saved = c.streamSettings();
		QDialog dialog(this);
		dialog.setWindowTitle(tr("Settings"));
		dialog.resize(620, 640);
		auto *outer = new QVBoxLayout(&dialog);
		auto *scroll = new QScrollArea(&dialog);
		scroll->setWidgetResizable(true);
		auto *fields = new QWidget(scroll);
		auto *form = new QFormLayout(fields);
		scroll->setWidget(fields);
		outer->addWidget(scroll);
		auto check = [&dialog, form](const QString &text, bool value) {
			auto *w = new QCheckBox(text, &dialog);
			w->setChecked(value);
			form->addRow(w);
			return w;
		};
		auto *coupling = check(tr("Start with OBS streaming"), saved.startWithMain);
		auto *fatal = check(tr("Stop all on fatal error"), saved.stopOthersOnFatal);
		auto *share = check(tr("Share main encoders"), saved.shareMainEncoder);
		auto *pool = check(tr("Use shared encoder pool"), saved.useSharedEncoderPool);
		auto *videoEncoder = new QLineEdit(saved.defaultVideoEncoder, &dialog);
		auto *audioEncoder = new QLineEdit(saved.defaultAudioEncoder, &dialog);
		form->addRow(tr("Default video encoder ID"), videoEncoder);
		form->addRow(tr("Default audio encoder ID"), audioEncoder);
		auto spin = [&dialog, form](const QString &text, int value, int max) {
			auto *w = new QSpinBox(&dialog);
			w->setRange(0, max);
			w->setValue(value);
			form->addRow(text, w);
			return w;
		};
		auto *video =
		    spin(tr("Default video bitrate"), saved.defaultVideoBitrateKbps, 100000);
		auto *audio =
		    spin(tr("Default audio bitrate"), saved.defaultAudioBitrateKbps, 1000);
		auto *attempts = spin(tr("Reconnect attempts"), saved.reconnectAttempts, 100);
		auto *delay = spin(tr("Reconnect delay (seconds)"), saved.reconnectDelaySec, 600);
		struct App {
			QString id;
			QLineEdit *client;
			QLineEdit *redirect;
			QLineEdit *secret;
			QCheckBox *clear;
		};
		QVector<App> apps;
		for (const auto *adapter : AdapterRegistry::instance().all()) {
			if (!adapter->oauthSpec().isValid())
				continue;
			const auto info = adapter->info();
			auto *client = new QLineEdit(c.platformClientId(info.id), &dialog);
			auto *redirect = new QLineEdit(c.platformRedirectUri(info.id), &dialog);
			auto *secret = new QLineEdit(&dialog);
			secret->setEchoMode(QLineEdit::Password);
			secret->setPlaceholderText(tr("Leave blank to keep stored secret"));
			form->addRow(info.displayName + tr(" client ID"), client);
			form->addRow(tr("Redirect URI (blank for loopback)"), redirect);
			form->addRow(tr("Client secret"), secret);
			auto *clear = new QCheckBox(tr("Clear stored client secret"), &dialog);
			form->addRow(clear);
			apps.push_back({info.id, client, redirect, secret, clear});
		}
		auto *buttons =
		    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
		outer->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		if (dialog.exec() != QDialog::Accepted)
			return;
		saved.startWithMain = coupling->isChecked();
		saved.stopOthersOnFatal = fatal->isChecked();
		saved.shareMainEncoder = share->isChecked();
		saved.useSharedEncoderPool = pool->isChecked();
		saved.defaultVideoEncoder = videoEncoder->text().trimmed();
		saved.defaultAudioEncoder = audioEncoder->text().trimmed();
		saved.defaultVideoBitrateKbps = video->value();
		saved.defaultAudioBitrateKbps = audio->value();
		saved.reconnectAttempts = attempts->value();
		saved.reconnectDelaySec = delay->value();
		c.setStreamSettings(saved);
		for (const auto &app : apps) {
			c.setPlatformApp(app.id, app.client->text().trimmed(),
					 app.redirect->text().trimmed());
			if (!app.secret->text().isEmpty())
				c.setPlatformClientSecret(app.id, app.secret->text());
			else if (app.clear->isChecked())
				c.setPlatformClientSecret(app.id, {});
		}
	});
	rebuild();
}
void MultistreamDock::createRegistered()
{
	if (g_dock)
		return;
	g_dock = new MultistreamDock();
	if (!obs_frontend_add_dock_by_id(kDockId, tr("Universal Multi-Stream").toUtf8().constData(),
					 g_dock)) {
		delete g_dock;
		g_dock = nullptr;
	}
}
void MultistreamDock::removeRegistered()
{
	if (!g_dock)
		return;
	obs_frontend_remove_dock(kDockId);
	g_dock = nullptr;
}
} // namespace ums
