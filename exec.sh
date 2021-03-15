#!/bin/bash
WORKPATH='/run/cpr/'
#TGTPART=`blkid|grep -v EFI|grep "TYPE=\"vfat\""`
#ORGDEV=`echo ${TGTPART}|awk '{print $1}'|cut -f1 -d':'`
#ORGFSUUID=`echo ${TGTPART}|awk -F"UUID=" '{print $2}'|cut -f2 -d'"'`
#ORGFSTYPE=`echo ${TGTPART}|awk -F"TYPE=" '{print $2}'|cut -f2 -d'"'`
#ROOTDEV=`echo ${ORGDEV}|sed 's/[0-9:]//g'|sed 's/\/dev\///g'`
#if [ -z "${TGTPART}" ]; then echo "no vfat-fs copy source"; exit; fi
#echo "rootdev:"${ROOTDEV}
#SDID=`dmesg|awk -F" sd " '{print $2}'|grep ${ROOTDEV}|tail -1|awk '{print $1}'`
#echo "SDID:"${SDID}
#ORGSCSI='/dev/'`dmesg|grep 'scsi generic'|grep ${SDID}|awk -F' scsi generic ' '{print $2}'|awk '{print $1}'|tail -1`
ORGDEV=${1}
ORGFSUUID=${2}
ORGFSTYPE=${3}
ORGSCSI=${4}
echo "orgdev:"${ORGDEV}" uuid:"${ORGFSUUID}" fstype:"${ORGFSTYPE}" scsi:"${ORGSCSI}
mkdir -p ${WORKPATH}
#TGTWORKUUID="8d8b3701-6725-41b9-bcf5-efefa04ee99d"
#TGTWORKDIR='/media/'${TGTWORKUUID}
TGTWORKDIR='/opt/works'
TGTSRCDIR=${WORKPATH}${ORGFSUUID}
if ! [ -d "${TGTSRCDIR}" ]; then mkdir -p ${TGTSRCDIR}; fi
if ! [ -d "${TGTWORKDIR}" ]; then mkdir -p ${TGTWORKDIR}; fi
MOUNTED=`mount|grep ${TGTSRCDIR}`
if [ -z "${MOUNTED}" ]; then mount -U ${ORGFSUUID} ${TGTSRCDIR}; fi
MOUNTED=`mount|grep ${TGTSRCDIR}`
if [ -z "${MOUNTED}" ]; then echo "can't find source disk, exit"; exit; fi
#MOUNTED=`mount|grep ${TGTWORKDIR}`
#if [ -z "${MOUNTED}" ]; then mount -U ${TGTWORKUUID} ${TGTWORKDIR} -o compress-force=lzo; fi
#MOUNTED=`mount|grep ${TGTWORKDIR}`
#if [ -z "${MOUNTED}" ]; then echo "can't find work disk, exit"; exit; fi
mkdir -p ${TGTWORKDIR}/${ORGFSUUID}
du -h ${TGTSRCDIR}
#du -h ${TGTWORKDIR}
rsync -avh --progress ${TGTSRCDIR}/* ${TGTWORKDIR}/${ORGFSUUID}
#umount ${TGTWORKDIR}
umount ${TGTSRCDIR}
#rm -rf ${TGTWORKDIR}
rm -rf ${TGTSRCDIR}
echo "eject:"${ORGSCSI}
eject ${ORGSCSI}
