/******************************************************************************
  QDeviceWatcherPrivate: watching depends on platform
  Copyright (C) 2011-2015 Wang Bin <wbsecg1@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/

#include "qdevicewatcher.h"
#include "qdevicewatcher_p.h"
#ifdef Q_OS_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#else
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)

#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <errno.h>
#include <unistd.h>

#include <QtCore/QCoreApplication>
#include <QtCore/qregexp.h>
#if CONFIG_SOCKETNOTIFIER
#include <QtCore/QSocketNotifier>
#elif CONFIG_TCPSOCKET
#include <QtNetwork/QTcpSocket>
#endif //CONFIG_SOCKETNOTIFIER
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define UEVENT_BUFFER_SIZE      2048

enum udev_monitor_netlink_group {
	UDEV_MONITOR_NONE,
	UDEV_MONITOR_KERNEL,
	UDEV_MONITOR_UDEV
};

QDeviceWatcherPrivate::~QDeviceWatcherPrivate()
{
	stop();
	close(netlink_socket);
	netlink_socket = -1;
}

bool QDeviceWatcherPrivate::start()
{
	if (!init())
		return false;
#if CONFIG_SOCKETNOTIFIER
	socket_notifier->setEnabled(true);
#elif CONFIG_TCPSOCKET
	connect(tcp_socket, SIGNAL(readyRead()), SLOT(parseDeviceInfo()));
#else
	this->QThread::start();
#endif
	return true;
}

bool QDeviceWatcherPrivate::stop()
{
	if (netlink_socket!=-1) {
#if CONFIG_SOCKETNOTIFIER
		socket_notifier->setEnabled(false);
#elif CONFIG_TCPSOCKET
		//tcp_socket->close(); //how to restart?
		disconnect(this, SLOT(parseDeviceInfo()));
#else
		this->quit();
#endif
		close(netlink_socket);
		netlink_socket = -1;
	}
	return true;
}

ssize_t QDeviceWatcherPrivate::readsockets(int socketdev, QBuffer *pqBuffer, bool bResetReadPoint, bool bReplaceCR2Zero) {
    //QByteArray data;
    ssize_t len;
#if CONFIG_SOCKETNOTIFIER
    //socket_notifier->setEnabled(false); //for win
    m_qBasisData.resize(65536);
    m_qBasisData.fill(0);
    if(bResetReadPoint) lseek(socketdev, 0, SEEK_SET); //zetta: for file
    len = read(socketdev, m_qBasisData.data(), UEVENT_BUFFER_SIZE*2);
    zDebug("read fro socket %ld bytes", len);
    m_qBasisData.resize(static_cast<int>(len));
    //socket_notifier->setEnabled(true); //for win
#elif CONFIG_TCPSOCKET
    data = tcp_socket->readAll();
#endif
    if(bReplaceCR2Zero) m_qBasisData = m_qBasisData.replace(0, '\n').trimmed(); //In the original line each information is seperated by 0
    if(pqBuffer->isOpen()) pqBuffer->close();
    pqBuffer->setBuffer(&m_qBasisData);
    return len;
}

QSet <QString> QDeviceWatcherPrivate::readlines(int socketdev, QBuffer *pqBuffer) {
    QSet <QString> strMountsCur;
    readsockets(socketdev, pqBuffer, true);
    pqBuffer->open(QIODevice::ReadOnly);
    while(!pqBuffer->atEnd()) { //buffer.canReadLine() always false?
        strMountsCur << pqBuffer->readLine().trimmed();
    }
    pqBuffer->close();
    return strMountsCur;
}

void QDeviceWatcherPrivate::parseDeviceInfo() {
    //zDebug("%s active", qPrintable(QTime::currentTime().toString()));
	QByteArray data;
    ssize_t len = readsockets(static_cast<int>(socket_notifier->socket()), &buffer, false);
    if(len == 0) { buffer.close(); return; }
	buffer.open(QIODevice::ReadOnly);
#if 0
    bool bPartition = false;
    while(!buffer.atEnd()) { //buffer.canReadLine() always false?
        //parseLine(buffer.readLine().trimmed());
        if(buffer.readLine().trimmed().contains("partition")) {
            bPartition = true;
            break;
        }
    }
    if(!bPartition) { buffer.close(); return; }
    buffer.seek(0);
#endif
	while(!buffer.atEnd()) { //buffer.canReadLine() always false?
        QByteArray oneline = buffer.readLine().trimmed();
        parseLine(oneline);
        parseScsiDevice(oneline);
	}
	buffer.close();
}

void QDeviceWatcherPrivate::parseMountInfo()
{//zDebug("%s active", qPrintable(QTime::currentTime().toString()));
    QByteArray data;
    QSet <QString> strMountsCur;

    strMountsCur = readlines(static_cast<int>(socket_notifier->socket()), &buffer);

    for(QString item: strMountsCur-strMounts) {
        QByteArray line=item.toLocal8Bit();
        int nB = line.indexOf(' ', 0);
        if(nB==-1) break;
        int nE = line.indexOf(' ', nB + 1);
        if(nE==-1) break;
        item = line.left(nE).right(nE-nB-1);
        emitDeviceAdded(item);
    }
    for(QString item: strMounts-strMountsCur) {
        QByteArray line=item.toLocal8Bit();
        int nB = line.indexOf(' ', 0);
        if(nB==-1) break;
        int nE = line.indexOf(' ', nB + 1);
        if(nE==-1) break;
        item = line.left(nE).right(nE-nB-1);
        emitDeviceRemoved(item);
    }

    strMounts = strMountsCur;
}

#if CONFIG_THREAD
//another thread
void QDeviceWatcherPrivate::run()
{
	QByteArray data;
	//loop only when event happens. because of recv() block the function?
	while (1) {
		//char buf[UEVENT_BUFFER_SIZE*2] = {0};
		//recv(d->netlink_socket, &buf, sizeof(buf), 0);
		data.resize(UEVENT_BUFFER_SIZE*2);
		data.fill(0);
		size_t len = recv(netlink_socket, data.data(), data.size(), 0);
		zDebug("read fro socket %d bytes", len);
		data.resize(len);
		data = data.replace(0, '\n').trimmed();
		if (buffer.isOpen())
			buffer.close();
		buffer.setBuffer(&data);
		buffer.open(QIODevice::ReadOnly);
		QByteArray line = buffer.readLine();
		while(!line.isNull()) {
			parseLine(line.trimmed());
			line = buffer.readLine();
		}
		buffer.close();
	}
}
#endif //CONFIG_THREAD

/**
 * Create new udev monitor and connect to a specified event
 * source. Valid sources identifiers are "udev" and "kernel".
 *
 * Applications should usually not connect directly to the
 * "kernel" events, because the devices might not be useable
 * at that time, before udev has configured them, and created
 * device nodes.
 *
 * Accessing devices at the same time as udev, might result
 * in unpredictable behavior.
 *
 * The "udev" events are sent out after udev has finished its
 * event processing, all rules have been processed, and needed
 * device nodes are created.
 **/

