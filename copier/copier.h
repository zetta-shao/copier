#ifndef COPIER_H
#define COPIER_H

#include <QtCore/QObject>
#include <QtCore/QThread>
#include "../src/qdevicewatcher.h"
#ifdef Q_OS_LINUX
#include <libudev.h>
#endif

struct tThreadParam {
    int     nFlag;
    char    pcParam[256];
};

enum { PlugNone, PlugAdd, PlugChange, PlugRemove };

class Copier : public QThread {
    Q_OBJECT

public:
    Copier(QThread *parent=nullptr);
    ~Copier(void);
    void    DoExec(const QString& strDEV);
    void    CheckExist(void);
    void    MountableAdded(const QString &dev, const QString &tgtdir, const QString &fstype);
    void    MountableDelete(const QString &dev, const QString &tgtdir, const QString &fstype);
    void    ScriptExec(const QString &dev, const QString &tgtdir, const QString &fstype, const QString &ScsiDev);
    QString         m_strDevName;
    QString         m_strUUID;
    QString         m_strFStype;
    QString         m_strScsiDevice;
    QString         m_qNameExec;
    //QString       m_qFileCheck;

public slots:
    void    slotDeviceAdded(const QString& dev);
    void    slotDeviceRemoved(const QString& dev);
    void    slotDeviceChanged(const QString& dev);
    void    slotScsiAdded(const QString& dev);

protected:
    //virtual bool event(QEvent *e);

signals:
    void    __DoExec(const QString& strDEV);
    void    slotMountableAdded(const QString &dev, const QString &tgtdir, const QString &fstype);
    void    slotMountableDelete(const QString &dev, const QString &tgtdir, const QString &fstype);
    void    slotScriptExec(const QString &dev, const QString &tgtdir, const QString &fstype, const QString &ScsiDev);

private:
    QDeviceWatcher *watcher;
    int         m_nState;
};

#endif // COPIER_H
