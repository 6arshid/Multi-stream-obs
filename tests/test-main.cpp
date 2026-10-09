/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/adapter-registry.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
int runCapabilities(int, char **);
int runRedact(int, char **);
int runBackoff(int, char **);
int runConfig(int, char **);
int runOAuth(int, char **);
int runRefresh(int, char **);
int runApiErrors(int, char **);
int runIngest(int, char **);
int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	if (app.arguments().contains("--support-matrix")) {
		auto &registry = ums::AdapterRegistry::instance();
		registry.ensureBuiltins();
		QTextStream out(stdout);
		out << "| Platform | OAuth | Key retrieval | Manual ingest | RTMP | Broadcast "
		       "creation | Metadata |\n|---|---|---|---|---|---|---|\n";
		for (const auto *adapter : registry.all()) {
			const auto info = adapter->info();
			out << "| " << info.displayName;
			for (auto feature :
			     {ums::Feature::OAuth, ums::Feature::StreamKeyRetrieval,
			      ums::Feature::ManualIngest, ums::Feature::RtmpIngest,
			      ums::Feature::BroadcastCreate, ums::Feature::MetadataManagement})
				out << " | " << ums::featureStatusText(info.caps.status(feature));
			out << " |\n";
		}
		return 0;
	}
	int result = 0;
	QTemporaryDir logs;
	for (auto run : {runCapabilities, runRedact, runBackoff, runConfig, runOAuth, runRefresh,
			 runApiErrors, runIngest}) {
		QByteArray output = (logs.filePath("suite.txt") + ",txt").toLocal8Bit();
		char option[] = "-o";
		char *args[] = {argv[0], option, output.data(), nullptr};
		result |= run(3, args);
		QFile log(logs.filePath("suite.txt"));
		if (log.open(QIODevice::ReadOnly))
			QTextStream(stdout) << log.readAll();
	}
	return result;
}
