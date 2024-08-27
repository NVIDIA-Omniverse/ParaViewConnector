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

#include <QDialog>
#include <QSettings>
#include <QComboBox>
#include <QButtonGroup>

#include "pqOmniConnectViewsSettingsManager.h"
#include "pqOmniConnectPropertyLinks.h"

class pqOmniConnectBaseDialog : public QDialog,  public pqOmniConnectPropertyLinks{
	Q_OBJECT

public:
	pqOmniConnectBaseDialog(QWidget* parent, pqOmniConnectViewsSettingsManager& settingsManager, Qt::WindowFlags f = Qt::WindowFlags{});
	virtual ~pqOmniConnectBaseDialog();

	void tryServerConnect(bool forceReconnect = false, const QString& host = QString::null);

protected:
	pqOmniConnectViewsSettingsManager& m_settingsManager;

	void init();
	bool isConnectionValid() const;
	void pushModifiedSettings();

	void saveComboBoxStates(QComboBox* comboBox, bool saveCurrentTextOnly = false);
	void restoreComboBoxStates(QComboBox* comboBox, bool restoreCurrentTextOnly = false);

	void saveButtonGroupStates(QButtonGroup* buttonGroup);
	void restoreButtonGroupStates(QButtonGroup* buttonGroup);

	void resizeWidgetToContent(QWidget* widget);

	void saveConfig(const QString& group, const QString& key, const QVariant& value);
	
	virtual void onInit(bool editable, pqOmniConnectViewSettings& settings) = 0;

protected slots:
	virtual void onTryConnectFinished(bool success);
  
private:
	QSettings* m_config;
};
