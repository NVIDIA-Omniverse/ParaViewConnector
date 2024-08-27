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

#include "pqOmniConnectActions.h"

#include "pqOmniConnectAccountDialog.h"
#include "pqOmniConnectSettingsDialog.h"
#include "pqOmniConnectAboutDialog.h"
#include "pqOmniConnectFolderPickerDialog.h"
#include "pqOmniConnectViewOpener.h"
#include "pqOmniConnectViewsSettingsManager.h"
#include "pqOmniConnectFileLogger.h"
#include "OmniConnect.h"

#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

#include "vtkSMIntVectorProperty.h"
#include "vtkSMProxySelectionModel.h"

#include "pqCoreUtilities.h"
#include "pqActiveObjects.h"
#include "pqApplicationCore.h"
#include "pqObjectBuilder.h"
#include "pqServer.h"

pqOmniConnectActions::pqOmniConnectActions(QWidget* p)
  : QToolBar("Omniverse Connector", p),
  m_viewOpener(nullptr),
  m_liveWorkflowAction(nullptr),
  m_activeOmniView(nullptr)
{
  if (OmniConnect::OmniClientEnabled()) {
    this->addToolbarAction(":/omniWidgets/Icons/SignIn.svg", "Sign In", SLOT(onSignInClicked()));
  }
  this->addToolbarAction(":/omniWidgets/Icons/Settings.svg", "Settings", SLOT(onSettingsClicked()));
  m_liveWorkflowAction = this->addToolbarAction(":/omniWidgets/Icons/OmniLivePanel.svg", "Live Workflow", SLOT(onLiveWorkflowClicked()), true);
  this->addToolbarAction(":/omniWidgets/Icons/SendToView.svg", "Open View in Omniverse App", SLOT(onOpenViewClicked()));
  this->addToolbarAction(":/omniWidgets/Icons/Help.svg", "Help", SLOT(onHelpClicked()));
  this->addToolbarAction(":/omniWidgets/Icons/About.svg", "About", SLOT(onAboutClicked()));

  connect(&pqActiveObjects::instance(), SIGNAL(viewChanged(pqView*)), this, SLOT(onActiveViewChanged(pqView*)));

  pqApplicationCore* core = pqApplicationCore::instance();
  pqObjectBuilder* builder = core->getObjectBuilder();
  connect(builder, &pqObjectBuilder::viewCreated, this, &pqOmniConnectActions::onViewCreated);

  m_liveWorkflowAction->setEnabled(false);

  m_settingsManager = new pqOmniConnectViewsSettingsManager();
  m_fileLogger = new pqOmniConnectFileLogger();
}

pqOmniConnectActions::~pqOmniConnectActions() 
{
  delete m_settingsManager;
  delete m_viewOpener;
  delete m_fileLogger;
}

QAction* pqOmniConnectActions::addToolbarAction(const QString& iconPath, const QString& actionName, const char* slotFunc, bool checkable) 
{
  QAction* action = new QAction(QIcon(iconPath), actionName, this);
  action->setCheckable(checkable);
  connect(action, SIGNAL(triggered()), this, slotFunc);
  this->addAction(action);

  return action;
}

void pqOmniConnectActions::onOpenViewClicked() 
{
  if (m_viewOpener) {
    delete m_viewOpener;
  }
  m_viewOpener = new pqOmniConnectViewOpener(*m_settingsManager);
  m_viewOpener->loadConfig();
  if (m_viewOpener->isUnset()) {
    int returnCode = QMessageBox::warning(nullptr, "", "Application is unset. Please select an Omniverse app from the 'Open View settings' in the Omniverse Connector Settings dialog", QMessageBox::Ok | QMessageBox::Cancel);
    if (returnCode == QMessageBox::Ok) {
      pqOmniConnectSettingsDialog* dialog = new pqOmniConnectSettingsDialog(pqCoreUtilities::mainWidget(), *m_settingsManager);
      dialog->checkDataAndShow();
    }
  }
  else {
    m_viewOpener->openUSD();
  }
}

void pqOmniConnectActions::onSignInClicked() 
{
  pqOmniConnectAccountDialog* dialog = new pqOmniConnectAccountDialog(pqCoreUtilities::mainWidget(), *m_settingsManager);
  dialog->show();
}

void pqOmniConnectActions::onSettingsClicked() 
{
  pqOmniConnectSettingsDialog* dialog = new pqOmniConnectSettingsDialog(pqCoreUtilities::mainWidget(), *m_settingsManager);
  dialog->checkDataAndShow();
}

void pqOmniConnectActions::onLiveWorkflowClicked() 
{
  bool liveWorkflowEnabled = m_liveWorkflowAction->isChecked();
  m_settingsManager->setLiveWorkflowEnabled(liveWorkflowEnabled);

  setViewIntProperty("LiveWorkflow", liveWorkflowEnabled);
}

void pqOmniConnectActions::setViewIntProperty(const char* propertyName, int value) 
{
  if(m_activeOmniView == nullptr)
	  return;

  pqServer* server = m_activeOmniView->getServer();
  vtkSMProxy* proxy = server->activeViewSelectionModel()->GetCurrentProxy();

  vtkSMProperty* prop = proxy->GetProperty(propertyName);
  
  Q_ASSERT(prop != nullptr);
  vtkSMIntVectorProperty* vectorProp = vtkSMIntVectorProperty::SafeDownCast(prop);
  Q_ASSERT(vectorProp != nullptr);
  vectorProp->SetElement(0, value); // assume 1 element for now
  proxy->UpdateVTKObjects();
}

void pqOmniConnectActions::onHelpClicked() 
{
  QString link = "https://docs.omniverse.nvidia.com/con_connect/con_connect/paraview.html";
  QDesktopServices::openUrl(QUrl(link));
}

void pqOmniConnectActions::onAboutClicked() 
{
  pqOmniConnectAboutDialog* dialog = new pqOmniConnectAboutDialog(pqCoreUtilities::mainWidget(), *m_settingsManager);
  dialog->show();
}

void pqOmniConnectActions::onViewCreated(pqView* view)
{
  if(!view)
    return;
    
  bool isOmniView = m_settingsManager->isOmniView(view);
  
  m_liveWorkflowAction->setEnabled(isOmniView);
  m_liveWorkflowAction->setChecked(false);
}

void pqOmniConnectActions::onActiveViewChanged(pqView* view)
{
  if(!view)
    return;

  bool isOmniView = m_settingsManager->isOmniView(view);
  m_liveWorkflowAction->setEnabled(isOmniView);

  if (isOmniView)
  {
    pqOmniConnectViewSettings settings = m_settingsManager->getSettings();
    m_liveWorkflowAction->setChecked(settings.LiveWorkflowEnabled);

	  m_activeOmniView = view;
  }
  else
  {
    m_liveWorkflowAction->setChecked(false);

	  m_activeOmniView = nullptr;
  }
}