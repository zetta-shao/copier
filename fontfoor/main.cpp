#include <QCoreApplication>
#include <QFile>
#include "qdevwdt.h"

qDevWgt::qDevWgt(QThread *parent) : QThread(parent) {
    start();
    moveToThread(this); //Let bool event(QEvent *e) be in another thread
    watcher = new QDeviceWatcher;
    watcher->moveToThread(this);
    watcher->appendEventReceiver(this);
    connect(watcher, SIGNAL(deviceAdded(QString)), this, SLOT(slotDeviceAdded(QString)), Qt::DirectConnection);
    connect(watcher, SIGNAL(deviceChanged(QString)), this, SLOT(slotDeviceChanged(QString)), Qt::DirectConnection);
    connect(watcher, SIGNAL(deviceRemoved(QString)), this, SLOT(slotDeviceRemoved(QString)), Qt::DirectConnection);
    connect(this, &qDevWgt::__DoExec, this, &qDevWgt::DoExec, Qt::DirectConnection);
    m_nState = PlugNone;
    watcher->start();
    //CheckExist();
}

void qDevWgt::slotDeviceAdded(const QString& dev) {
    QFile qF;
    QString param;
    Q_UNUSED(dev);
    //qDebug("tid=%#x %s: add %s", (quintptr)QThread::currentThreadId(), __PRETTY_FUNCTION__, qPrintable(dev));
    //printf("device added.%s\n", qPrintable(dev));
    m_nState = PlugIn;
    if(qF.exists(dev+m_qFileCheck)) {
        emit __DoExec(dev);
        //QMetaObject::invokeMethod(this, "__DoExec", Q_ARG(QString, dev));
    }
}
void qDevWgt::slotDeviceRemoved(const QString& dev) {
    Q_UNUSED(dev);
    //qDebug("tid=%#x %s: remove %s", (quintptr)QThread::currentThreadId(), __PRETTY_FUNCTION__, qPrintable(dev));
    //printf("device removed.%s\n", qPrintable(dev));
    m_nState = PlugOut;
}
void qDevWgt::slotDeviceChanged(const QString& dev) {
    Q_UNUSED(dev);
    //qDebug("tid=%#x %s: change %s", (quintptr)QThread::currentThreadId(), __PRETTY_FUNCTION__, qPrintable(dev));
    //printf("device changed.%s\n", qPrintable(dev));
    m_nState = PlugChange;
}

void qDevWgt::DoExec(const QString &strDEV) {
    QString param;
    QFile qF;
    char pcParam[256];
    param = m_qFileExec + " -d " + strDEV + " -c " + strDEV + m_qFileCheck;
    strcpy(pcParam, param.toLocal8Bit().data());
    do {
        printf("exec: %s\n", pcParam);
        system(pcParam);
        if(!qF.exists(strDEV + m_qFileCheck)) { printf("!\n"); break; }
    } while(m_nState==PlugIn);
}

void qDevWgt::CheckExist(void) {
    char    pcVol[4] = { 0 };
    int     nVol = 4;
    QString str;
    QFile   qF;
    for(; nVol<=26; nVol++) {
        pcVol[0] = static_cast<char>(nVol+0x40L);
        str = pcVol;
        str += ":/";
        str += m_qFileCheck;
        //printf("check %s\n", str.toLocal8Bit().data());
        if(qF.exists(str)) {
            str = pcVol;
            str += ":/";
            //printf("emit %s\n", str.toLocal8Bit().data());
            QMetaObject::invokeMethod(watcher, "deviceAdded", Q_ARG(QString, str));
        }
    }
}

int main(int argc, char *argv[])
{
    char **pvArgv;
    int i;
    QCoreApplication a(argc, argv);
    qDevWgt objDev;

    objDev.SetFileCheck("set.xml");
    objDev.SetFileExec("start.exe");

    for(pvArgv=argv,i=argc; i>0; pvArgv++, i--) {
        if(!strcmp("-c", *pvArgv) && i>1) {
            pvArgv++; i--;
            objDev.SetFileExec(*pvArgv);
        }
        if(!strcmp("-e", *pvArgv) && i>1) {
            pvArgv++; i--;
            objDev.SetFileExec(*pvArgv);
        }
    }

    objDev.CheckExist();

    return a.exec();
}

