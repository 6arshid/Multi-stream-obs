/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "destination-panel.hpp"
#include "core/adapter-registry.hpp"
#include "core/stream-controller.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>

namespace ums
{
DestinationPanel::DestinationPanel(const QString &destinationId, QWidget *parent)
    : QDialog(parent), destinationId_(destinationId)
{
	auto &controller = StreamController::instance();
	const auto *original = controller.destination(destinationId);
	if (!original)
		return;
	const DestinationConfig saved = *original;
	const auto *adapter = AdapterRegistry::instance().get(saved.platform);
	const auto info = adapter->info();
	setWindowTitle(tr("Edit destination"));
	auto *form = new QFormLayout(this);
	auto edit = [this, form](const QString &title, const QString &value) {
		auto *field = new QLineEdit(value, this);
		form->addRow(title, field);
		return field;
	};
	auto *label = edit(tr("Label"), saved.label);
	auto *mode = new QComboBox(this);
	mode->addItem(tr("Manual"), int(AuthMode::Manual));
	if (adapter->oauthSpec().isValid())
		mode->addItem(tr("API"), int(AuthMode::Api));
	mode->setCurrentIndex(mode->findData(int(saved.authMode)));
	form->addRow(tr("Authentication"), mode);
	auto *server = edit(tr("Server URL"), saved.server);
	server->setPlaceholderText(info.defaultIngestUrl);
	server->setToolTip(info.ingestHint);
	server->setLayoutDirection(Qt::LeftToRight);
	auto *key = edit(tr("Stream key"), {});
	key->setEchoMode(QLineEdit::Password);
	key->setLayoutDirection(Qt::LeftToRight);
	key->setPlaceholderText(controller.hasStreamKey(destinationId)
				    ? tr("Stored in vault; leave blank to keep")
				    : tr("Enter stream key"));
	auto *clear = new QCheckBox(tr("Clear stored stream key"), this);
	form->addRow(clear);
	auto *account = new QComboBox(this);
	auto refreshAccounts = [account, platform = saved.platform, alias = saved.accountAlias] {
		auto &c = StreamController::instance();
		QString selected = account->currentData().toString();
		if (selected.isEmpty())
			selected = alias;
		account->clear();
		auto entries = c.config().accounts.value(platform).value("accounts").toArray();
		for (const auto &entry : entries) {
			auto item = entry.toObject();
			account->addItem(item.value("name").toString(),
					 item.value("alias").toString());
		}
		if (account->count() == 0)
			account->addItem(
			    c.isAccountConnected(platform) ? c.accountDisplayName(platform)
							   : tr("Not connected"),
			    c.isAccountConnected(platform) ? QString("me") : QString());
		int index = account->findData(selected);
		if (index >= 0)
			account->setCurrentIndex(index);
	};
	refreshAccounts();
	form->addRow(tr("Account"), account);
	auto *connectButton = new QPushButton(tr("Connect account"), this);
	form->addRow(connectButton);
	connect(connectButton, &QPushButton::clicked, this, [platform = saved.platform] {
		StreamController::instance().connectAccount(platform);
	});
	auto *disconnectButton = new QPushButton(tr("Disconnect account"), this);
	form->addRow(disconnectButton);
	connect(disconnectButton, &QPushButton::clicked, this, [platform = saved.platform] {
		StreamController::instance().disconnectAccount(platform);
	});
	connect(
	    &controller, &StreamController::accountStateChanged, this,
	    [refreshAccounts, platform = saved.platform](const QString &id, bool, const QString &) {
		    if (id == platform)
			    refreshAccounts();
	    });
	auto *token = new QPushButton(tr("Paste access token"), this);
	form->addRow(token);
	connect(token, &QPushButton::clicked, this, [this, platform = saved.platform] {
		QDialog dialog(this);
		auto *layout = new QFormLayout(&dialog);
		auto *field = new QLineEdit(&dialog);
		field->setEchoMode(QLineEdit::Password);
		layout->addRow(tr("Access token"), field);
		auto *buttons =
		    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
		layout->addRow(buttons);
		connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		if (dialog.exec() == QDialog::Accepted && !field->text().trimmed().isEmpty())
			StreamController::instance().storeAccessToken(platform,
								      field->text().trimmed());
	});
	auto *title = edit(tr("Title"), saved.options.value("title").toString());
	auto *privacy = new QComboBox(this);
	privacy->addItems({"private", "unlisted", "public"});
	privacy->setCurrentText(saved.options.value("privacy").toString("private"));
	form->addRow(tr("Privacy"), privacy);
	auto *options =
	    edit(tr("Platform options (JSON)"),
		 QString::fromUtf8(QJsonDocument(saved.options).toJson(QJsonDocument::Compact)));
	auto spin = [this, form](const QString &title, int value, int maximum) {
		auto *field = new QSpinBox(this);
		field->setRange(0, maximum);
		field->setValue(value);
		form->addRow(title, field);
		return field;
	};
	auto *video = spin(tr("Video bitrate (kbps; 0 inherits)"), saved.videoBitrateKbps, 100000);
	auto *audio = spin(tr("Audio bitrate (kbps; 0 inherits)"), saved.audioBitrateKbps, 1000);
	auto *interval = spin(tr("Keyframe interval (seconds)"), saved.keyframeIntervalSec, 20);
	auto *encoder = edit(tr("Encoder ID (blank inherits)"), saved.encoderId);
	auto refreshMode = [=] {
		bool manual = mode->currentData().toInt() == int(AuthMode::Manual);
		server->setEnabled(manual);
		key->setEnabled(manual);
		clear->setEnabled(manual);
		account->setEnabled(!manual);
		connectButton->setEnabled(!manual);
		disconnectButton->setEnabled(!manual);
		token->setEnabled(!manual);
	};
	connect(mode, &QComboBox::currentIndexChanged, this, refreshMode);
	refreshMode();
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(buttons, &QDialogButtonBox::accepted, this, [=] {
		auto updated = saved;
		updated.label = label->text().trimmed();
		updated.authMode = AuthMode(mode->currentData().toInt());
		updated.server = server->text().trimmed();
		updated.accountAlias = account->currentData().toString();
		QJsonParseError parse;
		auto json = QJsonDocument::fromJson(options->text().toUtf8(), &parse);
		if (parse.error != QJsonParseError::NoError || !json.isObject()) {
			QMessageBox::warning(this, tr("Invalid destination"),
					     tr("Platform options must be a JSON object"));
			return;
		}
		updated.options = json.object();
		updated.options.insert("title", title->text());
		updated.options.insert("privacy", privacy->currentText());
		updated.videoBitrateKbps = video->value();
		updated.audioBitrateKbps = audio->value();
		updated.keyframeIntervalSec = interval->value();
		updated.encoderId = encoder->text().trimmed();
		QStringList errors = adapter->validate(updated);
		if (!errors.isEmpty()) {
			QMessageBox::warning(this, tr("Invalid destination"), errors.join("\n"));
			return;
		}
		if (!key->text().isEmpty() &&
		    !StreamController::instance().setStreamKey(destinationId_, key->text())) {
			QMessageBox::warning(this, tr("Credential vault"),
					     tr("Could not store stream key"));
			return;
		}
		if (clear->isChecked() && key->text().isEmpty())
			StreamController::instance().clearStreamKey(destinationId_);
		updated.hasKey = StreamController::instance().hasStreamKey(destinationId_);
		if (!StreamController::instance().updateDestination(updated, &errors)) {
			QMessageBox::warning(this, tr("Invalid destination"), errors.join("\n"));
			return;
		}
		applied_ = true;
		accept();
	});
}
} // namespace ums
