/******************************************************************************
	hotplugwatcher_gui.cpp: gui application to watch hotplug event
    Copyright (C) 2012-2015 Wang Bin <wbsecg1@gmail.com>
	
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


#include "hotplugwatcher_gui.h"

#include <QtCore/QThread>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QToolButton>
#include <QSystemTrayIcon>
#include <QMessageBox>

#include "qdevicewatcher.h"

HotplugWatcher_GUI::HotplugWatcher_GUI(QWidget *parent) :
	QWidget(parent)
{
	QVBoxLayout *vbl = new QVBoxLayout(this);
	setLayout(vbl);

	state = new QLabel(this);
	button = new QPushButton(tr("Start"), this);
	detail_button = new QToolButton(this);
	detail_button->setText(tr("Detail"));
	detail_button->setCheckable(true);
	detail_button->setChecked(true);
	msg_view = new QTextBrowser(this);
	msg_view->setLineWrapMode(QTextEdit::NoWrap);

	QHBoxLayout *hbl = new QHBoxLayout(this);
	hbl->addWidget(button);
	hbl->addWidget(detail_button);

	vbl->addWidget(state);
	vbl->addLayout(hbl);
	vbl->addWidget(msg_view);

	tray = new QSystemTrayIcon(this);
	tray->show();

	watcher = new QDeviceWatcher;
	watcher->appendEventReceiver(this);
	connect(watcher, SIGNAL(deviceAdded(QString)), this, SLOT(slotDeviceAdded(QString)), Qt::DirectConnection);
	connect(watcher, SIGNAL(deviceChanged(QString)), this, SLOT(slotDeviceChanged(QString)), Qt::DirectConnection);
	connect(watcher, SIGNAL(deviceRemoved(QString)), this, SLOT(slotDeviceRemoved(QString)), Qt::DirectConnection);

	connect(button, SIGNAL(clicked()), SLOT(toggleWatch()));
	connect(detail_button, SIGNAL(toggled(bool)), SLOT(showDetail(bool)));
}

HotplugWatcher_GUI::~HotplugWatcher_GUI()
{

}

void HotplugWatcher_GUI::appendMessage(const QString &msg)
{
	msg_view->append(msg);
}

void HotplugWatcher_GUI::toggleWatch()
{
	if (watcher->isRunning()) {
		if (!watcher->stop()) {
			QMessageBox::warning(0, tr("WARNGING"), tr("Failed to stop"));
		}
		button->setText(tr("Start"));
	} else {
		if (!watcher->start()) {
			QMessageBox::warning(0, tr("WARNGING"), tr("Failed to start"));
		}
		button->setText(tr("Stop"));
	}
}

void HotplugWatcher_GUI::showDetail(bool show)
{
	msg_view->setVisible(show);
	int w = width();
	adjustSize();  //width changes
	resize(w, height());
}

void HotplugWatcher_GUI::slotDeviceAdded(const QString &dev)
{
    qDebug("tid=%#x: add %s", (quintptr) QThread::currentThreadId(), qPrintable(dev));

	state->setText("<font color=#0000ff>Add: </font>" + dev);
	tray->showMessage(tr("New device"), dev);
}

void HotplugWatcher_GUI::slotDeviceChanged(const QString &dev)
{
    qDebug("tid=%#x: change %s", (quintptr) QThread::currentThreadId(), qPrintable(dev));

	state->setText("<font color=#0000ff>Change: </font>" + dev);
	tray->showMessage(tr("Change device"), dev);
}

void HotplugWatcher_GUI::slotDeviceRemoved(const QString &dev)
{
    qDebug("tid=%#x: remove %s", (quintptr) QThread::currentThreadId(), qPrintable(dev));

	state->setText("<font color=#0000ff>Remove: </font>" + dev);
	tray->showMessage(tr("Remove device"), dev);
}
