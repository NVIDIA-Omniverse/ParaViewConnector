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

#include "pqOmniConnectFolderPickerDialog.h"

#include "pqOmniConnectSettingsDialog.h"
#include "pqOmniConnectFolderPickerTreeModel.h"
#include "pqOmniConnectUtils.h"
#include "pqOmniConnectLogger.h"

#include <QDirIterator>
#include <QMessageBox>

#include "vtkPVOmniConnectProxy.h"

pqOmniConnectFolderPickerDialog::pqOmniConnectFolderPickerDialog(QWidget* p, pqOmniConnectViewsSettingsManager& settingsManager, Qt::WindowFlags f)
  : pqOmniConnectBaseDialog(p, settingsManager, f),
	m_omniRootItem(nullptr)
{
  m_ui.setupUi(this);
  this->init();
}
	  
pqOmniConnectFolderPickerDialog::~pqOmniConnectFolderPickerDialog()
{
	delete m_treeModel;
}

void pqOmniConnectFolderPickerDialog::onInit(bool editable, pqOmniConnectViewSettings& settings) {
	m_treeModel = new pqOmniConnectFolderPickerTreeModel("");
	m_ui.treeView->setRootIsDecorated(false);
	m_ui.treeView->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
	m_ui.treeView->setModel(m_treeModel);

	m_ui.tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Interactive);
	m_ui.tableWidget->verticalHeader()->setVisible(false);

	m_ui.pathLineEdit->setReadOnly(true);
	m_ui.projectFolderLineEdit->setReadOnly(true);

	connect(m_ui.cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
	connect(m_ui.treeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &pqOmniConnectFolderPickerDialog::onTreeCurrentItemChanged);
	connect(m_ui.reloadButton, SIGNAL(clicked()), this, SLOT(loadContent()));
	connect(m_ui.rightArrowButton, SIGNAL(clicked()), this, SLOT(selectFirstChild()));
	connect(m_ui.leftArrowButton, SIGNAL(clicked()), this, SLOT(selectParent()));
	connect(m_ui.addButton, SIGNAL(clicked()), this, SLOT(createFolder()));
	connect(m_ui.okButton, SIGNAL(clicked()), this, SLOT(applySettings()));

	m_ui.widget->setEnabled(editable);
	m_ui.widget_2->setEnabled(editable);
	m_ui.widget_3->setEnabled(editable);
	m_ui.projectFolderLineEdit->setEnabled(editable);
	m_ui.descriptionLineEdit->setEnabled(editable);
	m_ui.okButton->setEnabled(editable);

	m_outputLocal = settings.OutputLocal;
	if (m_outputLocal) {
		m_projectFolder = settings.LocalOutputDirectory;
	}
	else {
		m_projectFolder = settings.OmniWorkingDirectory;
	}

	// also hide description field for now until checkpoint is supported
	m_ui.label_4->setVisible(false);
	m_ui.descriptionLineEdit->setVisible(false);

	if (editable) {
		pqOmniConnectBaseDialog::tryServerConnect();
		this->loadContent();
	}
	else {
		if (settings.OutputLocal) {
			QString projectDirectory = QString("%1/Session_%2").arg(settings.LocalOutputDirectory).arg(settings.SessionNumber);
			m_ui.pathLineEdit->setText(projectDirectory);
			m_ui.projectFolderLineEdit->setText(projectDirectory);
		}
		else {
			QString projectDirectory = QString("%1/Session_%2").arg(settings.OmniWorkingDirectory).arg(settings.SessionNumber);
			m_ui.pathLineEdit->setText(projectDirectory);
			m_ui.projectFolderLineEdit->setText(projectDirectory);
		}

		// Set guide label
		QString activeView = m_settingsManager.getActiveViewName();
		QString guide = QString("An Omniverse Connector View's output directory is immutable after its creation. Please deselect %1 to modify the output directory for a new view").arg(activeView);
		m_ui.guideLabel->setText(guide);
	}

	// show/hide widgets accordingly
	m_ui.guideLabel->setVisible(!editable);
	m_ui.widget->setVisible(editable);
	m_ui.widget_2->setVisible(editable);
	m_ui.widget_3->setVisible(editable);
	pqOmniConnectBaseDialog::resizeWidgetToContent(this);
}

