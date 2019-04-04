#ifndef HARDWARE_H
#define HARDWARE_H

#include <QObject>
#include <QSerialPort>

class Hardware : public QObject {
    Q_OBJECT

    Q_PROPERTY(int speed READ speed NOTIFY speedChanged);
    Q_PROPERTY(qreal rpm READ rpm NOTIFY rpmChanged);

public:
    explicit Hardware(QObject* parent = 0);
    ~Hardware();

    bool sendCmdAsync(const QByteArray& cmd);
    QByteArray sendCmdSync(const QByteArray& cmd, int timeout=200);

private slots:
    void readData();

private:
    bool init();
    bool findBaudrate();
    bool setMaxBaudrate();

private:
    QSerialPort serialPort;
    bool elmFound;
    QByteArray buffer;

public:
    bool searching;
    bool open(const QString& port);
    void close();
    QByteArray sendCmd(const QString& cmd, int timeout=200);
    void processPacket(const QString& str);

    int speed() const { return m_speed; }
    qreal rpm() const { return m_rpm; }
    int airTemp() const { return m_airTemp; }
    int coolantTemp() const { return m_coolantTemp; }
    int fuelLevel() const { return m_fuelLevel; }

signals:
    void speedChanged();
    void rpmChanged();
    void airTempChanged();
    void coolantTempChanged();
    void fuelLevelChanged();

private:
    int m_speed = 0;
    qreal m_rpm = 0;
    int m_airTemp = 0;
    int m_coolantTemp = 0;
    int m_fuelLevel = 0;
};

#endif
