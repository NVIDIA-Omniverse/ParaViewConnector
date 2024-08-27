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

#include "pqOmniConnectDataItem.h"

pqOmniConnectDataItem::pqOmniConnectDataItem(const QList<QVariant> &data, pqOmniConnectDataItem *parentItem)
{
	m_url = QString::null;
	m_parentItem = parentItem;
	m_itemData = data;
	m_local = false;
}

pqOmniConnectDataItem::~pqOmniConnectDataItem()
{
	qDeleteAll(m_childItems);
}

pqOmniConnectDataItem* pqOmniConnectDataItem::child(int row) {
	if (row < 0 || row >= m_childItems.size())
		return nullptr;
	return m_childItems.at(row);
}

int pqOmniConnectDataItem::childCount() const {
	return m_childItems.count();
}

int pqOmniConnectDataItem::columnCount() const {
	return m_itemData.count();
}

QVariant pqOmniConnectDataItem::data(int column) const {
	if (column < 0 || column >= m_itemData.size())
		return QVariant();
	return m_itemData.at(column);
}

bool pqOmniConnectDataItem::insertChildren(int position, int count, int columns) {
	Q_UNUSED(columns);
	if (position < 0 || position > m_childItems.size())
		return false;

	for (int row = 0; row < count; ++row) {
		//QList<QVariant> data (columns);
		QList<QVariant> data;
		pqOmniConnectDataItem* item = new pqOmniConnectDataItem(data, this);
		m_childItems.insert(position, item);
	}

	return true;
}

bool pqOmniConnectDataItem::insertChild(int position, pqOmniConnectDataItem* child){
	if (position < 0 || position > m_childItems.size())
		return false;
	m_childItems.insert(position, child);
	return true;
}

bool pqOmniConnectDataItem::insertColumns(int position, int columns) {
	if (position < 0 || position > m_itemData.size())
		return false;

	for (int column = 0; column < columns; ++column)
		m_itemData.insert(position, QVariant());

	for (pqOmniConnectDataItem* child : qAsConst(m_childItems))
		child->insertColumns(position, columns);

	return true;
}

pqOmniConnectDataItem* pqOmniConnectDataItem::parent() {
	return m_parentItem;
}

bool pqOmniConnectDataItem::removeChildren(int position, int count) {
	if (position < 0 || position + count > m_childItems.size())
		return false;

	for (int row = 0; row < count; ++row)
		delete m_childItems.takeAt(position);

	return true;
}

bool pqOmniConnectDataItem::removeColumns(int position, int columns) {
	if (position < 0 || position + columns > m_itemData.size())
		return false;

	for (int column = 0; column < columns; ++column)
		m_itemData.removeAt(position);

	for (pqOmniConnectDataItem *child : qAsConst(m_childItems))
		child->removeColumns(position, columns);

	return true;
}

int pqOmniConnectDataItem::childNumber() const {
	if (m_parentItem)
		return m_parentItem->m_childItems.indexOf(const_cast<pqOmniConnectDataItem*>(this));
	return 0;
}

bool pqOmniConnectDataItem::setData(int column, const QVariant &value) {
	if (column < 0 || column >= m_itemData.size())
		return false;

	m_itemData[column] = value;
	return true;
}

QString pqOmniConnectDataItem::getUrl() const {
	QString fullUrl = m_url;
	if (m_parentItem && !m_parentItem->getUrl().isNull()) {
		fullUrl = QString("%1/%2").arg(m_parentItem->getUrl()).arg(m_url);
	}
	return fullUrl;
}

void pqOmniConnectDataItem::setUrl(const QString& url) {
	m_url = url;
}

bool pqOmniConnectDataItem::isLocal() const {
	return m_local;
}

void pqOmniConnectDataItem::setLocal(bool local) {
	m_local = local;
}