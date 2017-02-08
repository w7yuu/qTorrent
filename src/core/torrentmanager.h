#ifndef TORRENTMANAGER_H
#define TORRENTMANAGER_H

#include "torrentsettings.h"
#include <QList>
#include <QUrl>

class QTorrent;
class Torrent;
class TorrentInfo;

class TorrentManager
{
public:
	TorrentManager(QTorrent* qTorrent);
	~TorrentManager();

	Torrent* addTorrentFromLocalFile(const QString& filename, const TorrentSettings& settings);

	// Loads all saved for resuming torrents
	bool resumeTorrents();

	// Not usable
	Torrent* addTorrentFromMagnetLink(QUrl url);

	// Saves resume info for all torrents
	bool saveTorrentsResumeInfo();
	// Permanently saves the torrent file to the app data directory
	bool saveTorrentFile(const QString& filename, TorrentInfo* torrentInfo);

	bool removeTorrent(Torrent* torrent, bool deleteData);

	/* Getters */
	QTorrent* qTorrent();
	const QList<Torrent*>& torrents() const;

private:
	QTorrent* m_qTorrent;
	QList<Torrent*> m_torrents;
};

#endif // TORRENTMANAGER_H
