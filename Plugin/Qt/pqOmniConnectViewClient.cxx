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

#include "pqOmniConnectViewClient.h"
#include "pqOmniConnectUtils.h"

#include <QNetworkReply>

pqOmniConnectViewClient::pqOmniConnectViewClient(QObject *parent)
    : QObject(parent)
{
	m_manager = new QNetworkAccessManager();
	connect(m_manager, &QNetworkAccessManager::finished, this, &pqOmniConnectViewClient::replyFinished);
}

pqOmniConnectViewClient::~pqOmniConnectViewClient() {
	delete m_manager;
}

void pqOmniConnectViewClient::openStage(quint16 port, const QString& server, const QString& userName, const QString& usdPath) {

	pqOmniConnectUtils::Spinner::setState(true);

	m_requestType = pqOmniConnectRequestType::OpenUsdStage;
	QUrl url(QString("http://localhost:%1/kit/usd/stage").arg(QString::number(port)));
	QString dataString = QString("{\"server\":\"%1\", \"user\":\"%2\", \"uri\":\"%3\"}").arg(server).arg(userName).arg(usdPath);
	QByteArray dataArray = dataString.toUtf8();
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/json"));
	request.setHeader(QNetworkRequest::ContentLengthHeader, QVariant(dataArray.length()));
	m_manager->post(request, dataArray);
}

void pqOmniConnectViewClient::checkIsRunning(quint16 port) {

	pqOmniConnectUtils::Spinner::setState(true);

	m_requestType = pqOmniConnectRequestType::RunningStatus;
	QUrl url(QString("http://localhost:%1/controlport/status").arg(QString::number(port)));
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/json"));
	m_manager->get(request);
}

void pqOmniConnectViewClient::replyFinished(QNetworkReply* reply) {

	pqOmniConnectUtils::Spinner::setState(false);

	bool success = reply->error() == QNetworkReply::NetworkError::NoError;
	if (m_requestType == pqOmniConnectRequestType::RunningStatus) {
		Q_EMIT runningStatusChecked(success);
	}
	else {
		Q_EMIT usdStageOpened(success);
	}
}