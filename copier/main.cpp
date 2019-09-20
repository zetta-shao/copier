#include <QCoreApplication>

#include <QFile>
#include <QDebug>
#include "copier.h"
#include <libudev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <unistd.h>

//static struct tThreadParam g_tThreadParam = { 0, "" };

void ThreadExec(void *pvParam) {
    struct tThreadParam *tobj = static_cast<tThreadParam*>(pvParam);

    if((tobj->nFlag&1)==1) return;
    tobj->nFlag |= 1;

    tobj->nFlag &= ~1;
}

Copier::Copier(QThread *parent) : QThread(parent) {
    start();
    moveToThread(this); //Let bool event(QEvent *e) be in another thread
    watcher = new QDeviceWatcher;
    watcher->moveToThread(this);
    watcher->appendEventReceiver(this);
    connect(watcher, SIGNAL(deviceAdded(QString)), this, SLOT(slotDeviceAdded(QString)), Qt::DirectConnection);
    connect(watcher, SIGNAL(deviceChanged(QString)), this, SLOT(slotDeviceChanged(QString)), Qt::DirectConnection);
    connect(watcher, SIGNAL(deviceRemoved(QString)), this, SLOT(slotDeviceRemoved(QString)), Qt::DirectConnection);
    connect(watcher, SIGNAL(scsiAdded(QString)), this, SLOT(slotScsiAdded(QString)), Qt::DirectConnection);
    connect(this, &Copier::__DoExec, this, &Copier::DoExec, Qt::DirectConnection);
    connect(this, &Copier::slotMountableAdded, this, &Copier::MountableAdded);
    connect(this, &Copier::slotMountableDelete, this, &Copier::MountableDelete);
    connect(this, &Copier::slotScriptExec, this, &Copier::ScriptExec);
    //m_ptUdev = udev_new();
    watcher->start();
    //CheckExist();
    m_nState = PlugNone;
}

Copier::~Copier(void) {
    //if(m_ptUdev) { udev_unref(m_ptUdev); m_ptUdev=nullptr; }
}

void Copier::slotDeviceAdded(const QString& dev) {
    QByteArray devname = dev.section('/', -1).toLocal8Bit();
    const char *fstype = nullptr;
    const char *fsuuid = nullptr;
    struct udev *ddev = nullptr;
    struct udev_device *udev = nullptr;
    int timeout=0;

    ddev = udev_new();
    udev = udev_device_new_from_subsystem_sysname(ddev, "block", devname);
    //printf("devtype:%s\n", udev_device_get_property_value(udev, "DEVTYPE"));
    timeout = strcmp("partition", udev_device_get_property_value(udev, "DEVTYPE"));
    udev_device_unref(udev); udev=nullptr;
    udev_unref(ddev); ddev=nullptr;
    if(timeout) { return; }
    if(m_nState == PlugAdd) return;

    for(timeout=0; timeout<10; timeout++) {
        ddev = udev_new();
        udev = udev_device_new_from_subsystem_sysname(ddev, "block", devname);
        fstype = udev_device_get_property_value(udev, "ID_FS_TYPE");
        fsuuid = udev_device_get_property_value(udev, "ID_FS_UUID_ENC");
        if(fstype && fsuuid) break;
        //else { printf("trying...\n"); }
        udev_device_unref(udev); udev=nullptr;
        udev_unref(ddev); ddev=nullptr;
        msleep(250);
    }

    //for(timeout=0; timeout<100&&!fstype; timeout++) {
    //    fstype = udev_device_get_property_value(udev, "ID_FS_TYPE");
    //    fsuuid = udev_device_get_property_value(udev, "ID_FS_UUID_ENC");
    //    if(fstype && fsuuid) break;
    //    msleep(250);
    //}
    //udev = udev_device_new_from_subsystem_sysname(ddev, "block", devname);
    //if(udev) {
#if 0
        fstype = udev_device_get_action(udev);
        for(timeout=0; timeout<100&&!fstype; timeout++) {
            msleep(250); //waiting for device ready
            //fstype = udev_device_get_action(udev);
            //fstype = udev_device_get_property_value(udev, "ID_FS_TYPE");
            //fsuuid = udev_device_get_property_value(udev, "ID_FS_UUID_ENC");
            //if(fstype!=nullptr && fsuuid!=nullptr) break;
            //else printf("try ready...\n");
            if(udev_device_get_is_initialized(udev)) break;
            else printf("try ready...\n");
        }
#endif
        //fstype = udev_device_get_property_value(udev, "ID_FS_TYPE");
        //fsuuid = udev_device_get_property_value(udev, "ID_FS_UUID_ENC");
        printf("fstype:%s fsuuid:%s\n", fstype==nullptr?"fail":fstype, fsuuid==nullptr?"fail":fsuuid);
        if(fsuuid!=nullptr && fstype!=nullptr) {
            m_strFStype = fstype;
            m_strDevName = dev;
            m_strUUID = fsuuid;
            m_nState = PlugAdd;
            //emit slotMountableAdded(m_strDevName, m_strUUID, m_strFStype);            
            emit slotScriptExec(m_strDevName, m_strUUID, m_strFStype, m_strScsiDevice);
        } else {
        }
        udev_device_unref(udev);
    //}
    udev_unref(ddev);
}
void Copier::slotDeviceRemoved(const QString& dev) {
    Q_UNUSED(dev);
    printf("device removed.%s\n", qPrintable(dev));
    emit slotMountableDelete(dev, m_strUUID, m_strFStype);
    m_nState = PlugNone;
}
void Copier::slotDeviceChanged(const QString& dev) {
    Q_UNUSED(dev);
    //printf("device changed.%s\n", qPrintable(dev));
    m_nState = PlugChange;
}

