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

#include "pqOmniConnectViewsSettingsManager.h"
#include "pqOmniConnectSettingsDialog.h"
#include "pqOmniConnectUtils.h"
#include "pqOmniConnectLogger.h"
#include "pqOmniConnectConnectionDialog.h"

#include "pqCoreUtilities.h"
#include "pqApplicationCore.h"
#include "pqObjectBuilder.h"
#include "pqServer.h"
#include "pqView.h"
#include "pqActiveObjects.h"

#include "vtkOmniConnectLogCallback.h"
#include "vtkSMProxySelectionModel.h"
#include "vtkRenderWindow.h"
#include "vtkCallbackCommand.h"
#include "vtkProcessModule.h"
#include "vtkObjectFactory.h"
#include "vtkSMViewProxy.h"
#include "vtkSMSessionProxyManager.h"

#include "vtkPVOmniConnectSettings.h"
#include "vtkPVOmniConnectProxy.h"

static constexpr const char* OMNI_RENDERVIEW_PROXY_GROUP = "views";
static constexpr const char* OMNI_RENDERVIEW_PROXY_NAME = "OmniConnectRenderView";

static constexpr const char* VIEW_SETTINGS_SCOPE_ID = "connector_view_settings";

// Just reroutes back to the settings manager, but cleans up automatically as observer
class pqOmniConnectViewsSettingsManagerCallbacks : public vtkObject
{
  public:
    static pqOmniConnectViewsSettingsManagerCallbacks* New();
    vtkTypeMacro(pqOmniConnectViewsSettingsManagerCallbacks, vtkObject);
    void PrintSelf(ostream& os, vtkIndent indent) override;

    void Initialize(pqOmniConnectViewsSettingsManager* settingsManager) { m_settingsManager = settingsManager; }

    void processViewEvents(vtkObject* object, unsigned long eventid, void* clientData) { m_settingsManager->processViewEvents(object, eventid, clientData); }
    void resetConnectionState() { m_settingsManager->resetConnectionState(); }
    void resetOmniClientState() { m_settingsManager->resetOmniClientState(); }

    pqOmniConnectViewsSettingsManager* m_settingsManager = nullptr;

  protected:
    pqOmniConnectViewsSettingsManagerCallbacks();
    ~pqOmniConnectViewsSettingsManagerCallbacks() override;

  private:
    pqOmniConnectViewsSettingsManagerCallbacks(const pqOmniConnectViewsSettingsManagerCallbacks&) = delete;
    void operator=(const pqOmniConnectViewsSettingsManagerCallbacks&) = delete;
};

vtkStandardNewMacro(pqOmniConnectViewsSettingsManagerCallbacks);

pqOmniConnectViewsSettingsManagerCallbacks::pqOmniConnectViewsSettingsManagerCallbacks()
{}

pqOmniConnectViewsSettingsManagerCallbacks::~pqOmniConnectViewsSettingsManagerCallbacks()
{}

void pqOmniConnectViewsSettingsManagerCallbacks::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

QString pqOmniConnectViewSettings::getUsdPath() const 
{
  // Determine *.usd or *.usda path based from the settings
  QString outputDirectory = OutputLocal ? LocalOutputDirectory : OmniWorkingDirectory;
  QString extension = LiveWorkflowEnabled ? ".live" : (OutputBinary ? ".usd" : ".usda");
  QString livePrefix = LiveWorkflowEnabled ? "live_" : "";
  QString fileName = NumPartitions > 1 ? QString("%1MultiScene%2").arg(livePrefix).arg(extension) : QString("%1FullScene%2").arg(livePrefix).arg(extension);
  QString usdPath;
  if (OutputLocal) {
    usdPath = QString("%1/Session_%2/%3").arg(outputDirectory).arg(QString::number(SessionNumber)).arg(fileName);
  }
  else {
    usdPath = QString("omniverse://%1%2/Session_%3/%4").arg(OmniServer).arg(outputDirectory).arg(QString::number(SessionNumber)).arg(fileName);
  }
  return usdPath;
}