void pqOmniConnectFolderPickerDialog::applySettings() {
	this->setIntPropertyLink("OutputLocal", m_outputLocal ? 1 : 0);
	if (m_outputLocal) {
		this->setStringPropertyLink("OmniverseWorkingDirectory", "");
		this->setStringPropertyLink("LocalOutputDirectory", m_projectFolder.toStdString());
	}
	else {
		this->setStringPropertyLink("OmniverseWorkingDirectory", m_projectFolder.toStdString());
		this->setStringPropertyLink("LocalOutputDirectory", "");

		m_settingsManager.disconnectServer(); // Working dir changed, so make sure connection is invalidated (to be restored at parent dialog's confirm)
	}

	OMNI_LOG_INFO("USD scene files output directory set to %s", m_projectFolder.toStdString().c_str());

	//this->pushModifiedSettings(); // don't save settings to m_current yet, in case parent dialog cancels
	this->accept();
}

void pqOmniConnectFolderPickerDialog::selectFirstChild() {
	// Choose root item if nothing is selected
	if (!m_ui.treeView->selectionModel()->hasSelection()) {
		QModelIndex rootIndex = m_treeModel->indexForTreeItem(m_treeModel->getRootItem());
		m_ui.treeView->selectionModel()->setCurrentIndex(rootIndex.child(0, 0), QItemSelectionModel::Select);
		return;
	}

	QModelIndex currentIndex = m_ui.treeView->selectionModel()->currentIndex();
	QModelIndex childIndex = currentIndex.child(0, 0);
	if (childIndex.isValid()) {
		m_ui.treeView->selectionModel()->setCurrentIndex(currentIndex, QItemSelectionModel::Deselect);
		m_ui.treeView->selectionModel()->setCurrentIndex(childIndex, QItemSelectionModel::Select);
	}
}

void pqOmniConnectFolderPickerDialog::selectParent() {
	QModelIndex currentIndex = m_ui.treeView->selectionModel()->currentIndex();
	QModelIndex parentIndex = currentIndex.parent();
	if (parentIndex.isValid()) {
		m_ui.treeView->selectionModel()->setCurrentIndex(currentIndex, QItemSelectionModel::Deselect);
		m_ui.treeView->selectionModel()->setCurrentIndex(parentIndex, QItemSelectionModel::Select);
	}
}

void pqOmniConnectFolderPickerDialog::createFolder() {
	if (!m_ui.treeView->selectionModel()->hasSelection()) {
		QMessageBox::warning(this, "", "Select a folder to create the new folder under", QMessageBox::Ok);
		return;
	}
	if (m_ui.folderNameLineEdit->text().length() == 0) {
		QMessageBox::warning(this, "", "Please enter a folder name", QMessageBox::Ok);
		return;
	}

	QModelIndex currentIndex = m_ui.treeView->selectionModel()->currentIndex();
	pqOmniConnectDataItem* item = m_treeModel->getItem(currentIndex);

	if (item->isLocal()) {
		// Create folder on local file system
		QDir parentDir(m_ui.projectFolderLineEdit->text());
		if (parentDir.mkpath(m_ui.folderNameLineEdit->text())) {
			onTreeCurrentItemChanged(currentIndex, currentIndex);
		}
		else {
			QMessageBox::warning(this, "", "Folder creation failed", QMessageBox::Ok);
		}
	}
	else {
		// Create folder on Onmiverse Nucleus
		QString path = QString("omniverse://%1%2/%3").arg(this->m_settingsManager.getSettings().OmniServer).arg(m_ui.projectFolderLineEdit->text()).arg(m_ui.folderNameLineEdit->text());
		if (vtkPVOmniConnectProxy::CreateFolder(this->m_settingsManager.getConnector(), path.toStdString().c_str())) 
		{
			onTreeCurrentItemChanged(currentIndex, currentIndex);
		}
		else {
			QMessageBox::warning(this, "", "Folder creation failed", QMessageBox::Ok);
		}
	}
}

