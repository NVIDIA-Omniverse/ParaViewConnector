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

#include "pqOmniConnectSettingsDialog.h"
#include "pqOmniConnectUtils.h"
#include "pqOmniConnectFolderPickerDialog.h"
#include "pqOmniConnectLogger.h"

#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>

#include "pqCoreUtilities.h"

#include "vtkPVOmniConnectSettings.h"
#include "vtkOmniConnectLogCallback.h"

pqOmniConnectSettingsDialog::pqOmniConnectSettingsDialog(QWidget* p, pqOmniConnectViewsSettingsManager& settingsManager, Qt::WindowFlags f)
  : pqOmniConnectBaseDialog(p, settingsManager, f)
{
  m_ui.setupUi(this);
  this->init();
}

pqOmniConnectSettingsDialog ::~pqOmniConnectSettingsDialog(){
  delete m_radioGroup;
  delete m_omniLauncherClient;
}

void pqOmniConnectSettingsDialog::onInit(bool editable, pqOmniConnectViewSettings& settings) {
  m_editable = editable;

  this->initOpenViewGroup(editable, settings);
  this->initConnectionGroup(editable, settings);
  this->initUSDGroup(editable, settings);
  this->initLoggingGroup(editable, settings);

  // Set guide label
  m_ui.guideLabel->setVisible(!editable);
  if (!editable) {
    QString activeView = m_settingsManager.getActiveViewName();
    QString guide = QString("An Omniverse Connector View's connection, USD, and logging options are immutable after its creation. Please deselect %1 to modify the global options for a new view").arg(activeView);
    m_ui.guideLabel->setText(guide);
  }

  connect(m_ui.saveButton, SIGNAL(clicked()), this, SLOT(onSave()));
  connect(m_ui.cancelButton, SIGNAL(clicked()), this, SLOT(onReject()));

  m_omniLauncherClient = new pqOmniConnectLauncherClient();
  connect(m_omniLauncherClient, &pqOmniConnectLauncherClient::replyProcessed, this, &pqOmniConnectSettingsDialog::onOmniLauncherAppInfoRequested);
}

void pqOmniConnectSettingsDialog::setButtonConfiguration(bool saveCancel) 
{
  if(saveCancel)
    m_ui.saveButton->setText("Save");
  else
    m_ui.saveButton->setText("Create");
  m_ui.cancelButton->setVisible(saveCancel);
}

void pqOmniConnectSettingsDialog::setOpenViewBoxVisibility(bool visible) 
{
  m_ui.openViewSettingsGroupBox->setVisible(visible); 
}

void pqOmniConnectSettingsDialog::resizeLayoutToContent()
{
  pqOmniConnectBaseDialog::resizeWidgetToContent(this);
}

void pqOmniConnectSettingsDialog::initOpenViewGroup(bool editable, const pqOmniConnectViewSettings& settings) {
  Q_UNUSED(editable);
  Q_UNUSED(settings);

  connect(m_ui.addLocalButton, SIGNAL(clicked()), this, SLOT(addCustomLocation()));
  connect(m_ui.addRemoteButton, SIGNAL(clicked()), this, SLOT(addCustomLocation()));
  connect(m_ui.removeLocalButton, SIGNAL(clicked()), this, SLOT(removeCustomLocation()));
  connect(m_ui.removeRemoteButton, SIGNAL(clicked()), this, SLOT(removeCustomLocation()));

  // Create exclusive group so only one item can be checked
  m_radioGroup = new QButtonGroup();
  m_radioGroup->setObjectName("sendToViewRadioButtonGroup");
  m_radioGroup->addButton(m_ui.unsetRadioButton, 0);
  m_radioGroup->addButton(m_ui.sendToLocalRadioButton, 1);
  m_radioGroup->addButton(m_ui.sendToServerRadioButton, 2);
  m_radioGroup->setExclusive(true);

  this->restoreComboBoxStates(m_ui.customViewLocationComboBox);
  this->restoreComboBoxStates(m_ui.kitRemoteLocationComboBox);
  this->restoreButtonGroupStates(m_radioGroup);

  // hide remote view/kit settings for now as it isn't supported
  m_ui.widget_6->setVisible(false);
  m_ui.widget_7->setVisible(false);
  m_ui.widget_8->setVisible(false);
  m_ui.widget_9->setVisible(false);
  pqOmniConnectBaseDialog::resizeWidgetToContent(this);
}