bool pqOmniConnectViewSettings::hasSameRootOutputDirectory(const pqOmniConnectViewSettings& otherSettings) const
{
  if (this->OutputLocal && otherSettings.OutputLocal) {
    return QString::compare(this->LocalOutputDirectory, otherSettings.LocalOutputDirectory, Qt::CaseInsensitive) == 0;
  }
  if (!this->OutputLocal && !otherSettings.OutputLocal) {
    if (QString::compare(this->OmniServer, otherSettings.OmniServer, Qt::CaseInsensitive) != 0) {
      return false;
    }
    else if (QString::compare(this->OmniWorkingDirectory, otherSettings.OmniWorkingDirectory, Qt::CaseInsensitive) != 0) {
      return false;
    }
    return true;
  }
  return false;
}

pqOmniConnectViewsSettingsManager::pqOmniConnectViewsSettingsManager(QObject* parent) : QObject(parent) 
{
  // Keep track of the current settings
  updateCurrentSettings();

  // Listen to connector view's creation and deletion events
  pqApplicationCore* core = pqApplicationCore::instance();
  pqObjectBuilder* builder = core->getObjectBuilder();
  connect(builder, &pqObjectBuilder::viewCreated, this, &pqOmniConnectViewsSettingsManager::onViewCreated);
  connect(this, SIGNAL(onConnectEnd(bool)), this, SLOT(connectServerFinished(bool)));

  vtkCallbacks.TakeReference(pqOmniConnectViewsSettingsManagerCallbacks::New());
  vtkCallbacks->Initialize(this);

  // Watch connection changes indicating new proxies and proxymanager (to reset connection)
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  pm->AddObserver(vtkCommand::ConnectionCreatedEvent, vtkCallbacks.Get(), &pqOmniConnectViewsSettingsManagerCallbacks::resetOmniClientState);
  pm->AddObserver(vtkCommand::ConnectionClosedEvent, vtkCallbacks.Get(), &pqOmniConnectViewsSettingsManagerCallbacks::resetConnectionState);

  // Find current connection proxy and initialize invariants
  vtkCallbacks->resetConnectionState();
  vtkCallbacks->resetOmniClientState();
}

pqOmniConnectViewsSettingsManager::~pqOmniConnectViewsSettingsManager() 
{
}

QString pqOmniConnectViewsSettingsManager::getActiveViewName() 
{
  // Returns current connector view name. If current view isn't an OV connector view we return null
  pqView* view = pqActiveObjects::instance().activeView();
  if (!view || (view &&!view->getViewProxy())) {
    return QString::null;
  }
  QString logName = QString(view->getViewProxy()->GetLogName());
  return m_viewsSettings.contains(logName) ? logName : QString::null;
}

bool pqOmniConnectViewsSettingsManager::isOmniViewActive()
{
  QString activeViewName = getActiveViewName();
  std::string readableName = activeViewName.toStdString();

  return m_viewsSettings.contains(activeViewName);
}

bool pqOmniConnectViewsSettingsManager::isOmniView(pqView* view)
{
  pqServer* server = view->getServer();
  vtkSMProxy* proxy = server->activeViewSelectionModel()->GetCurrentProxy();

  return proxy && strcmp(proxy->GetXMLGroup(), OMNI_RENDERVIEW_PROXY_GROUP) == 0 && strcmp(proxy->GetXMLName(), OMNI_RENDERVIEW_PROXY_NAME) == 0;
}

pqOmniConnectViewSettings pqOmniConnectViewsSettingsManager::getSettings() 
{
  QString activeViewName = getActiveViewName();

  // Return settings used at connector view's creations if any, otherwise returns current settings
  if (m_viewsSettings.contains(activeViewName)) 
  {
    return m_viewsSettings[activeViewName];
  }
  return m_currentSettings;
}

