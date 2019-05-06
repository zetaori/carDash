#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QQmlContext>
#include <QCursor>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSettings>
#include <QDebug>

#include "hardware.h"
#include <xmlparser.h>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setOverrideCursor(QCursor(Qt::BlankCursor));

    Hardware hw;
    QString port;
    QSettings sett("ChalkElec","CarDash");
    QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();

 //   xmlParser x;
 //   x.process(app.arguments().at(1));
 //   x.printAll();
 //   return 0;



    // Find correct serial port of OBD2 scanner
    if (app.arguments().count() > 1) {
        port=app.arguments().at(1);
        qDebug() << "Using command-line port" << port;
    }
    else if (!sett.value("port").toString().isEmpty()) {
        port=sett.value("port").toString();
        qDebug() << "Using saved port" << port;
    }
    else if (!portList.isEmpty()) {
        for (QSerialPortInfo p : portList) {
            QString vidpid="VID:PID=";
            port=p.systemLocation();
            if (p.hasVendorIdentifier()) vidpid+=QString("%1:").arg(p.vendorIdentifier(),4,16);
            else vidpid+="0:";
            if (p.hasProductIdentifier()) vidpid+=QString("%1").arg(p.productIdentifier(),4,16);
            else vidpid+="0";
            qDebug() << "Found serial port:" << port << "," << vidpid;
        }
        qDebug() << "Using scanned port" << port;
    }
    else {
        qFatal("OBD2 scanner is not detected!");
    }

    // Open serial port of OBD2
    if (!hw.open(port)) qFatal("Can't open serial port of OBD2 scanner!");
    else sett.setValue("port",port);

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
                //hw.sendCmdAsync("010D"); // speed
                hw.sendCmdAsync("010C"); // rpm
                //hw.sendCmdAsync("0146"); // air temp
                //hw.sendCmdAsync("0105"); // coolant temp
            });

    timer.start(6);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("HardwareClass", &hw);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}
