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

#include <QObject>
#include <QMap>

#include "OmniConnectData.h"
#include "vtkSmartPointer.h"

class pqView;
class vtkCallbackCommand;
class vtkObject;
class vtkSMProxy;
class pqOmniConnectViewsSettingsManagerCallbacks;

enum class pqOmniConnectViewAxis
{
  X = 0,
  Y = 1,
  Z = 2
};

struct pqOmniConnectViewSettings {
  // Connection options
  QString OmniServer;           // Omniverse server to connect to
  QString OmniWorkingDirectory; // Base directory in which the PV session directories are created, containing scene + actor usd files and assets
  QString LocalOutputDirectory; // Directory for local output (used in case OutputLocal is enabled)
  bool OutputLocal;                 // Output to OmniLocalDirectory instead of the Omniverse
  bool CreateNewOmniSession;        // Find a new Omniverse session directory on creation of the connector, or re-use the last opened one.

  // USD options
  bool OutputBinary;                // Output binary usd files instead of text-based usda
  pqOmniConnectViewAxis UpAxis;           // Up axis of USD output
  bool UsePointInstancer;           // Either use UsdGeomPointInstancer for point data, or otherwise UsdGeomPoints
  bool UseStickLines;               // Either use the UsdGeomBasisCurves prim (curves) or UsdGeomPointInstancer with cylinders (sticks)
  bool UseStickWireframe;           // either use the UsdGeomBasisCurves prim (curves) or UsdGeomPointInstancer with cylinders (sticks)
  bool UseMeshVolume;               // Represent volumes as regular textured meshes

  // log options
  bool ShowOmniErrors;
  bool ShowOmniDebug;
  int NucleusVerbosity;

  // number of data partitions
  int NumPartitions;

  // Session Number mapped to this view
  int SessionNumber;

  // User name
  QString UserName;

  // Live workflow
  bool LiveWorkflowEnabled;

  // Utility function to get path to *.usd or *.usda
  QString getUsdPath() const;

  // Whether this settings and other settings have the same root output directory (ignoring session number)
  bool hasSameRootOutputDirectory(const pqOmniConnectViewSettings& otherSettings) const;
};

/**
* Manager class to keep track of OV connector view states and settings used
* at the time of each view's creation.
**/
class pqOmniConnectViewsSettingsManager : public QObject {
  Q_OBJECT

public:
  pqOmniConnectViewsSettingsManager(QObject* parent = nullptr);
  ~pqOmniConnectViewsSettingsManager();

  QString getActiveViewName();
  bool isOmniViewActive();
  bool isOmniView(pqView* view);
  pqOmniConnectViewSettings getSettings();
  void updateCurrentSettings();

  void connectServer(const pqOmniConnectViewSettings& settings, bool openConnection = true);
  void disconnectServer();
  void resetConnectionState();
  void resetOmniClientState();

  void setLiveWorkflowEnabled(bool enabled);

  bool isSettingsInvalid(QString& conflictingViewOut) const;

  vtkSMProxy* getConnector() const { return m_connector; }
  bool getConnectionValid() const { return m_connector && m_connectionValid; }
  bool getOmniClientEnabled() const { return m_omniClientEnabled; }

  void processViewEvents(vtkObject* obj, unsigned long eventid, void* clientData);

protected:
  void cacheConnectorViewSettings(const char* key, int numPartitions);
  void findConnectorViewSessionInfo(QString& userNameOut, int& sessionNumberOut);

private slots:
  void onViewCreated(pqView* view);
  void connectServerFinished(bool success);

Q_SIGNALS:
  void onConnectEnd(bool success);

private:
  // Used to process events
  vtkSmartPointer<pqOmniConnectViewsSettingsManagerCallbacks> vtkCallbacks;
  	
  vtkSMProxy* m_connector = nullptr;
  bool m_connectionValid = false;
  bool m_omniClientEnabled = false;

  QMap<QString, pqOmniConnectViewSettings> m_viewsSettings;
  pqOmniConnectViewSettings m_currentSettings;
};