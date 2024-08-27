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

#include <QVariant>

class pqOmniConnectDataItem
{
public:
	explicit pqOmniConnectDataItem(const QList<QVariant> &data, pqOmniConnectDataItem *parentItem = 0);
	~pqOmniConnectDataItem();

	pqOmniConnectDataItem *child(int number);
	int childCount() const;
	int columnCount() const;
	QVariant data(int column) const;
	bool insertChildren(int position, int count, int columns);
	bool insertChild(int position, pqOmniConnectDataItem* child);
	bool insertColumns(int position, int columns);
	pqOmniConnectDataItem *parent();
	bool removeChildren(int position, int count);
	bool removeColumns(int position, int columns);
	int childNumber() const;
	bool setData(int column, const QVariant &value);

	QString getUrl() const;
	void setUrl(const QString& url);

	// item mapped to local file system vs mapped to Omniverse Nucleus server
	bool isLocal() const;
	void setLocal(bool local);
private:
	QList<pqOmniConnectDataItem*> m_childItems;
	QList<QVariant> m_itemData;
	pqOmniConnectDataItem* m_parentItem;

	QString m_url;
	bool m_local;
};