void Copier::slotScsiAdded(const QString& dev) {
    m_strScsiDevice = dev;
    printf("scsi_generic:%s\n", dev.toLocal8Bit().data());
}

void Copier::MountableAdded(const QString &dev, const QString &tgtdir, const QString &fstype) {
    QString strName;
    int rc;
    char opVFAT[] = "utf8,uid=1000,gid=1000,fmask=0022,dmask=0022,shortname=mixed,showexec,utf8,flush";
    printf("dev:%s ", dev.toLocal8Bit().data());
    printf("uuid:%s ", tgtdir.toLocal8Bit().data());
    printf("fs:%s ", fstype.toLocal8Bit().data());
    printf("\n");
    strName = "/media/" + tgtdir;
    mkdir(strName.toLocal8Bit().data(), S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);//775
    if(fstype == "vfat") {
        rc = mount(  dev.toLocal8Bit().data(), strName.toLocal8Bit().data(), fstype.toLocal8Bit().data(),
                MS_NOSUID | MS_NODEV | MS_RELATIME, opVFAT);
        if(rc==0) printf("mount success.\n");
    }
}

void Copier::MountableDelete(const QString &dev, const QString &tgtdir, const QString &fstype) {
    (void)fstype; (void)dev; (void)tgtdir;
#if 0
    QString strName;
    int rc;
    strName = "/media/" + tgtdir;
    rc = umount2(strName.toLocal8Bit().data(), 1);
    if(rc == 0) {
        rc = rmdir(strName.toLocal8Bit().data());
        if(rc == 0) { printf("remount success\n"); }
    } else { printf("umount %s fail\n", strName.toLocal8Bit().data()); }
#endif
    m_strUUID.clear();
    m_strFStype.clear();
    m_strDevName.clear();
    m_strScsiDevice.clear();
}

void Copier::ScriptExec(const QString &dev, const QString &tgtdir, const QString &fstype, const QString &ScsiDev) {
    (void)dev; (void)tgtdir; (void)fstype;
    QString strExec = m_qNameExec + " " + dev + " " + tgtdir + " " + fstype + " " + ScsiDev;
    printf("exec %s\n", strExec.toLocal8Bit().data());
    system(strExec.toLocal8Bit().data());
}

void Copier::DoExec(const QString &strDEV) { (void)strDEV; }

void Copier::CheckExist(void) { }

void printhelp(char *argv0) {
    printf("usage: %s -c <monior-file>\n", argv0);
    printf("usage: %s -e <exec-file>\n", argv0);
    printf("example: %s -c play.xml -e %s\n", argv0, argv0);
}


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Copier cpr;
    int nArgc=argc;
    char **ppcArgv=argv;
    cpr.m_qNameExec = "~/exec.sh";
    for(; nArgc>0; ppcArgv++, nArgc--) {
        if(!strcmp("-e", *ppcArgv ) && nArgc >= 2) {
            ppcArgv++; nArgc--;
            cpr.m_qNameExec = *ppcArgv;
        }
    }

    printf("script: %s\n", cpr.m_qNameExec.toLocal8Bit().data());



    printf("waiting hotplug device..\n");

    return a.exec();
}
