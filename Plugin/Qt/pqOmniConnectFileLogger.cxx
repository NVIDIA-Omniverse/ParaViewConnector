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

#include "pqOmniConnectFileLogger.h"
#include "pqOmniConnectUtils.h"

#include "vtkLogger.h"
#include "vtkPVLogger.h"

#include <QFile>
#include <QTextStream>

static constexpr const char* LOGGER_NAME = "pqOmniConnectFileLogger";

pqOmniConnectFileLogger::pqOmniConnectFileLogger() {

	QString filePath = QString("%1/log.txt").arg(pqOmniConnectUtils::StandardPaths::baseFolderPath());
	m_file = new QFile(filePath);
	if (m_file->open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text)) {
		m_textStream = new QTextStream(m_file);
		vtkLogger::AddCallback(LOGGER_NAME, loggerCallback, m_textStream, vtkLogger::VERBOSITY_INFO, nullptr, flushCallback);

		if (!vtkLogger::IsEnabled()) {
			*m_textStream << "logging support is disabled in this build." << endl;
			m_textStream->flush();
		}
	}
}

pqOmniConnectFileLogger::~pqOmniConnectFileLogger() {
	vtkLogger::RemoveCallback(LOGGER_NAME);
	m_textStream->flush();
	delete m_textStream;
	m_file->close();
	delete m_file;
}

void pqOmniConnectFileLogger::loggerCallback(void* userData, const vtkLogger::Message& message) {
	QTextStream* textStream = reinterpret_cast<QTextStream*>(userData);
	*textStream << "\n";
	*textStream << message.preamble;
	*textStream << message.indentation;
	*textStream << message.prefix;
	*textStream << message.message;
}

void pqOmniConnectFileLogger::flushCallback(void* userData) {
	QTextStream* textStream = reinterpret_cast<QTextStream*>(userData);
	textStream->flush();
}