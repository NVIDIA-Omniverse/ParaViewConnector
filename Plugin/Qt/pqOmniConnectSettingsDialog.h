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

#include "ui_pqOmniConnectSettingsDialog.h"
#include "pqOmniConnectBaseDialog.h"
#include "pqOmniConnectLauncherClient.h"

#include <QButtonGroup>

class pqOmniConnectSettingsDialog : public pqOmniConnectBaseDialog
{
  Q_OBJECT

public:
  pqOmniConnectSettingsDialog(QWidget* p, pqOmniConnectViewsSettingsManager& settingsManager, Qt::WindowFlags f = Qt::WindowFlags{});
  virtual ~pqOmniConnectSettingsDialog();

  void setButtonConfiguration(bool saveCancel); // The alternative is just the save button with "Create" as text
  void setOpenViewBoxVisibility(bool visible);
  void resizeLayoutToContent();
  void checkDataAndShow();

protected:
  virtual void onInit(bool editable, pqOmniConnectViewSettings& settings);

private:
  Ui::pqOmniConnectSettingsDialog m_ui;
  QButtonGroup* m_radioGroup;
  pqOmniConnectLauncherClient* m_omniLauncherClient;
  bool m_editable;

  void initOpenViewGroup(bool editable, const pqOmniConnectViewSettings& settings);
  void initConnectionGroup(bool editable, const pqOmniConnectViewSettings& settings);
  void initUSDGroup(bool editable, const pqOmniConnectViewSettings& settings);
  void initLoggingGroup(bool editable, const pqOmniConnectViewSettings& settings);

  void addVersionItems(const QString& prefix, const QStringList& versions, QComboBox* comboBox);
  void getAppSlugVersion(const QString& text, QString& outSlug, QString& outVersion);
  void applyOpenViewSettings(bool onlyUpdateLatestPaths);

private slots:
  void onSave();
  void onReject();
  void onOmniLauncherAppInfoRequested(bool success);
  void addCustomLocation();
  void removeCustomLocation();
  void pickOutputFolder();
};