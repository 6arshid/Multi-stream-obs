/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/obs-utils.hpp"
#include "core/stream-controller.hpp"
#include "ui/multistream-dock.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QTranslator>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-universal-multistream", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Universal Multi-Stream: stream to multiple RTMP/RTMPS destinations at once "
	       "from a capability-aware dock with official YouTube, Twitch, Kick, Facebook, "
	       "Restream and Trovo API integrations.";
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Universal Multi-Stream";
}

namespace {

/* Flat "English source=text" translator (see data/locale/*.ini). Installs a
 * QTranslator so every tr() call - core, controller and UI - resolves from
 * the OBS locale without needing Qt .qm files. */
class LocaleTranslator : public QTranslator {
public:
	using QTranslator::QTranslator;
	bool isEmpty() const override { return map_.isEmpty(); }

	QString translate(const char *, const char *sourceText, const char *,
	                  int) const override
	{
		if (!sourceText)
			return {};
		const QString key = QString::fromUtf8(sourceText);
		const auto it = map_.constFind(key);
		return it == map_.constEnd() ? QString() : it.value();
	}

	void setFilePath(const QString &path)
	{
		filePath_ = path;
		loadFile();
	}

private:
	void loadFile()
	{
		map_.clear();
		if (filePath_.isEmpty())
			return;
		QFile file(filePath_);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return;
		while (!file.atEnd()) {
			const QString line = QString::fromUtf8(file.readLine()).remove(QLatin1Char('\n')).remove(QLatin1Char('\r'));
			if (line.trimmed().isEmpty() || line.startsWith(QLatin1Char('#')) ||
			    line.startsWith(QLatin1Char('[')))
				continue;
			const int eq = line.indexOf(QLatin1Char('='));
			if (eq <= 0)
				continue;
			map_.insert(line.left(eq), line.mid(eq + 1));
		}
	}

	QString filePath_;
	QHash<QString, QString> map_;
};

LocaleTranslator *translator = nullptr;
bool dockCreated = false;

void installLocale()
{
	const char *rawLocale = obs_get_locale();
	if (!rawLocale)
		return;
	const QString locale = QString::fromUtf8(rawLocale);
	char *rawPath = obs_module_file(QStringLiteral("locale/%1.ini").arg(locale).toUtf8());
	const QString path = rawPath ? QString::fromUtf8(rawPath) : QString();
	bfree(rawPath);
	if (path.isEmpty() || !QFile::exists(path))
		return;

	translator = new LocaleTranslator(qApp);
	translator->setFilePath(path);
	qApp->installTranslator(translator);
}

void onFrontendEvent(enum obs_frontend_event event, void *)
{
	auto &controller = ums::StreamController::instance();
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		if (!dockCreated) {
			ums::MultistreamDock::createRegistered();
			dockCreated = true;
		}
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		controller.onMainStreamingStarted();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		controller.onMainStreamingStopped();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		ums::MultistreamDock::removeRegistered();
		controller.shutdown();
		break;
	default:
		break;
	}
}

} // namespace

bool obs_module_load(void)
{
	installLocale();

	auto &controller = ums::StreamController::instance();
	const QString configPath =
	    ums::obsutil::moduleConfigFilePath(QStringLiteral("multistream.json"));
	if (!controller.initialize(configPath)) return false;

	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	ums::MultistreamDock::removeRegistered();
	ums::StreamController::instance().shutdown();

	delete translator;
	translator = nullptr;
}
