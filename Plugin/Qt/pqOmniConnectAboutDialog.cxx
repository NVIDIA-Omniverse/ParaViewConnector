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

#include "pqOmniConnectAboutDialog.h"
#include "ui_pqOmniConnectAboutDialog.h"
#include "BuildVersion.h"
#include "vtkPVOmniConnectProxy.h"

pqOmniConnectAboutDialog::pqOmniConnectAboutDialog(QWidget* p,  pqOmniConnectViewsSettingsManager& settingsManager, Qt::WindowFlags f)
  : pqOmniConnectBaseDialog(p, settingsManager, f)
{
  m_ui.setupUi(this);
  this->init();
}
	  
pqOmniConnectAboutDialog ::~pqOmniConnectAboutDialog() 
{
}

void pqOmniConnectAboutDialog::onInit(bool editable, pqOmniConnectViewSettings& settings) 
{
  Q_UNUSED(editable);
  Q_UNUSED(settings);
  connect(m_ui.closeButton, SIGNAL(clicked()), this, SLOT(reject()));
  m_ui.buildVersionLabel->setText(QString("Build: %1").arg(BUILD_VERSION));
  m_ui.dateLabel->setText(QString("Date: %1").arg(__DATE__));
  pqOmniConnectBaseDialog::tryServerConnect();
  if (m_settingsManager.getOmniClientEnabled()) 
  {
  	QString libVersion = QString::fromStdString(vtkPVOmniConnectProxy::GetClientVersion(m_settingsManager.getConnector()));
  	m_ui.libVersionLabel->setText(QString("Client Library Version: %1").arg(libVersion));
  }
  else 
  {
  	m_ui.libVersionLabel->setText("OmniClient library isn't included in this build.");
  }
}