void pqOmniConnectSettingsDialog::initConnectionGroup(bool editable, const pqOmniConnectViewSettings& settings) {

  connect(m_ui.browseButton, SIGNAL(clicked()), this, SLOT(pickOutputFolder()));

  m_ui.newSessionCheckBox->setVisible(editable);
  m_ui.browseButton->setVisible(editable);

  // Set output directory label
  if (editable) {
    this->addPropertyLink(m_ui.newSessionCheckBox, "CreateNewOmniverseSession");
    if (settings.OutputLocal) {
      m_ui.projectDirectoryLabel->setText(settings.LocalOutputDirectory);
    }
    else {
      m_ui.projectDirectoryLabel->setText(settings.OmniWorkingDirectory);
    }
  }
  else {
    m_ui.newSessionCheckBox->setChecked(settings.CreateNewOmniSession);
    if (settings.OutputLocal) {
      m_ui.projectDirectoryLabel->setText(QString("%1/Session_%2").arg(settings.LocalOutputDirectory).arg(settings.SessionNumber));
    }
    else {
      m_ui.projectDirectoryLabel->setText(QString("%1/Session_%2").arg(settings.OmniWorkingDirectory).arg(settings.SessionNumber));
    }
  }

  // Set Omniverse server label
  m_ui.serverTitleLabel->setVisible(!settings.OutputLocal);
  m_ui.serverValueLabel->setVisible(!settings.OutputLocal);
  m_ui.serverValueLabel->setText(settings.OmniServer);
}

void pqOmniConnectSettingsDialog::initUSDGroup(bool editable, const pqOmniConnectViewSettings& settings) {
  if (editable) {
    // Set up property links
    this->addPropertyLink(m_ui.outputExtComboBox, "OutputExtension");
    this->addPropertyLink(m_ui.upAxisComboBox, "UpAxis");
    this->addComboBoxPropertyLink(m_ui.pointsComboBox, "PointsRepresentation", pqOmniConnectLinkType::Index);
    this->addComboBoxPropertyLink(m_ui.linesComboBox, "LinesRepresentation", pqOmniConnectLinkType::Index);
    this->addComboBoxPropertyLink(m_ui.wireframeComboBox, "TriangleWireframeRepresentation", pqOmniConnectLinkType::Index);
    this->addComboBoxPropertyLink(m_ui.volumeComboBox, "VolumeRepresentation", pqOmniConnectLinkType::Index);

  }
  else {
    // Populate values from settings
    m_ui.outputExtComboBox->setCurrentIndex(settings.OutputBinary ? 1 : 0);
    m_ui.upAxisComboBox->setCurrentIndex(((int)settings.UpAxis)-1);
    m_ui.pointsComboBox->setCurrentIndex(settings.UsePointInstancer ? 1 : 0);
    m_ui.linesComboBox->setCurrentIndex(settings.UseStickLines ? 1 : 0);
    m_ui.wireframeComboBox->setCurrentIndex(settings.UseStickWireframe ? 1 : 0);
    m_ui.volumeComboBox->setCurrentIndex(settings.UseMeshVolume ? 1 : 0);
  }

  m_ui.outputExtComboBox->setEnabled(editable);
  m_ui.upAxisComboBox->setEnabled(editable);
  m_ui.pointsComboBox->setEnabled(editable);
  m_ui.linesComboBox->setEnabled(editable);
  m_ui.wireframeComboBox->setEnabled(editable);
  m_ui.volumeComboBox->setEnabled(editable);

  m_ui.usdOptionsGroupBox->setEnabled(editable);
}

