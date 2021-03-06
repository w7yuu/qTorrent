/* qTorrent - An open-source, cross-platform BitTorrent client
 * Copyright (C) 2017 Petko Georgiev
 *
 * torrentinfo.cpp
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

#include "torrentinfo.h"
#include "bencodeparser.h"
#include <QFile>
#include <QString>
#include <QCryptographicHash>
#include <QDebug>

void TorrentInfo::clearError()
{
	m_errorString.clear();
}

void TorrentInfo::setError(QString errorString)
{
	m_errorString = errorString;
}

QString TorrentInfo::errorString() const
{
	return m_errorString;
}

TorrentInfo::TorrentInfo()
	: m_length(0)
	, m_pieceLength(0)
	, m_creationDate(nullptr)
	, m_comment(nullptr)
	, m_createdBy(nullptr)
	, m_encoding(nullptr)
	, m_numberOfPieces(0)
{
}

TorrentInfo::~TorrentInfo()
{
	if (m_creationDate != nullptr) delete m_creationDate;
	if (m_comment != nullptr) delete m_comment;
	if (m_createdBy != nullptr) delete m_createdBy;
	if (m_encoding != nullptr) delete m_encoding;
}

bool TorrentInfo::loadFromTorrentFile(QString filename)
{
	m_creationFileName = filename;
	BencodeParser bencodeParser;

	/* Read torrent file */
	if (!bencodeParser.readFile(filename)) {
		setError("Failed to read file " + filename + ": " + bencodeParser.errorString());
		return false;
	}

	/* Parse torrent file */
	if (!bencodeParser.parse()) {
		setError("Failed to parse file " + filename + ": " + bencodeParser.errorString());
		return false;
	}

	try {
		/* Just in case we need to throw stuff */
		BencodeException ex("TorrentInfo::loadFromTorrentFile(): ");

		/* Required parameters */

		// Main list
		QList<BencodeValue *> mainList = bencodeParser.list();

		// The main list must have only one element - the main dictionary
		if (mainList.isEmpty()) {
			throw ex << "Torrent file is empty";
		} else if (mainList.size() > 1) {
			throw ex << "Main list size is " << mainList.size() << ". Expected 1";
		}

		// Main dictionary
		BencodeDictionary *mainDict = mainList.first()->toBencodeDictionary();

		// The Info dictionary
		BencodeDictionary *infoDict = mainDict->value("info")->toBencodeDictionary();

		// Announce URL
		try {
			QList<QByteArray> announceUrlsList;
			QList<BencodeValue*> announceList = mainDict->value("announce-list")->toList();
			for (BencodeValue *announceListValue : announceList) {
				QList<BencodeValue *> announceSubList = announceListValue->toList();
				for (BencodeValue *announceUrl : announceSubList) {
					// [TODO] Support shuffling
					// http://bittorrent.org/beps/bep_0012.html
					announceUrlsList.push_back(announceUrl->toByteArray());
				}
			}
			m_announceUrlsList = announceUrlsList;
		} catch (BencodeException &ex) {
			m_announceUrlsList.clear();
			// Try to find 'announce' key
			try {
				QByteArray url = mainDict->value("announce")->toByteArray();
				m_announceUrlsList.push_back(url);
			} catch(BencodeException &ex) {
				m_announceUrlsList.clear();
			}
		}

		// Torrent name
		m_torrentName = infoDict->value("name")->toByteArray();

		// Piece length
		m_pieceLength = infoDict->value("piece length")->toInt();

		// SHA-1 hash sums of the pieces
		QByteArray pieceData = infoDict->value("pieces")->toByteArray();
		if (pieceData.size() % 20 != 0) {
			throw ex << "Piece data length is not a multiple of 20";
		}
		for (int i = 0; i < pieceData.size();) {
			QByteArray piece;
			for (int j = 0; j < 20; j++) {
				piece.append(pieceData[i++]);
			}
			m_pieces.append(piece);
		}

		// Information about all files in the torrent
		if (infoDict->keyExists("length")) {
			// Single file torrent
			m_length = infoDict->value("length")->toInt();
			FileInfo fileInfo;
			fileInfo.length = m_length;
			fileInfo.path = QList<QString>({m_torrentName});
			m_fileInfos.push_back(fileInfo);
		} else {
			// Multi file torrent
			m_length = 0;
			QList<BencodeValue *> filesList = infoDict->value("files")->toList();
			for (BencodeValue *file : filesList) {
				BencodeDictionary *fileDict = file->toBencodeDictionary();
				FileInfo fileInfo;
				fileInfo.length = fileDict->value("length")->toInt();
				QList<BencodeValue *> pathList = fileDict->value("path")->toList();
				fileInfo.path = QList<QString>({m_torrentName});
				for (auto path : pathList) {
					fileInfo.path.push_back(path->toByteArray());
				}
				m_length += fileInfo.length;
				m_fileInfos.push_back(fileInfo);
			}
		}

		/* Optional parameters */

		// Creation date
		try {
			m_creationDate = new QDateTime;
			qint64 creation = mainDict->value("creation date")->toInt();
			*m_creationDate = QDateTime::fromMSecsSinceEpoch(1000*creation);
		} catch (BencodeException &ex) {
			delete m_creationDate;
			m_creationDate = nullptr;
		}

		// Comment
		try {
			m_comment = new QString;
			*m_comment = mainDict->value("comment")->toByteArray();
		} catch (BencodeException &ex) {
			delete m_comment;
			m_comment = nullptr;
		}

		// Created by
		try {
			m_createdBy = new QString;
			*m_createdBy = mainDict->value("created by")->toByteArray();
		} catch (BencodeException &ex) {
			delete m_createdBy;
			m_createdBy = nullptr;
		}

		// Encoding
		try {
			m_encoding = new QString;
			*m_encoding = mainDict->value("encoding")->toByteArray();
		} catch (BencodeException &ex) {
			// Default to UTF-8 encoding
			delete m_encoding;
			m_encoding = new QString("UTF-8");
		}

		/* Calculate torrent file info hash */
		m_infoHash = QCryptographicHash::hash(infoDict->getRawBencodeData(), QCryptographicHash::Sha1);

		/* Calculate total number of pieces */
		m_numberOfPieces = m_length / m_pieceLength;
		if (m_length % m_pieceLength != 0) {
			m_numberOfPieces++;
		}
	} catch (BencodeException &ex) {
		setError(ex.what());
		return false;
	}
	return true;
}


