/* qTorrent - An open-source, cross-platform BitTorrent client
 * Copyright (C) 2017 Petko Georgiev
 *
 * torrentslist.cpp
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qtorrent.h"
#include "core/torrent.h"
#include "core/torrentinfo.h"
#include "mainwindow.h"
#include "torrentslist.h"
#include "torrentslistitem.h"
#include "torrentitemdelegate.h"
#include <QHeaderView>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QVariant>

TorrentsList::TorrentsList()
{
	QFontMetrics fm = fontMetrics();
	QStringList headers;
	headers << tr("Torrent") << tr("Size") << tr("Peers")
			<< tr("State") << tr("Progress")
			<< tr("Upload Speed") << tr("Download Speed")
			<< tr("Available") << tr("Left")
			<< tr("Total Downloaded") << tr("Total Uploaded") << tr("Ratio")
			<< tr("Downloaded") << tr("Uploaded");

	QHeaderView *headerView = header();

	setHeaderLabels(headers);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setAlternatingRowColors(true);
	setRootIsDecorated(false);
	setSortingEnabled(true);
	setExpandsOnDoubleClick(true);
	setAcceptDrops(true);
	setContextMenuPolicy(Qt::CustomContextMenu);
	setItemDelegate(new TorrentItemDelegate(this));

	headerView->setSectionsMovable(true);
	headerView->setSectionsClickable(true);
	headerView->setContextMenuPolicy(Qt::CustomContextMenu);

	const int typicalSizeWidth = fm.width(" 987.65 MiB ");

// An easier way to set the width of a section
// Ugly, but easier
#define SECTION_HEADER_WIDTH(Section) fm.width(headers.at(TorrentsListItem::Section) + "  ")
#define BYTES_SECTION_WIDTH(Section) qMax(SECTION_HEADER_WIDTH(Section), typicalSizeWidth)
#define SET_SECTION_WIDTH(Section, width) headerView->resizeSection(TorrentsListItem::Section, width)
#define SET_SECTION_TO_HEADER_WIDTH(Section) SET_SECTION_WIDTH(Section, SECTION_HEADER_WIDTH(Section))
#define SET_BYTES_SECTION_WIDTH(Section) SET_SECTION_WIDTH(Section, BYTES_SECTION_WIDTH(Section))

	SET_SECTION_WIDTH(Name, fm.width("typical-name-for-a-torrent.torrent"));
	SET_BYTES_SECTION_WIDTH(Available);
	SET_SECTION_WIDTH(Peers, fm.width(" 123/456 "));
	SET_SECTION_WIDTH(State, fm.width(" Downloading "));
	SET_SECTION_WIDTH(Progress, SECTION_HEADER_WIDTH(Progress) * 2);

	SET_SECTION_WIDTH(DownloadSpeed, SECTION_HEADER_WIDTH(DownloadSpeed));
	SET_SECTION_WIDTH(UploadSpeed, SECTION_HEADER_WIDTH(UploadSpeed));

	SET_BYTES_SECTION_WIDTH(Available);
	SET_BYTES_SECTION_WIDTH(Left);
	SET_BYTES_SECTION_WIDTH(TotalDownloaded);
	SET_BYTES_SECTION_WIDTH(TotalUploaded);
	SET_SECTION_WIDTH(Ratio, fm.width(" 132.456 "));
	SET_BYTES_SECTION_WIDTH(Downloaded);
	SET_BYTES_SECTION_WIDTH(Uploaded);

	// Don't criticize me

	for (Torrent *torrent : QTorrent::instance()->torrents()) {
		addTorrent(torrent);
	}

	connect(this, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(openContextMenu(QPoint)));
	connect(headerView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(openHeaderContextMenu(QPoint)));
}

TorrentsList::~TorrentsList()
{
}

void TorrentsList::addTorrent(Torrent *torrent)
{
	TorrentsListItem *item = new TorrentsListItem(this, torrent);
	connect(item, &TorrentsListItem::removeTorrent, this, &TorrentsList::removeTorrentSignal);
	m_items.append(item);
	item->refresh();
}

void TorrentsList::removeTorrent(Torrent *torrent)
{
	TorrentsListItem *item = torrentItem(torrent);
	if (item) {
		m_items.removeAll(item);
		removeItemWidget(item, 0);
		delete item;
	}
}

void TorrentsList::refresh()
{
	for (TorrentsListItem *item : m_items) {
		item->refresh();
	}
}

void TorrentsList::openContextMenu(const QPoint &pos)
{
	TorrentsListItem *item = dynamic_cast<TorrentsListItem *>(itemAt(pos));

	if (item == nullptr) {
		// This shouldn't happen at all
		return;
	}

	QAction *openAct = new QAction(tr("Open"), this);
	QAction *openLocationAct = new QAction(tr("Open containing folder"), this);
	QAction *pauseAct = new QAction(tr("Pause"), this);
	QAction *startAct = new QAction(tr("Start"), this);
	QAction *stopAct = new QAction(tr("Stop"), this);
	QAction *recheckAct = new QAction(tr("Recheck"), this);
	QAction *removeAct = new QAction(tr("Remove"), this);

	Torrent *torrent = item->torrent();
	if (!torrent->torrentInfo()->isSingleFile() || !torrent->isDownloaded()) {
		openAct->setEnabled(false);
	}
	if (torrent->isPaused()) {
		pauseAct->setEnabled(false);
	}
	if (!torrent->isPaused() && torrent->state() == Torrent::Started) {
		startAct->setEnabled(false);
	}
	if (torrent->state() == Torrent::Stopped) {
		pauseAct->setEnabled(false);
		stopAct->setEnabled(false);
	}

	QMenu menu(this);

	menu.addAction(openAct);
	menu.addAction(openLocationAct);
	menu.addSeparator();
	menu.addAction(pauseAct);
	menu.addAction(startAct);
	menu.addAction(stopAct);
	menu.addAction(recheckAct);
	menu.addAction(removeAct);

	connect(openAct, SIGNAL(triggered()), item, SLOT(onOpenAction()));
	connect(openLocationAct, SIGNAL(triggered()), item, SLOT(onOpenLocationAction()));
	connect(pauseAct, SIGNAL(triggered()), item, SLOT(onPauseAction()));
	connect(startAct, SIGNAL(triggered()), item, SLOT(onStartAction()));
	connect(stopAct, SIGNAL(triggered()), item, SLOT(onStopAction()));
	connect(recheckAct, SIGNAL(triggered()), item, SLOT(onRecheckAction()));
	connect(removeAct, SIGNAL(triggered()), item, SLOT(onRemoveAction()));

	menu.exec(mapToGlobal(pos));
}

void TorrentsList::openHeaderContextMenu(const QPoint &pos)
{
	QList<QAction *> actions;
	QMenu menu(header());
	for (int i = 0; i < header()->count(); i++) {
		QString label = header()->model()->headerData(i, Qt::Horizontal).toString();
		QAction *action = new QAction(label, &menu);
		action->setCheckable(true);
		action->setChecked(!header()->isSectionHidden(i));
		menu.addAction(action);
		actions.append(action);
	}
	menu.exec(mapToGlobal(pos));
	for (int i = 0; i < header()->count(); i++) {
		header()->setSectionHidden(i, !actions[i]->isChecked());
	}
}

void TorrentsList::showAll()
{
	for (TorrentsListItem *item : m_items) {
		item->setHidden(false);
	}
}

void TorrentsList::showCompleted()
{
	for (TorrentsListItem *item : m_items) {
		bool downloaded = item->torrent()->isDownloaded();
		item->setHidden(!downloaded);
	}
}

void TorrentsList::showDownloading()
{
	for (TorrentsListItem *item : m_items) {
		bool downloaded = item->torrent()->isDownloaded();
		item->setHidden(downloaded);
	}
}

void TorrentsList::showUploading()
{
	for (TorrentsListItem *item : m_items) {
		item->setHidden(false);
	}
}


TorrentsListItem *TorrentsList::torrentItem(Torrent *torrent)
{
	for (TorrentsListItem *item : m_items) {
		if (item->torrent() == torrent) {
			return item;
		}
	}
	return nullptr;
}

TorrentsListItem *TorrentsList::torrentItem(const QString &name)
{
	for (TorrentsListItem *item : m_items) {
		if (item->text(TorrentsListItem::Name) == name) {
			return item;
		}
	}
	return nullptr;
}

Torrent *TorrentsList::currentTorrent()
{
	TorrentsListItem *item = static_cast<TorrentsListItem *>(currentItem());
	if (!item) {
		return nullptr;
	}
	return item->torrent();
}

void TorrentsList::dragEnterEvent(QDragEnterEvent *event)
{
	event->acceptProposedAction();
}

void TorrentsList::dragMoveEvent(QDragMoveEvent *event)
{
	event->acceptProposedAction();
}

void TorrentsList::dropEvent(QDropEvent *event)
{
	const QMimeData *mimeData = event->mimeData();
	if (mimeData->hasUrls()) {
		QList<QUrl> urlList = mimeData->urls();
		for (QUrl url : urlList) {
			MainWindow::instance()->addTorrentFromUrl(url);
		}
	}
}
