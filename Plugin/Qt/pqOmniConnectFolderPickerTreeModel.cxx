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

#include "pqOmniConnectFolderPickerTreeModel.h"

pqOmniConnectFolderPickerTreeModel::pqOmniConnectFolderPickerTreeModel(const QString &data, QObject *parent):
	QAbstractItemModel(parent)
{
	QList<QVariant> rootData;
	rootData << data;
	m_rootItem = new pqOmniConnectDataItem(rootData);
}

pqOmniConnectFolderPickerTreeModel::~pqOmniConnectFolderPickerTreeModel()
{
	delete m_rootItem;
}

QVariant pqOmniConnectFolderPickerTreeModel::data(const QModelIndex &index, int role) const {
	if (!index.isValid())
		return QVariant();

	if (role != Qt::DisplayRole && role != Qt::EditRole)
		return QVariant();

	pqOmniConnectDataItem *item = getItem(index);

	return item->data(index.column());
}

QVariant pqOmniConnectFolderPickerTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return m_rootItem->data(section);

	return QVariant();
}

QModelIndex pqOmniConnectFolderPickerTreeModel::index(int row, int column,
	const QModelIndex &parent) const {
	if (parent.isValid() && parent.column() != 0)
		return QModelIndex();

	pqOmniConnectDataItem *parentItem = getItem(parent);
	if (!parentItem)
		return QModelIndex();

	pqOmniConnectDataItem *childItem = parentItem->child(row);
	if (childItem)
		return createIndex(row, column, childItem);
	return QModelIndex();
}

QModelIndex pqOmniConnectFolderPickerTreeModel::parent(const QModelIndex &index) const {
	if (!index.isValid())
		return QModelIndex();

	pqOmniConnectDataItem *childItem = getItem(index);
	pqOmniConnectDataItem *parentItem = childItem ? childItem->parent() : nullptr;

	if (parentItem == m_rootItem || !parentItem)
		return QModelIndex();

	return createIndex(parentItem->childNumber(), 0, parentItem);
}

int pqOmniConnectFolderPickerTreeModel::rowCount(const QModelIndex &parent) const {
	if (parent.isValid() && parent.column() > 0)
		return 0;

	const pqOmniConnectDataItem *parentItem = getItem(parent);

	return parentItem ? parentItem->childCount() : 0;
}

int pqOmniConnectFolderPickerTreeModel::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return m_rootItem->columnCount();
}

Qt::ItemFlags pqOmniConnectFolderPickerTreeModel::flags(const QModelIndex &index) const {
	if (!index.isValid())
		return 0;

	return QAbstractItemModel::flags(index);
}

bool pqOmniConnectFolderPickerTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
	if (role != Qt::EditRole)
		return false;

	pqOmniConnectDataItem *item = getItem(index);
	bool result = item->setData(index.column(), value);

	if (result)
		emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

	return result;
}
bool pqOmniConnectFolderPickerTreeModel::setHeaderData(int section, Qt::Orientation orientation,
	const QVariant &value, int role) {
	if (role != Qt::EditRole || orientation != Qt::Horizontal)
		return false;

	const bool result = m_rootItem->setData(section, value);

	if (result)
		emit headerDataChanged(orientation, section, section);

	return result;
}

bool pqOmniConnectFolderPickerTreeModel::insertColumns(int position, int columns,
	const QModelIndex &parent) {
	beginInsertColumns(parent, position, position + columns - 1);
	const bool success = m_rootItem->insertColumns(position, columns);
	endInsertColumns();

	return success;
}

bool pqOmniConnectFolderPickerTreeModel::removeColumns(int position, int columns,
	const QModelIndex &parent) {
	beginRemoveColumns(parent, position, position + columns - 1);
	const bool success = m_rootItem->removeColumns(position, columns);
	endRemoveColumns();

	if (m_rootItem->columnCount() == 0)
		removeRows(0, rowCount());

	return success;
}

bool pqOmniConnectFolderPickerTreeModel::insertRows(int position, int rows,
	const QModelIndex &parent) {
	pqOmniConnectDataItem *parentItem = getItem(parent);
	if (!parentItem)
		return false;

	beginInsertRows(parent, position, position + rows - 1);
	const bool success = parentItem->insertChildren(position,
		rows, m_rootItem->columnCount());
	endInsertRows();

	return success;
}

bool pqOmniConnectFolderPickerTreeModel::removeRows(int position, int rows,
	const QModelIndex &parent) {
	pqOmniConnectDataItem *parentItem = getItem(parent);
	if (!parentItem || rows <= 0)
		return false;

	beginRemoveRows(parent, position, position + rows - 1);
	const bool success = parentItem->removeChildren(position, rows);
	endRemoveRows();

	return success;
}

pqOmniConnectDataItem* pqOmniConnectFolderPickerTreeModel::addItem(const QString& text, pqOmniConnectDataItem* parent, const QModelIndex& parentIndex) {
	// if parent is null we use m_rootItem as parent
	parent = parent == nullptr ? m_rootItem : parent;

	QList<QVariant> data;
	data << text;
	pqOmniConnectDataItem* element = new pqOmniConnectDataItem(data, parent);

	beginInsertRows(parentIndex, parent->childCount(), parent->childCount());
	parent->insertChild(parent->childCount(), element);
	endInsertRows();
	return element;
}

pqOmniConnectDataItem* pqOmniConnectFolderPickerTreeModel::getItem(const QModelIndex &index) const {
	if (index.isValid()) {
		pqOmniConnectDataItem *item = static_cast<pqOmniConnectDataItem*>(index.internalPointer());
		if (item) return item;
	}
	return m_rootItem;
}

QModelIndex pqOmniConnectFolderPickerTreeModel::indexForTreeItem(pqOmniConnectDataItem* item)
{
	return createIndex(item->childNumber(), 0, item);
}

pqOmniConnectDataItem* pqOmniConnectFolderPickerTreeModel::getRootItem() const{
	return m_rootItem;
}