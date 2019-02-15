#include "hardware.h"
#include <QSerialPort>
#include <QTime>
#include <QTimer>
#include <QDebug>

Hardware::Hardware(QObject *parent) : QObject(parent), elmFound(false) {}

Hardware::~Hardware() {}

// Open serial port
bool Hardware::open(const QString& port, QSerialPort::BaudRate baudrate) {
    searching = false;
    buffer.clear();
    if (elmFound) serialPort.close();
    qDebug() << "Opening serial port ..." << port;
    serialPort.setPortName(port);
    serialPort.setBaudRate(baudrate);
    serialPort.setDataBits(QSerialPort::Data8);
    serialPort.setParity(QSerialPort::NoParity);
    serialPort.setStopBits(QSerialPort::OneStop);
    serialPort.setFlowControl(QSerialPort::NoFlowControl);

    if (!serialPort.open(QIODevice::ReadWrite)) {
        qDebug() << "Failed to open serial port!";
        return false;
    }
    else qDebug() << "Serial port is opened";

    qDebug() << "Detecting ELM327 ...";
    elmFound = startupCheck();
    if (!elmFound) {
        serialPort.close();
        qDebug() << "Failed to contact ELM327!";
    }
    else {
        qDebug() << "ELM327 is detected";
        connect(&serialPort, SIGNAL(readyRead()), SLOT(readData()));
    }

    return elmFound;
}

// Close serial port
void Hardware::close() {
    if (serialPort.isOpen()) {
        disconnect(&serialPort);
        serialPort.close();
    }
    elmFound = false;
}

// Check that ELM327 is connected
bool Hardware::startupCheck() {
    if (!serialPort.isOpen()) return false;

    // Reset ELM327
    QByteArray r = sendCmd("ATZ",1000);
    qDebug() << ">" << r;
    if (r.length()==0) return false;

    // Get module information
    r = sendCmd("ATI",1000);
    qDebug() << ">" << r;
    if (!r.contains("ELM327")) return false;

    // Line feeds On
    r = sendCmd("ATL1");
    qDebug() << ">" << r;
    if (!r.contains("OK")) return false;

    // Headers On
    //r = sendCmd("ATH1");
    //qDebug() << ">" << r;
    //if (!r.contains("OK")) return false;

    // Echo off
    r = sendCmd("ATE0");
    qDebug() << ">" << r;
    if (!r.contains("OK")) return false;

    // Allow long messages (>7 bytes)
    r = sendCmd("ATAL");
    qDebug() << ">" << r;
    if (!r.contains("OK")) return false;

    // Set protocol to Auto
    r = sendCmd("ATSP0",1000);
    qDebug() << ">" << r;
    if (!r.contains("OK")) return false;

    return true;
}

bool Hardware::sendCmdAsync(const QByteArray &cmd) {
    if (!elmFound) return false;
    if (searching) return false;
    QByteArray data = cmd;
    serialPort.write(data.append(0x0D));
    return true;
}

// Send AT command to ELM327
QByteArray Hardware::sendCmd(const QString& cmd, int timeout) {
    qDebug() << "Sending command:" << cmd;
    QByteArray data = cmd.toLatin1().append(0x0D);
    return sendCmdSync(data, timeout);
}

QByteArray Hardware::sendCmdSync(const QByteArray &cmd, int timeout) {
    QByteArray r;
    if (serialPort.isOpen()) {
        serialPort.blockSignals(true);
        serialPort.write(cmd);
        serialPort.flush();
        while(serialPort.waitForReadyRead(timeout))
            r.append(serialPort.readAll());

        serialPort.blockSignals(false);
        if (!r.isEmpty()) {
            if (r.at(r.size()-1) == 0x0D) r.remove(r.size()-1, 1);
        }
    }
    return r;
}

void Hardware::readData() {
    QByteArray data = serialPort.readAll();
    buffer.append(data);

    //qDebug() << "Buffer: " << buffer.size() << " bytes";

    while(buffer.contains(0x0D)) {
        int i=buffer.indexOf(0x0D);
        QByteArray d = buffer.left(i);
        QString str = d.replace("\r", "").replace("\n", "").replace(" ", "").replace(">", "").trimmed();
        if (str.length()) processPacket(str);
        buffer=buffer.mid(i+1);
    }
}

void Hardware::processPacket(const QString &str) {

    //qDebug() << "Raw data recieved:" << str;

    if (str.contains("SEARCHING")) {
        searching = true;
        return;
    }

    searching = false;

    if (!str.startsWith("41") || str.length() < 6) return;

    int value = 0;
    QString pid = str.mid(2, 2);
    //qDebug() << "Pid:" << pid;

    if (pid == "0D")  { // Speed
        bool ok = false;
        value = QString(str.mid(4, 2)).toInt(&ok, 16);
        Q_ASSERT(ok);
        //qDebug() << "Speed value received:" << value;
        m_speed = value;

        emit speedChanged();
    }
    else if (pid == "0C")  { // RPM
        bool ok = false;
        value = QString(str.mid(4, 4)).toInt(&ok, 16);
        Q_ASSERT(ok);
        qDebug() << "RPM value received:" << value;
        m_rpm = value / 1000.0 / 4;
        emit rpmChanged();
    }
    else if (pid == "46")  { // air temperature in C
        bool ok = false;
        value = QString(str.mid(4, 4)).toInt(&ok, 16);
        Q_ASSERT(ok);
        qDebug() << "Air temp value received:" << value;
        m_airTemp = value - 40;
        emit airTempChanged();
    }
    else if (pid == "05")  { // coolant temperature in C
        bool ok = false;
        value = QString(str.mid(4, 4)).toInt(&ok, 16);
        Q_ASSERT(ok);
        qDebug() << "Coolant temp value received:" << value;
        m_coolantTemp = value - 40;
        emit coolantTempChanged();
    }
    else if (pid == "2F")  { // fuel level in %
        bool ok = false;
        value = QString(str.mid(4, 4)).toInt(&ok, 16);
        Q_ASSERT(ok);
        qDebug() << "Fuel level received:" << value;
        m_fuelLevel = 100*value / 255;
        emit fuelLevelChanged();
    }
    else
        return;
}