bool QDeviceWatcherPrivate::init()
{
#if 1
	struct sockaddr_nl snl;
	const int buffersize = 16 * 1024 * 1024;
	int retval;

	memset(&snl, 0x00, sizeof(struct sockaddr_nl));
	snl.nl_family = AF_NETLINK;
	snl.nl_groups = UDEV_MONITOR_KERNEL;

	netlink_socket = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	//netlink_socket = socket(PF_NETLINK, SOCK_DGRAM|SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT); //SOCK_CLOEXEC may be not available
	if (netlink_socket == -1) {
		qWarning("error getting socket: %s", strerror(errno));
		return false;
	}

	/* set receive buffersize */
	setsockopt(netlink_socket, SOL_SOCKET, SO_RCVBUFFORCE, &buffersize, sizeof(buffersize));
	retval = bind(netlink_socket, (struct sockaddr*) &snl, sizeof(struct sockaddr_nl));
	if (retval < 0) {
		qWarning("bind failed: %s", strerror(errno));
		close(netlink_socket);
		netlink_socket = -1;
		return false;
	} else if (retval == 0) {
		//from libudev-monitor.c
		struct sockaddr_nl _snl;
		socklen_t _addrlen;

		/*
		 * get the address the kernel has assigned us
		 * it is usually, but not necessarily the pid
		 */
		_addrlen = sizeof(struct sockaddr_nl);
		retval = getsockname(netlink_socket, (struct sockaddr *)&_snl, &_addrlen);
		if (retval == 0)
			snl.nl_pid = _snl.nl_pid;
	}
#if CONFIG_SOCKETNOTIFIER
	socket_notifier = new QSocketNotifier(netlink_socket, QSocketNotifier::Read, this);
	connect(socket_notifier, SIGNAL(activated(int)), SLOT(parseDeviceInfo())); //will always active
	socket_notifier->setEnabled(false);
#elif CONFIG_TCPSOCKET
	//QAbstractSocket *socket = new QAbstractSocket(QAbstractSocket::UnknownSocketType, this); //will not detect "remove", why?
	tcp_socket = new QTcpSocket(this); //works too
	if (!tcp_socket->setSocketDescriptor(netlink_socket, QAbstractSocket::ConnectedState)) {
		qWarning("Failed to assign native socket to QAbstractSocket: %s", qPrintable(tcp_socket->errorString()));
		delete tcp_socket;
		return false;
	}
#endif
#else
    netlink_socket = open("/proc/mounts", O_RDONLY);
    socket_notifier = new QSocketNotifier(netlink_socket, QSocketNotifier::Write, this);
    connect(socket_notifier, SIGNAL(activated(int)), SLOT(parseMountInfo())); //will always active
    socket_notifier->setEnabled(false);
    strMounts = readlines(static_cast<int>(socket_notifier->socket()), &buffer);
#endif
	return true;
}

