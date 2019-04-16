#include "hardware.h"
#include <QSerialPort>
#include <QTime>
#include <QTimer>
#include <QDebug>

float ema(int newValue) {
    static constexpr float alpha = 0.7;
    static float lastEma = 0;
    lastEma = (alpha * newValue) + (1.0 - alpha) * lastEma;
    return lastEma;
}

Hardware::Hardware(QObject *parent) : QObject(parent), 
		  elmFound(false),
		  m_smoothingEnabled(true) {}

Hardware::~Hardware() {}

// Find current baudrate of adapter
bool Hardware::findBaudrate() {
    const qint32 tryBaudrates[]={2000000, 115200, 38400, 9600, 230400, 57600, 19200, 1000000, 500000};
    // Try different baudrates to find correct one
    qint32 baud=0;
    for (auto b : tryBaudrates) {
        serialPort.setBaudRate(b);
        qDebug() << "Trying baudrate:" << b;
        // Send no-meaning command to check for answer
        QByteArray r = sendCmdSync("\x7F\x7F\r",1000);
        if (r.endsWith('>')) {
            baud=b;
            break;
        }
    }
    if (baud) {
        qDebug() << "Connected with baudrate: " << baud << "\n";
        return true;
    }
    qDebug() << "Can't find correct baudrate";
    return false;
}

// Set maximal baudrate that is supported by adapter
bool Hardware::setMaxBaudrate() {
    const qint32 setBaudrates[]={2000000, 1000000, 500000, 230400, 115200};
    qint32 savedBaudrate = serialPort.baudRate();
    // Increase baudrate to maximum (try 2 000 000 bps first, then go down to 115 200)
    QByteArray idString = sendCmd("ATI");
    qDebug() << "Adapter ID:" << idString;

    QByteArray r = sendCmd("ATBRT20");
    if (!r.contains("OK")) {
        qDebug() << "ATBRT command is not supported";
        return false;
    }

    for (auto s : setBaudrates) {
        serialPort.setBaudRate(savedBaudrate);

        qDebug() << "Trying to set baudrate:" << s;
        QByteArray r = sendCmd(QString("ATBRD%1").arg(qint32(round(qint32(4000000)/s)),2,16,QChar('0')),100);
        if (!r.contains("OK")) {
            qDebug() << "Didn't get OK";
            continue;
        }
        else qDebug() << "Got first reply:" << r;

        r.clear();
        // Now switch serial baudrate to new one and wait for id String
        serialPort.setBaudRate(s);
        qDebug() << "Interface baudrate is" << serialPort.baudRate();

        while(serialPort.waitForReadyRead(100)) r.append(serialPort.readAll());
        qDebug() << "Got second reply:" << r;

        if (r.contains(idString)) {
            // We got correct reply, so lets confirm this baudrate by sending empty '\r' to scanner
            sendCmd("");
            qDebug() << "New (maximum) baudrate is " << serialPort.baudRate();
            return true;
        }
    }
    return false;
}


// Open serial port
bool Hardware::open(const QString& port) {
    searching = false;
    buffer.clear();
    if (elmFound) serialPort.close();
    qDebug() << "Opening serial port ..." << port;
    serialPort.setPortName(port);
    serialPort.setDataBits(QSerialPort::Data8);
    serialPort.setParity(QSerialPort::NoParity);
    serialPort.setStopBits(QSerialPort::OneStop);
    serialPort.setFlowControl(QSerialPort::NoFlowControl);
    serialPort.setBaudRate(115200);
    if (!serialPort.open(QIODevice::ReadWrite)) {
        qDebug() << "Failed to open serial port!";
        return false;
    }
    else qDebug() << "Serial port is opened";

    // Find current baudrate of adapter
    if (!findBaudrate()) return false;

    // Set maximal baudarate
    if (!setMaxBaudrate()) return false;

    qDebug() << "Initializing ELM327 ...";
    elmFound = init();
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
bool Hardware::init() {
    if (!serialPort.isOpen()) return false;

    QByteArray r;

    // Soft reset ELM327
    //r = sendCmd("ATWS",1000);
    //qDebug() << ">" << r;
    //if (r.length()==0) return false;

    // Line feeds Off
    r = sendCmd("ATL0",100);
    qDebug() << ">" << r;
    if (!r.contains("OK")) return false;

    // Echo off
    r = sendCmd("ATE0",100);
    qDebug() << ">" << r;
    if (!r.contains("OK")) return false;

    // Headers On
    //r = sendCmd("ATH1",100);
    //qDebug() << ">" << r;
    //if (!r.contains("OK")) return false;

    // Allow long messages (>7 bytes)
    r = sendCmd("ATAL",100);
    qDebug() << ">" << r;
    if (!r.contains("OK")) return false;

    // Set protocol to Auto
    r = sendCmd("ATSP0",100);
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
    QByteArray r;
    qDebug() << "Sending command:" << cmd;
    r = sendCmdSync(cmd.toLatin1().append('\r'), timeout);
    r.replace("\r","");
    r=r.replace(">","");
    return r;
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
        setSpeed(value);
    }
    else if (pid == "0C")  { // RPM
        bool ok = false;
        value = QString(str.mid(4, 4)).toInt(&ok, 16);
        Q_ASSERT(ok);
        qDebug() << "RPM value received:" << value;
        setRpm(value / 1000.0 / 4);
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

void Hardware::setSpeed(int value) {
    if (m_smoothingEnabled) {
        m_speed = ema(value);
        emit speedChanged();
    }
    else {
        if (m_speed == value) return;
        m_speed = value;
        emit speedChanged();
    }
}

void Hardware::setRpm(qreal value) {
    if (m_rpm == value) return;
    m_rpm = value;
    emit rpmChanged();
}

void Hardware::setSmoothingEnabled(bool value) {
    m_smoothingEnabled = value;
}