void pqOmniConnectSettingsDialog::initLoggingGroup(bool editable, const pqOmniConnectViewSettings& settings) {
  if (editable) {
    // Set up property links
    this->addPropertyLink(m_ui.showErrorMsgcheckBox, "ShowErrorMessages");
    this->addPropertyLink(m_ui.showDebugMsgCheckBox, "ShowDebugMessages");
    this->addComboBoxPropertyLink(m_ui.verbosityComboBox, "NucleusVerbosity", pqOmniConnectLinkType::Index);
  }
  else {
    // Populate values from settings
    m_ui.showErrorMsgcheckBox->setChecked(settings.ShowOmniErrors ? true : false);
    m_ui.showDebugMsgCheckBox->setChecked(settings.ShowOmniDebug ? true : false);
    m_ui.verbosityComboBox->setCurrentIndex(settings.NucleusVerbosity);
  }

  m_ui.showErrorMsgcheckBox->setEnabled(editable);
  m_ui.showDebugMsgCheckBox->setEnabled(editable);
  m_ui.verbosityComboBox->setEnabled(editable);

  m_ui.loggingGroupBox->setEnabled(editable);
}

void pqOmniConnectSettingsDialog::checkDataAndShow() {

  pqOmniConnectUtils::Spinner::setState(true);

  // request local installation info from Omniverse Launcher before we can populate the combo boxes
  // GUI population is handled in the request callback.
  m_omniLauncherClient->requestInfo();
}

void pqOmniConnectSettingsDialog::applyOpenViewSettings(bool onlyUpdateLatestPaths)
{
  if (!onlyUpdateLatestPaths)
  {
    this->saveComboBoxStates(m_ui.sendToLocalComboBox, true);
    this->saveComboBoxStates(m_ui.customViewLocationComboBox);
    this->saveComboBoxStates(m_ui.kitRemoteLocationComboBox);
    this->saveButtonGroupStates(m_radioGroup);
  }

  // Save application port and launch path
  QString launchPath;
  if (m_ui.sendToLocalComboBox->currentText().compare("Use Current View") == 0) {
    if (m_omniLauncherClient->getAppLatestLaunchPath("view", launchPath)) {
      this->saveConfig("omniViewInfo", "port", 8011);
      this->saveConfig("omniViewInfo", "path", launchPath);
    }
    else if (m_omniLauncherClient->getAppLatestLaunchPath("view-daily", launchPath)) {
      this->saveConfig("omniViewInfo", "port", 8011);
      this->saveConfig("omniViewInfo", "path", launchPath);
    }
  }
  else if (m_ui.sendToLocalComboBox->currentText().compare("Use Current Create") == 0) {
    if (m_omniLauncherClient->getAppLatestLaunchPath("create", launchPath)) {
      this->saveConfig("omniViewInfo", "port", 8111);
      this->saveConfig("omniViewInfo", "path", launchPath);
    }
    else if (m_omniLauncherClient->getAppLatestLaunchPath("create-daily", launchPath)) {
      this->saveConfig("omniViewInfo", "port", 8111);
      this->saveConfig("omniViewInfo", "path", launchPath);
    }
  }
  else if (!onlyUpdateLatestPaths)
  {
    if (m_ui.sendToLocalComboBox->currentText().compare("Use Custom") == 0) 
    {
      quint16 port = m_ui.customViewLocationComboBox->currentText().toLower().contains("create") ? 8111 : 8011;
      this->saveConfig("omniViewInfo", "port", port);
      this->saveConfig("omniViewInfo", "path", m_ui.customViewLocationComboBox->currentText());
    }
    else 
    {
      QString slug;
      QString version;
      this->getAppSlugVersion(m_ui.sendToLocalComboBox->currentText(), slug, version);
      if (m_omniLauncherClient->getAppVersionLaunchPath(slug, version, launchPath)) {
        quint16 port = slug.contains("create") ? 8111 : 8011;
        this->saveConfig("omniViewInfo", "port", port);
        this->saveConfig("omniViewInfo", "path", launchPath);
      }
    }
  }
  OMNI_LOG_INFO("Open view Settings saved");
}

