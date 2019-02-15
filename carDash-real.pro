TEMPLATE = app
TARGET = carDash-real

QT += qml quick serialport
QT -= gui
CONFIG += c++11

SOURCES += main.cpp hardware.cpp
HEADERS += hardware.h

RESOURCES += cardashboard.qrc

