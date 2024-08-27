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

#include "ui_pqOmniConnectFolderPickerDialog.h"

#include "pqOmniConnectBaseDialog.h"

class pqOmniConnectFolderPickerTreeModel;
class pqOmniConnectDataItem;

class pqOmniConnectFolderPickerDialog : public pqOmniConnectBaseDialog
{
  Q_OBJECT

public: 
	pqOmniConnectFolderPickerDialog(QWidget* p, pqOmniConnectViewsSettingsManager& settingsManager, Qt::WindowFlags f = Qt::WindowFlags{});
  virtual ~pqOmniConnectFolderPickerDialog();

protected:
	struct PickerFileInfo
	{
		std::string url;
		std::string author;
		time_t modifiedTime;
	}; 
	virtual void onInit(bool editable, pqOmniConnectViewSettings& settings);

private slots:
	void loadContent();
	void applySettings();
	void selectFirstChild();
	void selectParent();
	void createFolder();
	void onTreeCurrentItemChanged(const QModelIndex &current, const QModelIndex &previous);

private:
	void getFilesAndFolders(const pqOmniConnectDataItem* item, std::vector<std::string>& folderList, std::vector<PickerFileInfo>& fileList);
	void getLocalFilesAndFolders(const QString& searchPath, std::vector<std::string>& folderList, std::vector<PickerFileInfo>& fileList);
	void getOmniFilesAndFolders(const QString& searchPath, std::vector<std::string>& folderList, std::vector<PickerFileInfo>& fileList);
	
	void loadContentForParent(const QModelIndex& parentIndex);
	QString formatTime(time_t time);

	pqOmniConnectFolderPickerTreeModel* m_treeModel;
	Ui::pqOmniConnectFolderPickerDialog m_ui;

	pqOmniConnectDataItem* m_omniRootItem;
	std::vector<pqOmniConnectDataItem*> m_localRootItems;

	bool m_outputLocal;
	QString m_projectFolder;
};