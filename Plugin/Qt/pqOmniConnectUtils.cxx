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

#include "pqOmniConnectUtils.h"

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QStandardPaths>

namespace pqOmniConnectUtils {

	/******************************
	* Spinner
	******************************/
	void Spinner::setState(bool on, bool useWaitCursorOnSpin) {
		if (on) {
			QApplication::setOverrideCursor(useWaitCursorOnSpin ? Qt::WaitCursor : Qt::BusyCursor);
		}
		else {
			QApplication::restoreOverrideCursor();
		}
		QApplication::processEvents();
	}

	/******************************
	* QuickLaunchWriter
	******************************/
	QString QuickLaunchWriter::write(const QString& usdPath, AppType appType) {
		QString filePath = QString("%1/quickLaunch.py").arg(pqOmniConnectUtils::StandardPaths::baseFolderPath());
		QFile file(filePath);
		bool success = false;
		if (file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text)) {
			QTextStream stream(&file);
					if (appType == AppType::View) {
			stream << "import omni.view.startup" << Qt::endl;
			stream << "import asyncio" << Qt::endl;
			stream << "import omni.usd" << Qt::endl;
			stream << "from omni.view.startup import get_instance as get_startup_instance" << Qt::endl;
			stream << "startup_window = get_startup_instance()" << Qt::endl;
			stream << "startup_window._close_fn()" << Qt::endl;
			stream << "async def task():" << Qt::endl;
			stream << "    await asyncio.sleep(3)" << Qt::endl;
			stream << "    await omni.usd.get_context().open_stage_async(\"" << usdPath << "\")" << Qt::endl;
			stream << "asyncio.ensure_future(task())" << Qt::endl;
			}
					else {
			stream << "import asyncio" << Qt::endl;
			stream << "import omni.usd" << Qt::endl;
			stream << "async def task():" << Qt::endl;
			stream << "    await asyncio.sleep(3)" << Qt::endl;
			stream << "    await omni.usd.get_context().open_stage_async(\"" << usdPath << "\")" << Qt::endl;
			stream << "asyncio.ensure_future(task())" << Qt::endl;
			}
			success = stream.status() == QTextStream::Ok;
		}
		file.close();
		return success ? filePath : QString();
	}

	/****************************
	* StandardPaths
	****************************/
	void StandardPaths::createPathIfMissing(const QString& folderPath) {
		QDir dir(folderPath);
		if (!dir.exists()) {
			dir.mkpath(".");
		}
	}

	QString StandardPaths::baseFolderPath() {
		QString path = QString("%1/OmniverseConnector").arg(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
		createPathIfMissing(path);
		return path;
	}

	QString StandardPaths::settingsFilePath() {
		return QString("%1/settings.ini").arg(baseFolderPath());
	}

	/****************************
	* ServerUtils
	****************************/
	QString ServerUtils::formatServerUrlWithProtocol(const QString& serverUrl) {
		if (serverUrl.contains("://")) {
			// Server already contains a protocol prefix
			return serverUrl;
		}
		else {
			// Add omniverse:// protocol prefix
			return QString("omniverse://%1").arg(serverUrl);
		}
	}

} // namespace