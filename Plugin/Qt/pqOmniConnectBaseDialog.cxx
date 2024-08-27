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

#include "pqOmniConnectBaseDialog.h"
#include "pqOmniConnectUtils.h"
#include "pqOmniConnectLogger.h"

#include <QSettings>
#include <QVariant>
#include <QAbstractButton>

pqOmniConnectBaseDialog::pqOmniConnectBaseDialog(QWidget* parent, pqOmniConnectViewsSettingsManager& settingsManager, Qt::WindowFlags f)
	: QDialog(parent, f),
	pqOmniConnectPropertyLinks(),
	m_settingsManager(settingsManager){

	setAttribute(Qt::WA_DeleteOnClose);
	setModal(true);

	m_config = new QSettings(pqOmniConnectUtils::StandardPaths::settingsFilePath(), QSettings::IniFormat);
}

pqOmniConnectBaseDialog::~pqOmniConnectBaseDialog() {
	delete m_config;
}

void pqOmniConnectBaseDialog::tryServerConnect(bool forceReconnect, const QString& host) {
	if (!this->isConnectionValid() || forceReconnect) {
		pqOmniConnectViewSettings tempSettings = m_settingsManager.getSettings();

		// Overwrites server url when specified. Otherwise use the server url from current settings
		if (!host.isNull()) {
			tempSettings.OmniServer = host;
		}
		tempSettings.OutputLocal = false; // attempt to connect to Nucleus
		tempSettings.CreateNewOmniSession = false; // don't create new session folder

		OMNI_LOG_INFO("Connecting to Nucleus at %s ...", tempSettings.OmniServer.toStdString().c_str());

		// this->onTryConnectFinished will be called when m_settingsManager's onConnectEnd signal is emitted
		m_settingsManager.connectServer(tempSettings);
	}
	else {
		onTryConnectFinished(true);
	}
}

void pqOmniConnectBaseDialog::onTryConnectFinished(bool success) {
	// subclass can reimplement this function
	Q_UNUSED(success);
}

void pqOmniConnectBaseDialog::init() {
	m_settingsManager.updateCurrentSettings();
	QString activeView = m_settingsManager.getActiveViewName();
	pqOmniConnectViewSettings settings = m_settingsManager.getSettings();
	bool editable = activeView.isNull();
	onInit(editable, settings);
}

bool pqOmniConnectBaseDialog::isConnectionValid() const {
	return m_settingsManager.getConnectionValid();
}

void pqOmniConnectBaseDialog::pushModifiedSettings() {
	// Update the VTK object on the server by pushing the values of all modified properties
	this->getOmniConnectSettingsProxy()->UpdateVTKObjects();
	m_settingsManager.updateCurrentSettings();
}

void pqOmniConnectBaseDialog::saveComboBoxStates(QComboBox* comboBox, bool saveCurrentTextOnly) {
	Q_ASSERT(m_config != nullptr);
	m_config->beginGroup(comboBox->objectName());
	if (saveCurrentTextOnly) {
		m_config->setValue("value", comboBox->currentText());
	}
	else {
		QVariantList values;
		for (int i = 0; i < comboBox->count(); i++) {
			values.append(comboBox->itemText(i));
		}
		m_config->setValue("values", values);
		m_config->setValue("index", comboBox->currentIndex());
	}
	m_config->endGroup();
}

void pqOmniConnectBaseDialog::restoreComboBoxStates(QComboBox* comboBox, bool restoreCurrentTextOnly) {
	Q_ASSERT(m_config != nullptr);
	m_config->beginGroup(comboBox->objectName());
	if (restoreCurrentTextOnly) {
		QString currentText = m_config->value("value").toString();
		if (comboBox->findText(currentText) >= 0) {
			comboBox->setCurrentText(currentText);
		}
	}
	else {
		QVariantList values = m_config->value("values").toList();
		int index = m_config->value("index").toInt();

		comboBox->clear();
		for (int i = 0; i < values.size(); i++) {
			comboBox->addItem(values[i].toString());
		}
		comboBox->setCurrentIndex(index);
	}

	m_config->endGroup();
}

void pqOmniConnectBaseDialog::saveButtonGroupStates(QButtonGroup* buttonGroup) {
	Q_ASSERT(m_config != nullptr);
	m_config->beginGroup(buttonGroup->objectName());
	m_config->setValue("checked", buttonGroup->checkedId());
	m_config->endGroup();
}
void pqOmniConnectBaseDialog::restoreButtonGroupStates(QButtonGroup* buttonGroup) {
	Q_ASSERT(m_config != nullptr);
	m_config->beginGroup(buttonGroup->objectName());
	int checkedId = m_config->value("checked").toInt();
	buttonGroup->button(checkedId)->setChecked(true);
	m_config->endGroup();
}

void pqOmniConnectBaseDialog::saveConfig(const QString& group, const QString& key, const QVariant& value) {
	Q_ASSERT(m_config != nullptr);
	m_config->beginGroup(group);
	m_config->setValue(key, value);
	m_config->endGroup();
}

void pqOmniConnectBaseDialog::resizeWidgetToContent(QWidget* widget) {
	widget->resize(widget->width(), widget->minimumHeight());
}