const QList<QByteArray> &TorrentInfo::announceUrlsList() const
{
	return m_announceUrlsList;
}

qint64 TorrentInfo::length() const
{
	return m_length;
}

const QByteArray &TorrentInfo::torrentName() const
{
	return m_torrentName;
}

qint64 TorrentInfo::pieceLength() const
{
	return m_pieceLength;
}

const QList<QByteArray> &TorrentInfo::pieces() const
{
	return m_pieces;
}

const QByteArray &TorrentInfo::piece(int pieceIndex) const
{
	return m_pieces[pieceIndex];
}

const QList<FileInfo> &TorrentInfo::fileInfos() const
{
	return m_fileInfos;
}

bool TorrentInfo::isSingleFile() const
{
	return (m_fileInfos.size() == 1);
}

const QByteArray &TorrentInfo::infoHash() const
{
	return m_infoHash;
}

const QString &TorrentInfo::creationFileName() const
{
	return m_creationFileName;
}

int TorrentInfo::numberOfPieces() const
{
	return m_numberOfPieces;
}

int TorrentInfo::bitfieldSize() const
{
	int size = m_numberOfPieces / 8;
	if (m_numberOfPieces % 8 != 0) {
		size++;
	}
	return size;
}


const QDateTime *TorrentInfo::creationDate() const
{
	return m_creationDate;
}

const QString *TorrentInfo::comment() const
{
	return m_comment;
}

const QString *TorrentInfo::createdBy() const
{
	return m_createdBy;
}

const QString *TorrentInfo::encoding() const
{
	return m_encoding;
}