void pqOmniConnectFolderPickerDialog::onTreeCurrentItemChanged(const QModelIndex &current, const QModelIndex &previous)
{
	Q_UNUSED(previous);

	// Load and expand the current item
	loadContentForParent(current);
	m_ui.treeView->expand(current);

	pqOmniConnectDataItem* item = m_treeModel->getItem(current);
	m_outputLocal = item->isLocal();
	if (item->isLocal()) {
		QString path = item->getUrl();
		m_projectFolder = path.replace("//", "/"); // replace double slash if any
		m_ui.pathLineEdit->setText(m_projectFolder);
		m_ui.projectFolderLineEdit->setText(m_projectFolder);
	}
	else {
		const pqOmniConnectViewSettings& settings = m_settingsManager.getSettings();
		QString prefix = QString("omniverse://%1").arg(settings.OmniServer);
		QString path = item->getUrl().replace(prefix, "Omniverse");
		m_ui.pathLineEdit->setText(path);

		m_projectFolder = path.replace("Omniverse", "");
		m_ui.projectFolderLineEdit->setText(m_projectFolder);
	}
}

void pqOmniConnectFolderPickerDialog::loadContentForParent(const QModelIndex& parentIndex) {
	pqOmniConnectDataItem* item = m_treeModel->getItem(parentIndex);

	// Repopulate children of the selected item by clearing them first
	if (item->parent() != nullptr) {
		m_treeModel->removeRows(0, item->childCount(), parentIndex);
	}

	pqOmniConnectUtils::Spinner::setState(true);

	// Retrieve folders and files
	std::vector<std::string> folderList;
	std::vector<PickerFileInfo> fileList;
	getFilesAndFolders(item, folderList, fileList);

	pqOmniConnectUtils::Spinner::setState(false);

	// Populate TreeView on the left
	for (int i = 0; i < folderList.size(); i++) {
		QString folderUrl = QString(folderList[i].c_str());
		pqOmniConnectDataItem* folderItem = m_treeModel->addItem(folderUrl, item, parentIndex);
		folderItem->setUrl(folderUrl);
		folderItem->setLocal(item->isLocal());
	}

	// Populate table on the right
	m_ui.tableWidget->setRowCount(0);
	for (int i = 0; i < fileList.size(); i++) {
		int row = m_ui.tableWidget->rowCount();
		m_ui.tableWidget->insertRow(row);
		QTableWidgetItem* urlItem = new QTableWidgetItem(QString::fromStdString(fileList[i].url));
		urlItem->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
		m_ui.tableWidget->setItem( row, 0, urlItem);

		QTableWidgetItem* timeItem = new QTableWidgetItem(formatTime(fileList[i].modifiedTime));
		timeItem->setFlags(Qt::ItemFlag::ItemIsEnabled);
		m_ui.tableWidget->setItem( row, 1, timeItem);

		QTableWidgetItem* authorItem = new QTableWidgetItem(QString::fromStdString(fileList[i].author));
		authorItem->setFlags(Qt::ItemFlag::ItemIsEnabled);
		m_ui.tableWidget->setItem( row, 2, authorItem);
	}
}

void pqOmniConnectFolderPickerDialog::getFilesAndFolders(const pqOmniConnectDataItem* item, std::vector<std::string>& folderList, std::vector<PickerFileInfo>& fileList) {
	// local file system
	if (item->isLocal()) {
		getLocalFilesAndFolders(item->getUrl(), folderList, fileList);
	}
	// file system on Omniverse Nucleus
	else {
		getOmniFilesAndFolders(item->getUrl(), folderList, fileList);
	}
}

QString pqOmniConnectFolderPickerDialog::formatTime(time_t time) {
	std::tm * ptm = std::localtime(&time);
	char buffer[30];
	std::strftime(buffer, 30, "%m/%d/%Y %H:%M:%S", ptm);
	return QString(buffer);
}

