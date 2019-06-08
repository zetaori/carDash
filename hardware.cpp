#include "hardware.h"
#include <QSerialPort>
#include <QTime>
#include <QTimer>
#include <QDebug>
#include <QJSEngine>
#include <QJSValue>
#include <QThread>
#include <QTimer>


Hardware::Hardware(const QString& xmlConfig, QObject *parent) : QObject(parent),
    m_searching(false),
    m_serialPort(nullptr),
    m_lastCmdTimerId(nullptr),
    m_isInitialized(false),
    m_currBaudrate(0),
    m_hardwareThread(new QThread(this))
{
    m_xmlParser.process(xmlConfig);
    m_xmlParser.printAll();

    // Do not move it to another thread, instead create it in another thread
    connect(m_hardwareThread, &QThread::started, [=] {
                m_serialPort = new QSerialPort(); // can not make a child of a parent from another thread
                m_lastCmdTimerId = new QTimer();
                connect(m_lastCmdTimerId, &QTimer::timeout, [=] {
                    Q_ASSERT(!m_lastAsyncCommand.isEmpty());
                    auto cmd = m_lastAsyncCommand;
                    m_lastAsyncCommand.clear();
                    qDebug() << "Cleared last async cmd on timer";
                    m_lastCmdTimerId->stop();
                    emit commandFinished(cmd);
                });
            });
    connect(m_hardwareThread, &QThread::finished, [=] {
                m_serialPort->deleteLater();
                m_lastCmdTimerId->stop();
                m_lastCmdTimerId->deleteLater();
                m_serialPort = NULL;
            });
    m_hardwareThread->setObjectName("HardwareThread");
    m_hardwareThread->start();

    // QTimer::singleShot(2000, [=] { processPacket("410D40"); });
    // QTimer::singleShot(3000, [=] { processPacket("410D50"); });
    // QTimer::singleShot(4000, [=] { processPacket("410D60"); });
    // QTimer::singleShot(6000, [=] { processPacket("410D80"); });

    // QTimer* timer = new QTimer(this);
    // connect(timer, &QTimer::timeout, [=] { processPacket("410D" + QString::number(int(qrand() % 100)).toLatin1()); });
    // timer->start(33);
}

Hardware::~Hardware() {
    m_hardwareThread = nullptr;
}

// Find current baudrate of adapter
bool Hardware::findBaudrate() {
    const quint32 tryBaudrates[]={2000000, 115200, 38400, 9600, 230400, 57600, 19200, 1000000, 500000};
    // Try different baudrates to find correct one
    quint32 baud=0;
    for (auto b : tryBaudrates) {
        m_serialPort->setBaudRate(b);
        qDebug() << "Trying baudrate:" << b;
        // Send no-meaning command to check for answer
        QByteArray r = sendCmdSync("\x7F\x7F\r",2000);
        qDebug() << "Response:" << r;
        if (r.endsWith('>')) {
            baud=b;
            break;
        }
    }
    if (baud) {
        qDebug() << "Connected with baudrate: " << baud << "\n";
        m_currBaudrate = baud;
        return true;
    }
    qWarning() << "Can't find correct baudrate";
    return false;
}

// Set maximal baudrate that is supported by adapter
bool Hardware::setMaxBaudrate() {
    qDebug() << Q_FUNC_INFO;

    const quint32 setBaudrates[]={2000000, 1000000, 500000, 230400, 115200};
    // const quint32 setBaudrates[]={115200};

    if (m_currBaudrate == setBaudrates[0])
        return true;

    qint32 savedBaudrate = m_serialPort->baudRate();
    // Increase baudrate to maximum (try 2 000 000 bps first, then go down to 115 200)
    QByteArray idString = sendCmd("ATI");
    qDebug() << "Adapter ID:" << idString;
    if (idString.startsWith("ATI"))
        idString = idString.mid(3);

    QByteArray r = sendCmd("ATBRT20");
    if (!r.contains("OK")) {
        qDebug() << "ATBRT command is not supported";
        return false;
    }

    for (auto s : setBaudrates) {
        m_serialPort->setBaudRate(savedBaudrate);

        qDebug() << "Trying to set baudrate:" << s;
        QByteArray r = sendCmd(QString("ATBRD%1").arg(qRound(4000000.0/s),2,16,QChar('0')),1000);
        if (!r.contains("OK")) {
            qDebug() << "Didn't get OK";
            continue;
        }
        else qDebug() << "Got first reply:" << r;

        r.clear();
        // Now switch serial baudrate to new one and wait for id String
        m_serialPort->setBaudRate(s);
        qDebug() << "Interface baudrate is" << m_serialPort->baudRate();

        // if (m_serialPort->waitForReadyRead(100)) {
        while (m_serialPort->waitForReadyRead(1000)) {
            r.append(m_serialPort->readAll());
            qDebug() << "Got second reply:" << r;
        }
        // else
        //     qWarning() << "Did not get second reply";

        if (r.contains(idString)) {
            // We got correct reply, so lets confirm this baudrate by sending empty '\r' to scanner
            sendCmd("");
            qDebug() << "New (maximum) baudrate is " << m_serialPort->baudRate();
            m_currBaudrate = s;
            return true;
        }
    }
    return false;
}


