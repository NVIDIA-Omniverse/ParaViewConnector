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

#include "pqOmniConnectViewOpener.h"
#include "pqOmniConnectUtils.h"
#include "pqOmniConnectLogger.h"

#include "OmniConnect.h"

#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

static constexpr const char* OPEN_INFO_SCOPE_ID = "usd_open_info";

pqOmniConnectViewOpener::pqOmniConnectViewOpener(pqOmniConnectViewsSettingsManager& settingsManager) :
	QObject(nullptr),
	m_process(nullptr),
	m_settingsManager(settingsManager)
{
	m_viewClient = new pqOmniConnectViewClient(this);
	connect(m_viewClient, &pqOmniConnectViewClient::runningStatusChecked, this, &pqOmniConnectViewOpener::onRunningStatusChecked);
	connect(m_viewClient, &pqOmniConnectViewClient::usdStageOpened, this, &pqOmniConnectViewOpener::onUsdStageOpened);
}

pqOmniConnectViewOpener::~pqOmniConnectViewOpener()
{
	if (m_process) {
		delete m_process;
		m_process = nullptr;
	}
}

void pqOmniConnectViewOpener::loadConfig()
{
	m_config = new QSettings(pqOmniConnectUtils::StandardPaths::settingsFilePath(), QSettings::IniFormat, this);

	m_config->beginGroup("sendToViewRadioButtonGroup");
	m_unset = m_config->value("checked").toInt() == 0 ? true : false;
	m_config->endGroup();

	m_config->beginGroup("omniViewInfo");
	QString version = m_config->value("version").toString();
	m_launchPath = m_config->value("path").toString();
	m_port = m_config->value("port").toInt();
	m_config->endGroup();
}

bool pqOmniConnectViewOpener::isUnset() const
{
	return m_unset;
}

void pqOmniConnectViewOpener::openUSD()
{
	QString activeView = m_settingsManager.getActiveViewName();
	if (activeView.isNull()) {
		QMessageBox::warning(nullptr, tr("Open View Failed"), tr("Please select an Omniverse Connector view to open in an Omniverse app."), QMessageBox::Ok);
	}
	else {
		// Actual opening of USD is performed after the checkIsRunning callback
		// e.g.  pqOmniConnectViewOpener::onRunningStatusChecked
		m_viewClient->checkIsRunning(m_port);
	}
}

void pqOmniConnectViewOpener::onRunningStatusChecked(bool isRunning)
{

	const pqOmniConnectViewSettings& settings = m_settingsManager.getSettings();
	QString usdPath = settings.getUsdPath();

	// Log usd and launch paths
	OMNI_LOG_INFO("Opening USD scene files...");
	OMNI_LOG_INFO_SCOPE_START(OPEN_INFO_SCOPE_ID);
	OMNI_LOG_VAR("path to usd", usdPath.toStdString().c_str());
	OMNI_LOG_VAR("path to application", m_launchPath.toStdString().c_str());
	OMNI_LOG_INFO_SCOPE_END(OPEN_INFO_SCOPE_ID);

	if (isRunning) {
		m_viewClient->openStage(m_port, settings.OmniServer, settings.UserName, usdPath);
	}
	else {
		pqOmniConnectUtils::QuickLaunchWriter::AppType appType = m_port == 8011 ? pqOmniConnectUtils::QuickLaunchWriter::AppType::View : pqOmniConnectUtils::QuickLaunchWriter::AppType::Create;
		QString launchFile = pqOmniConnectUtils::QuickLaunchWriter::write(usdPath, appType);
		if (launchFile.isNull()) {
			OMNI_LOG_ERROR("Failed to create the quick launch file");
			QMessageBox::warning(nullptr, tr("File creation failed."), tr("Unable to create the quick launch file"), QMessageBox::Ok);
			return;
		}
		
		QString workingDir = QFileInfo(m_launchPath).absoluteDir().absolutePath();
		QStringList args {"--exec", QString("\"%1\"").arg(launchFile)};
		m_process = new QProcess();
		bool success = m_process->startDetached(m_launchPath, args, workingDir);

		if (success) {
			OMNI_LOG_ERROR("Omniverse application launched successfully.");
		}
		else {
			OMNI_LOG_ERROR("Failed to launch the Omiverse appplication");
			QMessageBox::warning(nullptr, tr("Failed to open USD"), tr("Unable to open USD stage"), QMessageBox::Ok);
		}
	}
}

void pqOmniConnectViewOpener::onUsdStageOpened(bool success)
{
	if (success) {
		OMNI_LOG_ERROR("Successfully posted the USD changes to the Omniverse app.");
	}
	else {
		OMNI_LOG_ERROR("Failed to post the USD changes");
		QMessageBox::warning(nullptr, tr("Failed to open USD"), tr("Unable to open USD stage"), QMessageBox::Ok);
	}
}