void pqOmniConnectViewsSettingsManager::connectServer(const pqOmniConnectViewSettings& settings, bool openConnection) 
{
  disconnectServer();

  OmniConnectEnvironment omniEnv{ 0, 1 }; // use default partition values

  std::string localOutputDir = settings.LocalOutputDirectory.toStdString();
  std::string omniServer = settings.OmniServer.toStdString();
  std::string omniWorkingDir = settings.OmniWorkingDirectory.toStdString();
  std::string rootFileName;
  OmniConnectSettings connectSettings = {
    omniServer.c_str(),
    omniWorkingDir.c_str(),
    localOutputDir.c_str(),
    rootFileName.c_str(),
    settings.OutputLocal,
    settings.OutputBinary,
    settings.UpAxis == pqOmniConnectViewAxis::Y ? OmniConnectAxis::Y : OmniConnectAxis::Z,
    settings.UsePointInstancer,
    settings.UseMeshVolume,
    settings.CreateNewOmniSession
  };

  pqServer* server = pqActiveObjects::instance().activeServer();
  m_connector = server->proxyManager()->GetProxy(vtkPVOmniConnectProxy::OMNICONNECT_GROUP_NAME, vtkPVOmniConnectProxy::OMNICONNECT_PROXY_NAME);
  vtkPVOmniConnectProxy::CreateConnection(m_connector, connectSettings, omniEnv);

  if(openConnection)
  {
    pqOmniConnectConnectionDialog* connectDialog = new pqOmniConnectConnectionDialog(this, omniServer);
    connect(connectDialog, SIGNAL(onConnectEnd(bool)), this, SIGNAL(onConnectEnd(bool)));
    connectDialog->openConnectionAsync();
    connectDialog->exec();
  }
}

void pqOmniConnectViewsSettingsManager::disconnectServer() 
{
  if (m_connector) 
  {
    vtkPVOmniConnectProxy::DestroyConnection(m_connector);
    resetConnectionState();
  }
}

void pqOmniConnectViewsSettingsManager::resetConnectionState()
{
  m_connector = nullptr; // currently invariant for "Has a connection been attempted since the last disconnectServer()"
  m_connectionValid = false;
}

void pqOmniConnectViewsSettingsManager::resetOmniClientState()
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  vtkSMProxy* connectProxy = server->proxyManager()->GetProxy(vtkPVOmniConnectProxy::OMNICONNECT_GROUP_NAME, vtkPVOmniConnectProxy::OMNICONNECT_PROXY_NAME);
  m_omniClientEnabled = vtkPVOmniConnectProxy::GetOmniClientEnabled(connectProxy);
}

void pqOmniConnectViewsSettingsManager::connectServerFinished(bool success)
{
  m_connectionValid = success;
}

void pqOmniConnectViewsSettingsManager::setLiveWorkflowEnabled(bool enabled)
{
  QString activeViewName = getActiveViewName();

  auto it = m_viewsSettings.find(activeViewName);

  // Return settings used at connector view's creations if any, otherwise returns current settings
  if (it != m_viewsSettings.end()) 
  {
    it->LiveWorkflowEnabled = enabled;
  }
}

void pqOmniConnectViewsSettingsManager::onViewCreated(pqView* view) 
{
  if (pqOmniConnectViewsSettingsManager::isOmniView(view)) 
  {
    // Prompt user to modify settings for this connector view
    pqOmniConnectSettingsDialog* dialog = new pqOmniConnectSettingsDialog(pqCoreUtilities::mainWidget(), *this);

    dialog->setOpenViewBoxVisibility(false);
    dialog->setButtonConfiguration(false);
    dialog->resizeLayoutToContent();
    dialog->checkDataAndShow(); // Purely to update the OpenView group, even though it's invisible, so that all paths to 'Current' apps will be updated once the dialog is closed
    dialog->exec(); // Block to let the user choose the view settings

    // Set Window name and observe view events
    vtkRenderWindow* win = view->getViewProxy()->GetRenderWindow();
    win->SetWindowName(view->getViewProxy()->GetLogName());

    // Cache the connection settings used for this view's creation.
    pqServer* server = view->getServer();
    cacheConnectorViewSettings(view->getViewProxy()->GetLogName(), server->getNumberOfPartitions());

    // Observe view deletion event to remove its settings from cache
    win->AddObserver(vtkCommand::DeleteEvent, vtkCallbacks.Get(), &pqOmniConnectViewsSettingsManagerCallbacks::processViewEvents);
  }
}

