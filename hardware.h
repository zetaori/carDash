#ifndef HARDWARE_H
#define HARDWARE_H

#include <QObject>
#include <QSerialPort>
#include <QJSValue>

class QThread;
class QTimer;

#include "xmlparser.h"

class Hardware : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isInitialized READ isInitialized NOTIFY isInitializedChanged);

public:
    explicit Hardware(const QString& xmlConfig, QObject* parent = 0);
    ~Hardware();

    QByteArray sendCmdSync(const QByteArray& cmd, int timeout=200);
    bool sendCmdAsync(const QByteArray& cmd);

    void processData(const QByteArray& data);

    const XmlParser& parser() const;

    bool isInitialized() const;

    void open(const QString& port);
    void close();

    // This is the serial port returned, so that we could use it as receiver in QueuedConnection
    // to make functions run in the receiver's thread
    const QObject* workerThreadObject() const;

private slots:
    void readData();

signals:
    void isInitializedChanged();
    void initFinished();

    void dataReceived(const QString& targetId, QVariant value);
    void commandFinished(const QByteArray& cmd);

private:
    bool init();
    bool findBaudrate();
    bool setMaxBaudrate();
    void setInitialized(bool value);

    QByteArray sendCmd(const QString& cmd, int timeout=200); // Also removes trailing >\r

    void processPacket(const QByteArray& str);

private:
    volatile bool m_searching;
    QSerialPort* m_serialPort; // Lives in another thread! Be careful to read and write in proper thread
    QByteArray m_buffer; // Be careful to read and write in serial port thread
    QByteArray m_lastAsyncCommand;
    QTimer* m_lastCmdTimerId;

    bool m_isInitialized;
    XmlParser m_xmlParser;
    quint32 m_currBaudrate;
    QThread* m_hardwareThread;
};

#endif