void pqOmniConnectFolderPickerDialog::getLocalFilesAndFolders(const QString& searchPath, std::vector<std::string>& folderList, std::vector<PickerFileInfo>& fileList) {
	// Get folders
	QDir::Filters dirFilters = QDir::Dirs | QDir::NoDotAndDotDot;
	QDirIterator it(searchPath, dirFilters, QDirIterator::NoIteratorFlags);
	while (it.hasNext()) {
		QFileInfo fileInfo(it.next());
		folderList.push_back(fileInfo.fileName().toStdString());
	}

	// Get files
	QDirIterator fileIt(searchPath, {"*.usd", "*.usda"}, QDir::Files);
	while (fileIt.hasNext()) {
		QFileInfo fileInfo(fileIt.next());
		PickerFileInfo info;
		info.url = fileInfo.fileName().toStdString();
		info.author = fileInfo.owner().toStdString();
		info.modifiedTime = fileInfo.lastModified().toTime_t();
		fileList.push_back(info);
	}
}

void pqOmniConnectFolderPickerDialog::getOmniFilesAndFolders(const QString& searchPath, std::vector<std::string>& folderList, std::vector<PickerFileInfo>& fileList) 
{
	if (!pqOmniConnectBaseDialog::isConnectionValid()) 
	{
		return;
	}
	
	vtkSmartPointer<vtkPVOmniConnectProxyUrlInfo> pvUrlInfo;
  	pvUrlInfo.TakeReference(vtkPVOmniConnectProxyUrlInfo::New());
	vtkPVOmniConnectProxy::GetUrlInfoList(m_settingsManager.getConnector(), searchPath.toStdString().c_str(), pvUrlInfo.Get());

	OmniConnectUrlInfo* urlInfos = pvUrlInfo->GetUrlInfoList().infos;
	//split the retrived data into folders and files
	for (int i = 0; i < pvUrlInfo->GetUrlInfoList().size; i++) {
		std::string urlT = urlInfos[i].Url;
		if (urlT.find("/", 0) == 0) // trim the leading /
			urlT = urlT.substr(1);
		if (urlT.find("/.thumbs/") == std::string::npos) {
			if (urlInfos[i].IsFile) {
				PickerFileInfo info;
				info.url = urlInfos[i].Url;
				info.author = urlInfos[i].Author;
				info.modifiedTime = urlInfos[i].ModifiedTime;
				fileList.push_back(std::move(info));
			}
			else {
				folderList.push_back(std::move(urlT)); // just the folder name
			}
		}
	}
}

void pqOmniConnectFolderPickerDialog::loadContent() {

	m_ui.treeView->selectionModel()->clearSelection();

	// Omniverse root
	if (!m_omniRootItem && m_settingsManager.getOmniClientEnabled() && pqOmniConnectBaseDialog::isConnectionValid()) {
		m_omniRootItem = m_treeModel->addItem("Omniverse", nullptr);

		const pqOmniConnectViewSettings& settings = m_settingsManager.getSettings();
		QString url = QString("omniverse://%1").arg(settings.OmniServer);
		m_omniRootItem->setUrl(url);
		m_omniRootItem->setLocal(false);
	}

	if (m_localRootItems.size() == 0) {
		QFileInfoList drives = QDir::drives();
		for (int i = 0; i < drives.size(); i++) {
			QString drivePath = drives[i].path();
			std::string temp = drivePath.toStdString();
			pqOmniConnectDataItem* driveItem = m_treeModel->addItem(drivePath, nullptr);
			driveItem->setUrl(drivePath);
			driveItem->setLocal(true);

			m_localRootItems.push_back(driveItem);
		}
	}

	for (int i = 0; i < m_localRootItems.size(); i++) {
		QModelIndex driveIndex = m_treeModel->indexForTreeItem(m_localRootItems[i]);
		loadContentForParent(driveIndex);
	}

	if (m_omniRootItem && m_settingsManager.getOmniClientEnabled() && pqOmniConnectBaseDialog::isConnectionValid()) {
		QModelIndex omniRootIndex = m_treeModel->indexForTreeItem(m_omniRootItem);
		loadContentForParent(omniRootIndex);

		// Expand the Omniverse item
		m_ui.treeView->expand(omniRootIndex);
	}
}