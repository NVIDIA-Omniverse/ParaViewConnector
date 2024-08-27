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

#pragma once

#include <QString>

namespace pqOmniConnectUtils {

	// Turn loading spinner on/off
	class Spinner {
	public:
		static void setState(bool on, bool useWaitCursorOnSpin = true);
	};

	// Create quicklaunch.py for launching Omniverse applications
	class QuickLaunchWriter {
	public:
		enum AppType {
			View = 0,
			Create = 1
		};

		static QString write(const QString& usdPath, AppType appType);
	};

	// Paths used by the Omniverse Connector plugin
	class StandardPaths {
	public:
		static QString baseFolderPath();
		static QString settingsFilePath();

	private:
		static void createPathIfMissing(const QString& folderPath);
	};

} // namespace