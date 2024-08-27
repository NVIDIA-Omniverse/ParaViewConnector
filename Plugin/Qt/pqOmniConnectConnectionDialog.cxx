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

#include "pqOmniConnectConnectionDialog.h"
#include "pqOmniConnectUtils.h"
#include "pqOmniConnectLogger.h"
#include "pqOmniConnectViewsSettingsManager.h"

#include "vtkPVOmniConnectProxy.h"

#include <QPushButton>
#include <QLabel>
#include <QFuture>
#include <QtConcurrent/qtconcurrentrun.h>
#include <QLayout>
#include <QVBoxLayout>

pqOmniConnectConnectionDialog::pqOmniConnectConnectionDialog(const pqOmniConnectViewsSettingsManager* settingsManager, const std::string& host) :
	QDialog(nullptr),
	m_connector(settingsManager->getConnector()),
	m_host(host)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setModal(true);

	QVBoxLayout* vLayout = new QVBoxLayout();

	QString msg = QString("Connecting to %1...\nPlease sign in using your browser").arg(QString::fromStdString(host));
	QLabel* label = new QLabel(msg);

	QPushButton* button = new QPushButton("Cancel");
	connect(button, SIGNAL(clicked()), this, SLOT(onCancelClicked()));

	vLayout->addWidget(label);
	vLayout->addWidget(button);
	this->setLayout(vLayout);

	connect(&m_watcher, &QFutureWatcher<bool>::finished, this, &pqOmniConnectConnectionDialog::onFutureFinished);
	connect(&m_watcher, &QFutureWatcher<bool>::canceled, this, &pqOmniConnectConnectionDialog::onFutureFinished);
}

pqOmniConnectConnectionDialog::~pqOmniConnectConnectionDialog() 
{
}

bool pqOmniConnectConnectionDialog::openConnection() 
{
	return vtkPVOmniConnectProxy::OpenConnection(m_connector, false);
}

void pqOmniConnectConnectionDialog::openConnectionAsync() 
{
	pqOmniConnectUtils::Spinner::setState(true, false);
	QFuture<bool> future = QtConcurrent::run(this, &pqOmniConnectConnectionDialog::openConnection);
	m_watcher.setFuture(future);
}

void pqOmniConnectConnectionDialog::onFutureFinished() 
{
	pqOmniConnectUtils::Spinner::setState(false);
	if (m_watcher.isCanceled()) {
		// onFutureFinished gets called twice on auth cancel, first time comes from the QFutureWatcher::canceled
		// signal and second time comes from QFutureWatcher::finished signal. Here we only fire onConnectEnd
		// signal after everything is done.

		//	note that we can't use future().result() here since cancelled task yields no result
		if (m_watcher.isFinished()) {
			OMNI_LOG_INFO("Connection to Nucleus at %s failed.", m_host.c_str());
			emit onConnectEnd(false);
			close();
		}
	}
	else {
		if (m_watcher.future().result()) {
			OMNI_LOG_INFO("Connection to Nucleus at %s established.", m_host.c_str());
			emit onConnectEnd(true);
		}
		else {
			OMNI_LOG_INFO("Connection to Nucleus at %s failed.", m_host.c_str());
			emit onConnectEnd(false);
		}
		close();
	}
}

void pqOmniConnectConnectionDialog::onCancelClicked()
{
	vtkPVOmniConnectProxy::CancelOmniClientAuth(m_connector);
	m_watcher.cancel();
}