void pqOmniConnectViewsSettingsManager::updateCurrentSettings() 
{
  vtkPVOmniConnectSettings* settings = vtkPVOmniConnectSettings::GetInstance();

  const char* omniServer = settings->GetOmniServer();
  const char* omniWorkingDirectory = settings->GetOmniWorkingDirectory();
  const char* omniOutputDirectory = settings->GetLocalOutputDirectory();
  int outputLocal = settings->GetOutputLocal();
  int createNewOmniSession = settings->GetCreateNewOmniSession();

  // Force server disconnection if the following parameters change, to update the dummy proxy
  if(  m_currentSettings.OmniServer.compare(omniServer) != 0
    || m_currentSettings.OmniWorkingDirectory.compare(omniWorkingDirectory) != 0
    || m_currentSettings.LocalOutputDirectory.compare(omniOutputDirectory) != 0
    || m_currentSettings.OutputLocal != (bool)outputLocal
    || m_currentSettings.CreateNewOmniSession != (bool)createNewOmniSession)
    disconnectServer();

  m_currentSettings = {
    omniServer,
    omniWorkingDirectory,
    omniOutputDirectory,
    outputLocal == 0 ? false : true,
    createNewOmniSession == 0 ? false : true,
    settings->GetOutputBinary() == 0 ? false : true,
    (settings->GetUpAxis() == 0) ? pqOmniConnectViewAxis::Y : pqOmniConnectViewAxis::Z,
    settings->GetUsePointInstancer() == 0 ? false : true,
    settings->GetUseStickLines() == 0 ? false : true,
    settings->GetUseStickWireframe() == 0 ? false : true,
    settings->GetUseMeshVolume() == 0 ? false : true,
    settings->GetShowOmniErrors() == 0 ? false : true,
    settings->GetShowOmniDebug() == 0 ? false : true,
    settings->GetNucleusVerbosity(),
    1, // default value for number of partitions
    0, // default session number
    "Guest", // default username
    false // default value for live workflow enabled
  };
}

