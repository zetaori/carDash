#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QQmlContext>
#include <QCursor>
#include <QSerialPort>

#include "hardware.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setOverrideCursor(QCursor(Qt::BlankCursor));

    Hardware h;

    // if (!h.open("/dev/ttyUSB0"))
    // if (!h.open(argv[1]))
    if (!h.open("\\\\.\\COM22")) // open with defaut settings for OBD2
        qFatal("Check your hardware");

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
                h.sendCmdAsync("010D"); // speed
                //h.sendCmdAsync("010C"); // rpm
                //h.sendCmdAsync("0146"); // air temp
                //h.sendCmdAsync("0105"); // coolant temp
            });

    timer.start(100);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("HardwareClass", &h);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}