void pqOmniConnectSettingsDialog::onReject()
{
  // restore vtkPVOmniConnectSettings to what is in currentsettings
  pqOmniConnectViewSettings prevSettings = m_settingsManager.getSettings();

  this->setStringPropertyLink("OmniverseServer", prevSettings.OmniServer.toStdString());
  this->setStringPropertyLink("OmniverseWorkingDirectory", prevSettings.OmniWorkingDirectory.toStdString());
  this->setStringPropertyLink("LocalOutputDirectory", prevSettings.LocalOutputDirectory.toStdString());
  this->setIntPropertyLink("OutputLocal", prevSettings.OutputLocal ? 1 : 0);

  this->setIntPropertyLink("CreateNewOmniverseSession", prevSettings.CreateNewOmniSession ? 1 : 0);
  this->setIntPropertyLink("OutputExtension", prevSettings.OutputBinary ? 1 : 0);
  this->setIntPropertyLink("UpAxis", ((int)prevSettings.UpAxis)-1);
  this->setIntPropertyLink("PointsRepresentation", prevSettings.UsePointInstancer ? 1 : 0);
  this->setIntPropertyLink("LinesRepresentation", prevSettings.UseStickLines ? 1 : 0);
  this->setIntPropertyLink("TriangleWireframeRepresentation", prevSettings.UseStickWireframe ? 1 : 0);
  this->setIntPropertyLink("VolumeRepresentation", prevSettings.UseMeshVolume ? 1 : 0);

  // These are immediately applied, so cannot be reverted
  //this->setIntPropertyLink("ShowErrorMessages", prevSettings.ShowOmniErrors ? 1 : 0);
  //this->setIntPropertyLink("ShowDebugMessages", prevSettings.ShowOmniDebug ? 1 : 0);
  //this->setIntPropertyLink("NucleusVerbosity", prevSettings.NucleusVerbosity);

  reject();
}

void pqOmniConnectSettingsDialog::onSave() {

  // Only apply open view settings when the panel is visible to users.
  this->applyOpenViewSettings(!m_ui.openViewSettingsGroupBox->isVisible());

  if (m_editable) 
  {
      // Propertylinks are connected to the widgets and therefore the vtkPVOmniConnectSettings have already been updated on widget change.
    // Just propagate those to currentsettings
    this->pushModifiedSettings();

    OMNI_LOG_INFO("Omniverse Connector View Settings saved");
    //this->saveOmniConnectSettings(true);
  }

  QString conflictingView;
  if (m_settingsManager.isSettingsInvalid(conflictingView)) {
    QString msg = QString("Session is already in use by %1. Please toggle on the 'Create New Omniverse Session' box to create a new session.").arg(conflictingView);
    QMessageBox::warning(this, "Session in use.", msg, QMessageBox::Ok);
  }
  else {
    this->accept();
  }
}

void pqOmniConnectSettingsDialog::onOmniLauncherAppInfoRequested(bool success) {
  pqOmniConnectUtils::Spinner::setState(false);
  if (success) {
    // Populate combo boxes for send to omniverse settings
    pqOmniConnectAppInfo viewInfo;
    bool viewInfoResult = m_omniLauncherClient->getAppInfo("view", viewInfo);

    pqOmniConnectAppInfo viewDailyInfo;
    bool viewDailyInfoResult = m_omniLauncherClient->getAppInfo("view-daily", viewDailyInfo);

    pqOmniConnectAppInfo createInfo;
    bool createInfoResult = m_omniLauncherClient->getAppInfo("create", createInfo);

    pqOmniConnectAppInfo createDailyInfo;
    bool createDailyInfoResult = m_omniLauncherClient->getAppInfo("create-daily", createDailyInfo);

    pqOmniConnectAppInfo kitRemoteInfo;
    bool kitRemoteInfoResult = m_omniLauncherClient->getAppInfo("kit_remote", kitRemoteInfo);

    pqOmniConnectAppInfo kitRemoteDailyInfo;
    bool kitRemoteDailyInfoResult = m_omniLauncherClient->getAppInfo("kit_remote_daily", kitRemoteDailyInfo);

    bool foundLocalInstalls = viewInfoResult || viewDailyInfoResult || createInfoResult || createDailyInfoResult || kitRemoteInfoResult || kitRemoteDailyInfoResult;
    if (!foundLocalInstalls) 
    {
      vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::WARNING, nullptr, "Unable to find a View or Create installation, so 'Open View in Omniverse App' will not work.");
    }
    else {
      if (viewInfoResult || viewDailyInfoResult) m_ui.sendToLocalComboBox->addItem("Use Current View");
      if (createInfoResult || createDailyInfoResult) m_ui.sendToLocalComboBox->addItem("Use Current Create");
      if (kitRemoteInfoResult || kitRemoteDailyInfoResult) m_ui.sendToServerComboBox->addItem("Use Current Kit");
    }
    m_ui.sendToLocalComboBox->addItem("Use Custom");
    m_ui.sendToServerComboBox->addItem("Use Custom");

    if (viewInfoResult) addVersionItems("View", viewInfo.versions, m_ui.sendToLocalComboBox);
    if (viewDailyInfoResult) addVersionItems("View Daily", viewDailyInfo.versions, m_ui.sendToLocalComboBox);
    if (createInfoResult) addVersionItems("Create", createInfo.versions, m_ui.sendToLocalComboBox);
    if (createDailyInfoResult) addVersionItems("Create Daily", createDailyInfo.versions, m_ui.sendToLocalComboBox);

    if (kitRemoteInfoResult) addVersionItems("Kit Remote", kitRemoteInfo.versions, m_ui.sendToServerComboBox);
    if (kitRemoteDailyInfoResult) addVersionItems("Kit Remote Daily", kitRemoteDailyInfo.versions, m_ui.sendToServerComboBox);

    if (foundLocalInstalls) this->restoreComboBoxStates(m_ui.sendToLocalComboBox, true);
  }
  else {
    m_ui.sendToLocalComboBox->addItem("Use Custom");
    m_ui.sendToServerComboBox->addItem("Use Custom");

    QMessageBox::critical(this, "Omniverse: Launcher Not Found", "Unable to find View installation information. Please make sure the Omniverse Launcher is running.", QMessageBox::Ok);
  }

  this->show();
}

