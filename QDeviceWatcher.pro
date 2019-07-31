TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = libqdevicewatcher test testgui \
    fontfoor

libqdevicewatcher.file = src/libQDeviceWatcher.pro
test.file = test/hotplugwatcher.pro
test.depends += libqdevicewatcher

testgui.file = test/hotplugwatcher_gui.pro
testgui.depends += libqdevicewatcher

OTHER_FILES += \
    TODO.txt \
    README

