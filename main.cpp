#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QQmlContext>
#include <QCursor>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSettings>
#include <QDebug>
#include <QThread>

#include "hardware.h"
#include <xmlparser.h>

int main(int argc, char *argv[]) {

    if (argc < 2) {
        qWarning() << "Usage: program <xml_commands_file.xml> <serial_port_optional>";
        return EXIT_FAILURE;
    }

    QGuiApplication app(argc, argv);
    app.setOverrideCursor(QCursor(Qt::BlankCursor));

    Hardware hw(argv[1]);

    // Load QML first with splashscreen while we are loading
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("HardwareClass", &hw);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    QString port;
    QSettings sett("ChalkElec","CarDash");
    QList<QSerialPortInfo> portList = QSerialPortInfo::availablePorts();

    // Find correct serial port of OBD2 scanner
    if (app.arguments().count() > 2) {
        port=app.arguments().at(2);
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

    QObject::connect(&hw, &Hardware::initFinished, [&] {
                if (hw.isInitialized())
                    sett.setValue("port",port);
                else
                    qFatal("Can't open serial port of OBD2 scanner!");
            });

    // Open serial port of OBD2
    hw.open(port);

    // Run cycle to send commands to controller in the proper thread
    QTimer::singleShot(0, const_cast<QObject*>(hw.workerThreadObject()), [&] {

            Q_ASSERT(QThread::currentThread()->objectName() == "HardwareThread");
// #if 0 // Sending comands from config - uncomment when needed
                auto& cmdList = hw.parser().commands();
                for (auto c : cmdList) {
                    // If skipCount is 0 (by default) we will process the command each time
                    if (++(c->curCount) >= c->skipCount) {
                        c->curCount = 0;

                        qDebug() << "Sending command:" << c->send;
                        QByteArray reply = hw.sendCmdSync(c->send, 100);
                        hw.processData(reply);
                    }
                    // else
                    //     qDebug() << "Skipping count for" << c->name;
                }
// #endif // Sensing commands from config

                // hw.sendCmdAsync("010D"); // speed
                // hw.sendCmdSync("010C"); // rpm
                // hw.sendCmdAsync("012F"); // fuel level
                // hw.sendCmdAsync("0146"); // outside air temp
                // hw.sendCmdAsync("0105"); // coolant temp
            });

    return app.exec();
}
