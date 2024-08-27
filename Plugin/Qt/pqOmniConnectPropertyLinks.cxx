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

#include "pqOmniConnectPropertyLinks.h"
#include "pqOmniConnectLogger.h"

#include "pqActiveObjects.h"
#include "pqApplicationCore.h"
#include "pqServer.h"
#include "pqSettings.h"
#include "pqSignalAdaptors.h"
#include "pqComboBoxDomain.h"

#include "vtkSMTrace.h"
#include "vtkSMProxy.h"
#include "vtkSMProperty.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSettings.h"
#include "vtkSMPropertyIterator.h"
#include "vtkPVXMLElement.h"
#include "vtkSMDocumentation.h"
#include "vtkPVOmniConnectSMPipelineController.h"

#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>

#include <QNetworkReply>

namespace
{
  void OmniAddQWidgetTooltip(QWidget* widget, vtkSMProperty* prop)
  {
    vtkSMDocumentation* documentation = prop->GetDocumentation();
    if (widget && documentation && documentation->GetDescription())
    {
      widget->setToolTip(QString(documentation->GetDescription()));
    }
  }
}

pqOmniConnectPropertyLinks::pqOmniConnectPropertyLinks() 
{
	// Get proxy for Omniverse connect settings
	pqServer* server = pqActiveObjects::instance().activeServer();
	m_proxy = server->proxyManager()->GetProxy(OMNI_PROXY_GROUP_NAME, OMNI_SETTINGS_PROXY_NAME);

	if (!m_proxy) OMNI_LOG_ERROR("`%s` ProxyGroup not found. Omniverse connector may not have loaded correctly on the connected pvserver.", OMNI_PROXY_GROUP_NAME);

	Q_ASSERT(m_proxy != nullptr);
	SM_SCOPED_TRACE(PropertiesModified).arg("proxy", m_proxy);
}

pqOmniConnectPropertyLinks::~pqOmniConnectPropertyLinks() {
}

vtkSMProxy* pqOmniConnectPropertyLinks::getOmniConnectSettingsProxy() const {
	return m_proxy;
}

void pqOmniConnectPropertyLinks::saveOmniConnectSettings(bool saveAll) {
	// Note to modify xml to save certain properties only
	// https://public.kitware.com/pipermail/paraview/2014-June/031331.html
	vtkSMSettings* settings = vtkSMSettings::GetInstance();
	pqSettings* qSettings = pqApplicationCore::instance()->settings();
	settings->SetProxySettings(m_proxy);
	vtkSmartPointer<vtkSMPropertyIterator> iter;
	iter.TakeReference(m_proxy->NewPropertyIterator());
	for (iter->Begin(); !iter->IsAtEnd(); iter->Next())
	{
		vtkSMProperty* smproperty = iter->GetProperty();
		if (saveAll || (smproperty && smproperty->GetHints() &&
			smproperty->GetHints()->FindNestedElementByName("SaveInQSettings")))
		{
			QString key = QString("%1.%2").arg(OMNI_SETTINGS_PROXY_NAME).arg(iter->GetKey());
			qSettings->saveInQSettings(key.toLocal8Bit().data(), smproperty);
		}
	}
}

void pqOmniConnectPropertyLinks::addPropertyLink(QObject* qObject, const char* propertyName) 
{
	if(!m_proxy)
		return;

	vtkSMProperty* prop = m_proxy->GetProperty(propertyName);
	Q_ASSERT(prop != nullptr);

  QWidget* linkedWidget = nullptr;

	if (QComboBox* comboBox = qobject_cast<QComboBox*>(qObject)) 
  {
		m_links.addPropertyLink(qObject, "currentText", SIGNAL(currentIndexChanged(const QString&)), m_proxy, prop);
    linkedWidget = comboBox;
	}
	else if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(qObject)) 
  {
		m_links.addPropertyLink(qObject, "text", SIGNAL(textChanged(const QString&)), m_proxy, prop);
    linkedWidget = lineEdit;
	}
  else if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(qObject)) 
  {
		m_links.addPropertyLink(qObject, "checked", SIGNAL(stateChanged(int)), m_proxy, prop);
    linkedWidget = checkBox;
	}

  OmniAddQWidgetTooltip(linkedWidget, prop);

	// to do: add links for other ui elements
}

void pqOmniConnectPropertyLinks::addComboBoxPropertyLink(QObject* qObject, const char* propertyName, pqOmniConnectLinkType linkType) {
	vtkSMProperty* prop = m_proxy->GetProperty(propertyName);
	Q_ASSERT(prop != nullptr);

	QComboBox* comboBox = qobject_cast<QComboBox*>(qObject);
	Q_ASSERT(comboBox != nullptr);

	new pqComboBoxDomain(comboBox, prop);
	pqSignalAdaptorComboBox* adaptor = new pqSignalAdaptorComboBox(comboBox);

	switch (linkType) {
	case pqOmniConnectLinkType::Text:
		m_links.addPropertyLink(adaptor, "currentText", SIGNAL(currentTextChanged(QString)), m_proxy, prop);
		break;
	case pqOmniConnectLinkType::Index:
		m_links.addPropertyLink(adaptor, "currentData", SIGNAL(currentTextChanged(QString)), m_proxy, prop);
		break;
	default:
		break;
	}

  OmniAddQWidgetTooltip(comboBox, prop);
}

void pqOmniConnectPropertyLinks::setIntPropertyLink(const char* propertyName, int value) {
	vtkSMProperty* prop = m_proxy->GetProperty(propertyName);
	Q_ASSERT(prop != nullptr);

	vtkSMIntVectorProperty* vectorProp = vtkSMIntVectorProperty::SafeDownCast(prop);
	Q_ASSERT(vectorProp != nullptr);
	vectorProp->SetElement(0, value); // assume 1 element for now
	m_proxy->UpdateVTKObjects();
}

void pqOmniConnectPropertyLinks::setStringPropertyLink(const char* propertyName, std::string value) {
	vtkSMProperty* prop = m_proxy->GetProperty(propertyName);
	Q_ASSERT(prop != nullptr);

	vtkSMStringVectorProperty* vectorProp = vtkSMStringVectorProperty::SafeDownCast(prop);
	Q_ASSERT(vectorProp != nullptr);
	vectorProp->SetElement(0, value.c_str()); // assume 1 element for now
	m_proxy->UpdateVTKObjects();
}