void pqOmniConnectSettingsDialog::addCustomLocation() {

#ifdef Q_OS_WIN
  QString filters = "Batch files (*.bat);; Executable files (*.exe)";
#else
  QString filters = "Shell scripts (*.sh);; All files (*)";
#endif
  QString fileName = QFileDialog::getOpenFileName(this, "Open File", "", filters);

  if (fileName.length() <= 0) return;

  QPushButton* button = qobject_cast<QPushButton*>(QObject::sender());
  QComboBox* comboBox = button == m_ui.addLocalButton ? m_ui.customViewLocationComboBox : m_ui.kitRemoteLocationComboBox;
  if (comboBox->findText(fileName) < 0) {
    comboBox->addItem(fileName);
    comboBox->setCurrentIndex(comboBox->count() - 1);
  }
}

void pqOmniConnectSettingsDialog::removeCustomLocation() {
  QPushButton* button = qobject_cast<QPushButton*>(QObject::sender());
  QComboBox* comboBox = button == m_ui.removeLocalButton ? m_ui.customViewLocationComboBox : m_ui.kitRemoteLocationComboBox;
  comboBox->removeItem(comboBox->currentIndex());
}

void pqOmniConnectSettingsDialog::pickOutputFolder() {
  pqOmniConnectFolderPickerDialog* dialog = new pqOmniConnectFolderPickerDialog(pqCoreUtilities::mainWidget(), m_settingsManager);
  dialog->exec();

  vtkPVOmniConnectSettings* settings = vtkPVOmniConnectSettings::GetInstance();
  if (settings->GetOutputLocal()) {
    m_ui.projectDirectoryLabel->setText(settings->GetLocalOutputDirectory());
  }
  else {
    m_ui.projectDirectoryLabel->setText(settings->GetOmniWorkingDirectory());
  }

  m_ui.serverTitleLabel->setVisible(!settings->GetOutputLocal());
  m_ui.serverValueLabel->setVisible(!settings->GetOutputLocal());
}

void pqOmniConnectSettingsDialog::addVersionItems(const QString& prefix, const QStringList& versions, QComboBox* comboBox) {
  for (int i = 0; i < versions.size(); i++) {
    QString item = QString("%1 %2").arg(prefix).arg(versions[i]);
    comboBox->addItem(item);
  }
}

void pqOmniConnectSettingsDialog::getAppSlugVersion(const QString& text, QString& outSlug, QString& outVersion) {
  // Converts 'View daily <version>' to 'view-daily'
  //          'View <versio> to 'view'
  QStringList tokens = text.split(" ");
  QString slug = tokens[0].toLower();
  for (int i = 1; i < tokens.size() - 1; i++) {
    slug = QString("%1-%2").arg(slug).arg(tokens[i].toLower());
  }
  outSlug = slug;
  outVersion = tokens[tokens.size() - 1];
}