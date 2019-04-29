#include "xmlparser.h"
#include "QDebug"
#include "QFile"
#include "QXmlStreamReader"


XmlParser::XmlParser() {
    init.clear();
    qDeleteAll(m_commands);
    m_commands.clear();
}

void XmlParser::printAll() {
    qDebug() << "Init section:";
    for (auto i : init) qDebug() << i;
    qDebug() << "Rotation section:";
    for (auto c : m_commands) qDebug() << c->name << c->send << c->replyLength << c->skipCount << c->curCount << c->conversion << c->units;
}

// Parse init section of XML file
void XmlParser::parseInit() {
    while (xml.readNextStartElement()) {
        if (xml.name() == "command") {
            for (auto attr : xml.attributes()) {
                if (attr.name().toString() == "send") init.append(attr.value().toString());
            }
            xml.readNext();
        }
        else break;
    }
}

// Parse rotation section of XML file
void XmlParser::parseRotation() {
    while (xml.readNextStartElement()) {
        if (xml.name() == "command") {
            Command* c = new Command();
            c->skipCount=0;
            c->curCount=0;
            c->replyLength=0;
            for (auto attr : xml.attributes()) {
                if (attr.name().toString() == "name") c->name=attr.value().toString();
                else if (attr.name().toString() == "targetId") c->name=attr.value().toString();
                else if (attr.name().toString() == "send") c->send=attr.value().toLatin1();
                else if (attr.name().toString() == "conversion") c->conversion=attr.value().toString();
                else if (attr.name().toString() == "units") c->units=attr.value().toString();
                else if (attr.name().toString() == "replyLength") c->replyLength=attr.value().toInt();
                else if (attr.name().toString() == "skipCount") c->skipCount=attr.value().toInt();
            }
            m_commands.append(c);
            xml.readNext();
        }
        else break;
    }
}


// Parse XML file, and place result in 2 lists: init and m_commands
bool XmlParser::process(QString fileName) {
    init.clear();
    qDeleteAll(m_commands);
    m_commands.clear();

    QFile* file = new QFile(fileName);
    if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Can't open XML config file:" << fileName;
        return false;
    }

    xml.clear();
    xml.setDevice(file);

    if (xml.readNextStartElement()) {
        if (xml.name() == "OBD2") {
            while (xml.readNextStartElement()) {
                if (xml.name() == "init") parseInit();
                else if (xml.name() == "rotation") parseRotation();
                else xml.skipCurrentElement();
            }
        }
        else {
            qDebug() << "Unknown tag in XML config file:" << xml.name();
            return false;
        }
    }
    else if (xml.hasError()) {
        qDebug() << "Error in XML config file:" << xml.errorString();
        return false;
    }
    return true;
}

const QList<Command*>& XmlParser::commands() const
{
    return m_commands;
}
