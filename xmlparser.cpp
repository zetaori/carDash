#include "xmlparser.h"
#include "QDebug"
#include "QFile"
#include "QXmlStreamReader"


xmlParser::xmlParser() {
    init.clear();
    cmd.clear();
}

void xmlParser::printAll() {
    qDebug() << "Init section:";
    for (auto i : init) qDebug() << i;
    qDebug() << "Commands section:";
    for (auto c : cmd) qDebug() << c.name << c.send << c.replyLength << c.skipCount << c.conversion << c.units;
}

// Parse init section of XML file
void xmlParser::parseInit() {
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

// Parse commands section of XML file
void xmlParser::parseRotation() {
    while (xml.readNextStartElement()) {
        if (xml.name() == "command") {
            commands c;
            c.skipCount=0;
            c.curCount=0;
            c.replyLength=0;
            for (auto attr : xml.attributes()) {
                if (attr.name().toString() == "name") c.name=attr.value().toString();
                else if (attr.name().toString() == "send") c.send=attr.value().toString();
                else if (attr.name().toString() == "conversion") c.conversion=attr.value().toString();
                else if (attr.name().toString() == "units") c.units=attr.value().toString();
                else if (attr.name().toString() == "replyLength") c.replyLength=attr.value().toInt();
                else if (attr.name().toString() == "skipCount") c.skipCount=attr.value().toInt();
            }
            cmd.append(c);
            xml.readNext();
        }
        else break;
    }
}


// Parse XML file, and place result in 2 lists: init and cmd
bool xmlParser::process(QString fileName) {
    init.clear();
    cmd.clear();

    QFile* file = new QFile(fileName);
    if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Can't open XML config file:" << fileName;
        return false;
    }

    xml.clear();
    xml.setDevice(file);

    if (xml.readNextStartElement()) {
        if (xml.name() == "obd2") {
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
