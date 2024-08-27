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

#include <QToolBar>
#include <QWidget>

class OmniConnect;
class vtkSMProperty;
class pqOmniConnectViewsSettingsManager;
class pqOmniConnectViewOpener;
class pqOmniConnectFileLogger;
class pqView;

class pqOmniConnectActions : public QToolBar
{
  Q_OBJECT
public:
  pqOmniConnectActions(QWidget* p);
  ~pqOmniConnectActions();

private:
  QAction* addToolbarAction(const QString& iconPath, const QString& actionName, const char* slotFunc, bool checkable = false);
  void setViewIntProperty(const char* propertyName, int value);

private slots:
  void onOpenViewClicked();
  void onSignInClicked();
  void onSettingsClicked();
  void onLiveWorkflowClicked();
  void onHelpClicked();
  void onAboutClicked();

  void onViewCreated(pqView* view);
  void onActiveViewChanged(pqView* view);

protected:
  pqOmniConnectViewsSettingsManager* m_settingsManager;
  pqOmniConnectViewOpener* m_viewOpener;
  pqOmniConnectFileLogger* m_fileLogger;
  QAction* m_liveWorkflowAction;
  pqView* m_activeOmniView;
};
