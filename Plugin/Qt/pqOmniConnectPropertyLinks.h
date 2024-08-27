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

#include "pqPropertyLinks.h"

// Used by combo box to determine whether to link with current text or current index
enum class pqOmniConnectLinkType {
	Text = 0,
	Index = 1
};

/** 
/* A wrapper class for pqPropertyLinks to synchroze settings between
/* ParaView client and server
**/
class pqOmniConnectPropertyLinks {
public:
	pqOmniConnectPropertyLinks();
	~pqOmniConnectPropertyLinks();

protected:
	vtkSMProxy* getOmniConnectSettingsProxy() const;
	void saveOmniConnectSettings(bool saveAll);
	void addPropertyLink(QObject* qObject, const char* propertyName);
	void addComboBoxPropertyLink(QObject* qObject, const char* propertyName, pqOmniConnectLinkType linkType);
	
	void setIntPropertyLink(const char* propertyName, int value);
	void setStringPropertyLink(const char* propertyName, std::string value);

private:
	vtkSMProxy* m_proxy;
	pqPropertyLinks m_links;
};