// Open serial port
void Hardware::open(const QString& port) {
    m_searching = false;

    if (m_isInitialized) {
        m_isInitialized = false; // not using setIsInitialized to not force splash screen again
        QTimer::singleShot(0, m_serialPort, [=] { m_serialPort->close(); });
    }

    qDebug() << "Opening serial port ..." << port;

    QTimer::singleShot(0, m_serialPort, [=] {
        Q_ASSERT(QThread::currentThread() == m_hardwareThread);

        m_buffer.clear();

        m_serialPort->setPortName(port);
        m_serialPort->setDataBits(QSerialPort::Data8);
        m_serialPort->setParity(QSerialPort::NoParity);
        m_serialPort->setStopBits(QSerialPort::OneStop);
        m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
        m_serialPort->setBaudRate(115200);
        if (!m_serialPort->open(QIODevice::ReadWrite)) {
            qDebug() << "Failed to open serial port!";
            emit initFinished();
            return;
        }
        else qDebug() << "Serial port is opened";

        // Find current baudrate of adapter
        if (!findBaudrate()) {
            emit initFinished();
            return;
        }

        // Set maximal baudarate
        if (!setMaxBaudrate())
            qWarning() << "Failed to send max baud rate, will proceed with current one";

        qDebug() << "Initializing ELM327 ...";
        init();

        emit initFinished();
    });
}

// Close serial port
void Hardware::close() {
    QTimer::singleShot(0, m_serialPort, [=] {
        if (m_serialPort->isOpen()) {
            disconnect(m_serialPort);
            m_serialPort->close();
        }
    });
    m_isInitialized = false;
}

// Check that ELM327 is connected
bool Hardware::init() {
    if (!m_serialPort->isOpen()) return false;

    const auto& cmdList = m_xmlParser.initCommands();
    for (auto& c : cmdList) {
        QByteArray r = sendCmd(c, 300);
        qDebug() << ">" << r;
        if (!r.contains("OK")) {
            m_serialPort->close();
            return false;
        }
    }

    qDebug() << "ELM327 is detected";
    connect(m_serialPort, SIGNAL(readyRead()), SLOT(readData()), Qt::DirectConnection);

    setInitialized(true);

    return true;
}

bool Hardware::sendCmdAsync(const QByteArray &cmd) {
    if (!m_isInitialized) return false;
    if (m_searching) return false;

    qDebug() << Q_FUNC_INFO << cmd;

    m_lastAsyncCommand = cmd.right(2).toLower();
    qDebug() << "Set last async cmd to" << m_lastAsyncCommand;

    QTimer::singleShot(0, m_serialPort, [=] {
                Q_ASSERT(QThread::currentThread() == m_hardwareThread);
                m_serialPort->write(cmd + char(0x0D));
                m_lastCmdTimerId->start(1000);
            });

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
    Q_ASSERT(QThread::currentThread() == m_hardwareThread);

    qDebug() << Q_FUNC_INFO << cmd;

    QByteArray r;
    if (m_serialPort->isOpen()) {
        m_serialPort->blockSignals(true);
        m_serialPort->write(cmd);
        m_serialPort->flush();
        // if (m_serialPort->waitForReadyRead(timeout))
        // int currTimeout = 0;
        // while (currTimeout <= timeout) {
        //         currTimeout += 100;
        //     if (m_serialPort->waitForReadyRead(100)) {
        //         qDebug() << "Curr timeout:" << currTimeout << timeout;
        //         if (m_serialPort->bytesAvailable() > 0)
        //             r.append(m_serialPort->readAll());
        //     }
        // }

        qDebug() << "Before blocking for read";

        while (m_serialPort->waitForReadyRead(timeout))
            r.append(m_serialPort->readAll());

        qDebug() << "After blocking for read";

        m_serialPort->blockSignals(false);
    }
    else
        qWarning() << "Serial port is closed!";
    return r;
}

