#ifndef PEER_H
#define PEER_H

#include <QByteArray>

class Peer {
	QByteArray m_address;
	int m_port;
public:
	Peer(const QByteArray& address, int port);
	~Peer();
	QByteArray& address();
	int port();
};

#endif // PEER_H