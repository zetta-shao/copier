#ifndef QDEVWDT_H
#define QDEVWDT_H

#include <QtCore/QObject>
#include <QtCore/QThread>
#include "../src/qdevicewatcher.h"

#ifndef __GNUC__
#define __PRETTY_FUNCTION__  __FUNCTION__
#endif

enum { PlugNone, PlugIn, PlugOut, PlugChange };

class qDevWgt : public QThread {
    Q_OBJECT

public:
    qDevWgt(QThread *parent=nullptr);
    void    SetFileExec(QString str) { m_qFileExec=str; }
    void    SetFileCheck(QString str) { m_qFileCheck=str; }
    void    DoExec(const QString& strDEV);
    void    CheckExist(void);

public slots:
    void slotDeviceAdded(const QString& dev);
    void slotDeviceRemoved(const QString& dev);
    void slotDeviceChanged(const QString& dev);

protected:
    //virtual bool event(QEvent *e);
signals:
    void        __DoExec(const QString& strDEV);

private:
    QDeviceWatcher *watcher;
    QString     m_qFileExec;
    QString     m_qFileCheck;
    int         m_nState;
};

#endif // QDEVWDT_H