void QDeviceWatcherPrivate::parseLine(const QByteArray &line)
{
    zDebug("-> %s", line.constData());
 #define USE_REGEXP 0
#if USE_REGEXP
	QRegExp rx("(\\w+)(?:@/.*/block/.*/)(\\w+)\\W*");
	//QRegExp rx("(add|remove|change)@/.*/block/.*/(\\w+)\\W*");
	if (rx.indexIn(line) == -1)
		return;
	QString action_str = rx.cap(1).toLower();
	QString dev = "/dev/" + rx.cap(2);
#else    
    if (!line.contains("/block/")) //hotplug
        return;
    QString action_str = line.left(line.indexOf('@')).toLower();
	QString dev = "/dev/" + line.right(line.length() - line.lastIndexOf('/') - 1);
#endif //USE_REGEXP
    QDeviceChangeEvent *event = nullptr;

	if (action_str==QLatin1String("add")) {
		emitDeviceAdded(dev);
		event = new QDeviceChangeEvent(QDeviceChangeEvent::Add, dev);
	} else if (action_str==QLatin1String("remove")) {
		emitDeviceRemoved(dev);
		event = new QDeviceChangeEvent(QDeviceChangeEvent::Remove, dev);
	} else if (action_str==QLatin1String("change")) {
		emitDeviceChanged(dev);
		event = new QDeviceChangeEvent(QDeviceChangeEvent::Change, dev);
	}

	zDebug("%s %s", qPrintable(action_str), qPrintable(dev));

    if (event != nullptr && !event_receivers.isEmpty()) {
		foreach(QObject* obj, event_receivers) {
			QCoreApplication::postEvent(obj, event, Qt::HighEventPriority);
		}
	}
}

void QDeviceWatcherPrivate::parseScsiDevice(const QByteArray &line)
{
    zDebug("-> %s", line.constData());
    if (!line.contains("/scsi_generic/")) //hotplug
        return;
    QString action_str = line.left(line.indexOf('@')).toLower();
    QString dev = "/dev/" + line.right(line.length() - line.lastIndexOf('/') - 1);
    QDeviceChangeEvent *event = nullptr;

    if (action_str==QLatin1String("add")) {
        emitScsiAdded(dev);
        event = new QDeviceChangeEvent(QDeviceChangeEvent::Add, dev);
    }

    zDebug("%s (scsi) %s", qPrintable(action_str), qPrintable(dev));

    if (event != nullptr && !event_receivers.isEmpty()) {
        foreach(QObject* obj, event_receivers) {
            QCoreApplication::postEvent(obj, event, Qt::HighEventPriority);
        }
    }
}


//SUBSYSTEM=scsi_generic"

#endif //Q_OS_LINUX