void Hardware::readData() {
    Q_ASSERT(QThread::currentThread() == m_hardwareThread);

    processData(m_serialPort->readAll());
}

void Hardware::processData(const QByteArray& data) {
    if (!data.isEmpty())
        m_buffer.append(data);

    qDebug() << "Buffer:" << data << ", size:" << m_buffer.size() << " bytes";

    while(m_buffer.contains(0x0D)) {
        int i=m_buffer.indexOf(0x0D);
        QByteArray d = m_buffer.left(i);
        QByteArray str = d.replace("\r", "").replace("\n", "").replace(" ", "").replace(">", "").trimmed();
        if (str.length()) processPacket(str);
        m_buffer=m_buffer.mid(i+1);
    }

}

void Hardware::processPacket(const QByteArray& str) {

    Q_ASSERT(QThread::currentThread() == m_hardwareThread);

    qDebug() << "Raw data received:" << str;

    if (str.contains("SEARCHING")) {
        m_searching = true;
        return;
    }

    m_searching = false;

    if (!str.startsWith("41") || str.length() < 6) {
        qWarning() << "Received data is too short:" << str;
        return;
    }

    QByteArray pid = str.mid(2, 2).toLower();
    // qDebug() << "Pid:" << pid;

    QJSEngine engine;

    // ACCESSING parser which is in another thread, but since we are not messing with it, I think it is OK
    auto& cmdList = m_xmlParser.commands();
    for (auto& c : cmdList) {

        QString cmdPIDShort = c->send.toLower();
        if (cmdPIDShort.startsWith("01") && cmdPIDShort.length() > 2)
            cmdPIDShort.remove("01");

        if (pid == cmdPIDShort) {
            QByteArray val = str.mid(4, c->replyLength * 2); // * 2 because 1 byte takes 2 characters

            if (!(c->conversion.isEmpty())) {
                // Conversion example: (B0*256+B1)/32768*14.7
                QStringList possibleParams { "B0", "B1", "B2", "B3", "V" };
                QMap<QString, int> convArgs;

                for (auto p : possibleParams) {
                    if (c->conversion.contains(p)) {
                        bool ok = false;
                        int byteNumber = p.right(1).toInt(&ok);

                        // In case of XML config error or V, index will be out of range, we take
                        // values as is.
                        if (!ok || byteNumber < 0 || byteNumber >= val.length())
                            convArgs.insert(p, val.toInt(NULL, 16));
                        else // B0-B3
                            convArgs.insert(p, val.mid(byteNumber * 2, 2).toInt(NULL, 16));
                    }
                }

                QString functionTemplate = "(function(%1) { return %2; })";
                QString funcStr = functionTemplate.arg(convArgs.keys().join(",")).arg(c->conversion);

                // qDebug() << "funcStr:" << funcStr << ", args:" << convArgs.values();

                QJSValue jsFunc = engine.evaluate(funcStr);
                QJSValueList args;
                for (auto v : convArgs.values())
                    args << v;

                QJSValue finalValue = jsFunc.call(args);

                // qDebug() << "Calculated result:" << finalValue.toVariant();

                // I don't pass QJSValue directly, because it seems to get destroyed at times
                // before it reaches QML handler, so receiving undefined instead.
                // That is mostlikely caused by passing QJSValue to QML from a different thread
                emit dataReceived(c->name, finalValue.toVariant());
            }
            else {
                qDebug() << "Conversion is empty for:" << pid;
                emit dataReceived(c->name, val.toInt(NULL, 16));
            }

            if (pid == m_lastAsyncCommand) {
                m_lastAsyncCommand.clear();
                qDebug() << "Cleared last async cmd";
                Q_ASSERT(m_lastCmdTimerId->isActive());
                m_lastCmdTimerId->stop();
                emit commandFinished(pid);
            }
            else
                qDebug() << "Pid" << pid << " != last cmd" << m_lastAsyncCommand;

            break;
        }
        // else
        //     qDebug() << "Cmd send:" << c->send << "not equal to:" << pid;
    }
}


const XmlParser& Hardware::parser() const
{
    return m_xmlParser;
}

void Hardware::setInitialized(bool value)
{
    qDebug() << Q_FUNC_INFO << value;

    if (m_isInitialized == value)
        return;

    m_isInitialized = value;
    emit isInitializedChanged();
}

bool Hardware::isInitialized() const
{
    return m_isInitialized;
}

const QObject* Hardware::workerThreadObject() const
{
    return m_serialPort;
}
