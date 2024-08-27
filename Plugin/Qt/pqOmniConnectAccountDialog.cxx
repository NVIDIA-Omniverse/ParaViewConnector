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

#include "pqOmniConnectAccountDialog.h"

#include <QMessageBox>
#include <QPushButton>

pqOmniConnectAccountDialog::pqOmniConnectAccountDialog(QWidget* p, pqOmniConnectViewsSettingsManager& settingsManager, Qt::WindowFlags f) : 
	pqOmniConnectBaseDialog(p, settingsManager, f){
	m_ui.setupUi(this);
	this->init();

	connect(&m_settingsManager, SIGNAL(onConnectEnd(bool)), this, SLOT(onTryConnectFinished(bool))); 
}
	  
pqOmniConnectAccountDialog::~pqOmniConnectAccountDialog() {
}

void pqOmniConnectAccountDialog::onInit(bool editable, pqOmniConnectViewSettings& settings) {

	connect(m_ui.dialogButtonBox, &QDialogButtonBox::accepted, this, &pqOmniConnectAccountDialog::onAccepted);

	if (editable) {
		this->addComboBoxPropertyLink(m_ui.serverComboBox, "OmniverseServer", pqOmniConnectLinkType::Text);

		this->restoreComboBoxStates(m_ui.serverComboBox);
		if (m_ui.serverComboBox->count() == 0) {
			m_ui.serverComboBox->addItem(settings.OmniServer);
		}
	}
	else {
		m_ui.serverComboBox->setCurrentText(settings.OmniServer);
	}

	m_ui.serverComboBox->setEnabled(editable);
	m_ui.dialogButtonBox->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(editable);
}

void pqOmniConnectAccountDialog::onAccepted() {
	pqOmniConnectBaseDialog::tryServerConnect(true, m_ui.serverComboBox->currentText());
}


void pqOmniConnectAccountDialog::onTryConnectFinished(bool success) {
	if (success) {
		this->setStringPropertyLink("OmniverseServer", m_ui.serverComboBox->currentText().toStdString());
		this->pushModifiedSettings();

		// If server url doesn't exist in the list, add to combobox and local settings
		if (m_ui.serverComboBox->findText(m_ui.serverComboBox->currentText()) < 0) {
			m_ui.serverComboBox->addItem(m_ui.serverComboBox->currentText());
			m_ui.serverComboBox->setCurrentIndex(m_ui.serverComboBox->count() - 1);
		}

		this->saveComboBoxStates(m_ui.serverComboBox);
	}
	else {
		QMessageBox::warning(this, tr("Unknown Server"),
			tr("Omniverse Nucleus could not be reached. Restart Nucleus, Reconnect to your network or Install Nucleus."),
			QMessageBox::Ok);
	}
	this->close();
}