void pqOmniConnectViewsSettingsManager::cacheConnectorViewSettings(const char* key, int numPartitions) {
  // Cache the connection settings used for this view's creation.

  pqOmniConnectUtils::Spinner::setState(true);

  vtkPVOmniConnectSettings* settings = vtkPVOmniConnectSettings::GetInstance();

  // Find session number and username used at connector view's creation
  int sessionNumber = 0;
  QString userName;
  findConnectorViewSessionInfo(userName, sessionNumber);

  pqOmniConnectViewSettings viewSettings = {
    settings->GetOmniServer(),
    settings->GetOmniWorkingDirectory(),
    settings->GetLocalOutputDirectory(),
    settings->GetOutputLocal() == 0 ? false : true,
    settings->GetCreateNewOmniSession() == 0 ? false : true,
    settings->GetOutputBinary() == 0 ? false : true,
    (settings->GetUpAxis() == 0) ? pqOmniConnectViewAxis::Y : pqOmniConnectViewAxis::Z,
    settings->GetUsePointInstancer() == 0 ? false : true,
    settings->GetUseStickLines() == 0 ? false : true,
    settings->GetUseStickWireframe() == 0 ? false : true,
    settings->GetUseMeshVolume() == 0 ? false : true,
    settings->GetShowOmniErrors() == 0 ? false : true,
    settings->GetShowOmniDebug() == 0 ? false : true,
    settings->GetNucleusVerbosity(),
    numPartitions,
    sessionNumber,
    userName,
    false
  };
  m_viewsSettings[QString(key)] = viewSettings;

  pqOmniConnectUtils::Spinner::setState(false);

  // log settings used for this view creation
  OMNI_LOG_INFO("Omniverse Connector View `%s` created", key);
  OMNI_LOG_INFO_SCOPE_START(VIEW_SETTINGS_SCOPE_ID);
  OMNI_LOG_VAR("OmniServer", settings->GetOmniServer());
  OMNI_LOG_VAR("OmniWorkingDirectory", settings->GetOmniWorkingDirectory());
  OMNI_LOG_VAR("LocalOutputDirectory", settings->GetLocalOutputDirectory());
  OMNI_LOG_VAR("OutputLocal", settings->GetOutputLocal() == 1);
  OMNI_LOG_VAR("CreateNewOmniSession", settings->GetCreateNewOmniSession() == 1);
  OMNI_LOG_VAR("OutputBinary", settings->GetOutputBinary() == 1);
  OMNI_LOG_VAR("UpAxis", (settings->GetUpAxis() == 0) ? "Y" : "Z");
  OMNI_LOG_VAR("UsePointInstancer", settings->GetUsePointInstancer() == 1);
  OMNI_LOG_VAR("UseStickLines", settings->GetUseStickLines() == 1);
  OMNI_LOG_VAR("UseStickWireframe", settings->GetUseStickWireframe() == 1);
  OMNI_LOG_VAR("UseMeshVolume", settings->GetUseMeshVolume() == 1);
  OMNI_LOG_VAR("ShowOmniErrors", settings->GetShowOmniErrors() == 1);
  OMNI_LOG_VAR("ShowOmniDebug", settings->GetShowOmniDebug() == 1);
  OMNI_LOG_VAR("NucleusVerbosity", settings->GetNucleusVerbosity());
  OMNI_LOG_VAR("No. Partitions", numPartitions);
  OMNI_LOG_VAR("Session No.", sessionNumber);
  OMNI_LOG_INFO_SCOPE_END(VIEW_SETTINGS_SCOPE_ID);
}

void pqOmniConnectViewsSettingsManager::findConnectorViewSessionInfo(QString& userNameOut, int& sessionNumberOut) 
{
  // Find the username and session number for the connector view
  if(!m_connector)
  {
    this->connectServer(m_currentSettings, false);
    m_connectionValid = vtkPVOmniConnectProxy::OpenConnection(m_connector, false);
  }

  if(m_connectionValid)
  {
    sessionNumberOut = vtkPVOmniConnectProxy::GetLatestSessionNumber(m_connector);
    QString serverUrl = QString("omniverse://%1/").arg(QString::fromStdString(m_currentSettings.OmniServer.toStdString()));
    userNameOut = QString::fromStdString(vtkPVOmniConnectProxy::GetUser(m_connector, serverUrl.toStdString().c_str()));
  }
}

void pqOmniConnectViewsSettingsManager::processViewEvents(vtkObject* obj, unsigned long eventid, void* clientData) 
{
  Q_UNUSED(clientData);
  // Remove the connector view's settings from cache on deletion
  if (eventid == vtkCommand::DeleteEvent) 
  {
    vtkRenderWindow* win = vtkRenderWindow::SafeDownCast(obj);
    QString logName = QString(win->GetWindowName());
    m_viewsSettings.remove(logName);

    OMNI_LOG_INFO("Omniverse Connector View `%s` destroyed", logName.toStdString().c_str());
  }
}

bool pqOmniConnectViewsSettingsManager::isSettingsInvalid(QString& conflictingViewOut) const 
{
  // Check current settings with settings used by any connector view(s).
  // current settings are considered invalid if it points to the same
  // session used by another connector view.
  for (auto it = m_viewsSettings.begin(); it != m_viewsSettings.end(); it++) 
  {
    if (m_currentSettings.hasSameRootOutputDirectory(it.value()) && !m_currentSettings.CreateNewOmniSession) 
    {
      conflictingViewOut = it.key();
      return true;
    }
  }
  return false;
}