/*###############################################################################
#
# Copyright 2024 NVIDIA Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
###############################################################################*/

#include "pqOmniConnectLauncherClient.h"
#include "pqOmniConnectLogger.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

static constexpr const char* APP_INFO_URL = "http://localhost:33480/components";
static constexpr const char* SLUG_KEY = "slug";
static constexpr const char* CURRENT_KEY = "current";
static constexpr const char* LATEST_KEY = "latest";
static constexpr const char* INSTALLED_VERSION_KEY = "installedVersions";
static constexpr const char* SETTINGS_KEY = "settings";
static constexpr const char* VERSION_KEY = "version";
static constexpr const char* LAUNCH_KEY = "launch";
static constexpr const char* PATH_KEY = "path";

pqOmniConnectLauncherClient::pqOmniConnectLauncherClient() {
	m_manager = new QNetworkAccessManager();
	connect(m_manager, &QNetworkAccessManager::finished, this, &pqOmniConnectLauncherClient::replyFinished);
}

pqOmniConnectLauncherClient::~pqOmniConnectLauncherClient() {
	delete m_manager;
}

void pqOmniConnectLauncherClient::requestInfo() {
	QUrl url(APP_INFO_URL);
	QNetworkRequest request(url);
	m_manager->get(request);
}

bool pqOmniConnectLauncherClient::getAppInfo(const QString& slugName, pqOmniConnectAppInfo& infoOut) {
	if (m_appsInfo.contains(slugName)) {
		infoOut.current = m_appsInfo[slugName].current;
		infoOut.latest = m_appsInfo[slugName].latest;
		infoOut.name = m_appsInfo[slugName].name;
		infoOut.versions = m_appsInfo[slugName].versions;
		infoOut.launchPaths = m_appsInfo[slugName].launchPaths;
		return true;
	}
	return false;
}

bool pqOmniConnectLauncherClient::getAppLatestLaunchPath(const QString& slugName, QString& outLaunchPath) {
	if (m_appsInfo.contains(slugName)) {
		const QStringList& versions = m_appsInfo[slugName].versions;
		const QStringList& paths = m_appsInfo[slugName].launchPaths;
		const QString& latest = m_appsInfo[slugName].latest;
		for (int i = 0; i < versions.size(); i++) {
			if (versions[i].compare(latest) == 0) {
				outLaunchPath = paths[i];
				return true;
			}
		}
	}
	return false;
}

bool pqOmniConnectLauncherClient::getAppVersionLaunchPath(const QString& slugName, const QString& version, QString& outLaunchPath) {
	if (m_appsInfo.contains(slugName)) {
		const QStringList& versions = m_appsInfo[slugName].versions;
		const QStringList& paths = m_appsInfo[slugName].launchPaths;
		for (int i = 0; i < versions.size(); i++) {
			if (versions[i].compare(version) == 0) {
				outLaunchPath = paths[i];
				return true;
			}
		}
	}
	return false;
}

void pqOmniConnectLauncherClient::replyFinished(QNetworkReply* reply) {
	if (reply->error()) {
		OMNI_LOG_WARNING("Failed to find local Omniverse installations. Code %d", static_cast<int>(reply->error()));
		Q_EMIT replyProcessed(false);
		return;
	}

	m_appsInfo.clear();

	QJsonDocument jsonResponse = QJsonDocument::fromJson(reply->readAll());
	QJsonArray appArray = jsonResponse.array();

	for(auto value : appArray) {
		QJsonObject obj = value.toObject();
		QString slug = obj[SLUG_KEY].toString();

		QJsonObject installedVersionsObj = obj[INSTALLED_VERSION_KEY].toObject();
		pqOmniConnectAppInfo appInfo;
		appInfo.name = obj[SLUG_KEY].toString();
		appInfo.current = installedVersionsObj[CURRENT_KEY].toString();
		appInfo.latest = installedVersionsObj[LATEST_KEY].toString();

		QJsonArray settingsArray = obj[SETTINGS_KEY].toArray();
		foreach(const QJsonValue& setting, settingsArray) {
			QString version = setting[VERSION_KEY].toString();
			QString launchPath = setting[LAUNCH_KEY].toObject()[PATH_KEY].toString();
			appInfo.versions << version;
			appInfo.launchPaths << launchPath;

			OMNI_LOG_INFO("Omniverse installation found %s %s", slug.toStdString().c_str(), version.toStdString().c_str());
		}
		m_appsInfo[slug] = appInfo;
	}
	Q_EMIT replyProcessed(true);
}