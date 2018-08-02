#-------------------------------------------------
#
# Project created by QtCreator 2016-02-15T10:11:40
#
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = app

VERSION = 1.1.0.7
QMAKE_TARGET_COMPANY = Straw Solutions
QMAKE_TARGET_PRODUCT = Straw Backup

DEFINES += PROGRAM_VERSION=\\\"$$VERSION\\\"
win32:DEFINES += PLATFORM_SUFFIX=\\\"win32\\\"

win32:RC_ICONS += ../res/icon.ico

Release:TARGET = strawBackup
Debug:TARGET = strawBackup_dbg

CONFIG += c++14

SOURCES += \
gui/mainwindow.cpp \
global.cpp \
main.cpp \
    gui/backupdirectoryeditdialog.cpp \
    job/backupmanager.cpp \
    gui/aboutdialog.cpp \
    job/jobthread.cpp \
    threaddb/dbmanager.cpp \
    threaddb/dbmodel.cpp \
    threaddb/dbquery.cpp


HEADERS  += \
gui/mainwindow.h \
global.h \
    gui/backupdirectoryeditdialog.h \
    job/backupmanager.h \
    gui/aboutdialog.h \
    job/jobthread.h \
    threaddb/dbmanager.h \
    threaddb/dbmodel.h \
    threaddb/dbquery.h


FORMS    += \
gui/mainwindow.ui \
    gui/backupdirectoryeditdialog.ui \
    gui/aboutdialog.ui



RESOURCES += \
    ../res/resources.qrc


INCLUDEPATH += $$PWD/../include
DEPENDPATH += $$PWD/../include

LIBS += -L$$PWD/../lib/
Debug:LIBS += -L$$PWD/../lib/debug/
Release:LIBS += -L$$PWD/../lib/release/

Release:DESTDIR = ../bin

Release:OBJECTS_DIR = release/.obj
Release:MOC_DIR = release/.moc
Release:RCC_DIR = release/.rcc
Release:UI_DIR = release/.ui
Release:PRECOMPILED_DIR = release

Debug:DESTDIR = ../bin

Debug:OBJECTS_DIR = debug/.obj
Debug:MOC_DIR = debug/.moc
Debug:RCC_DIR = debug/.rcc
Debug:UI_DIR = debug/.ui
Debug:PRECOMPILED_DIR = debug

DISTFILES +=
