#ifndef XMLPARSER_H
#define XMLPARSER_H

#include <QList>
#include <QXmlStreamReader>

typedef struct{
    QString name;           // Name of Cardash widget that will show/handle this parameter
    QString send;           // Command to send to OBD2
    int replyLength;        // Expected # of bytes in reply
    int skipCount;          // Number of skips in rotations for this value
    int curCount;           // Current counter of rotation
    QString conversion;     // Conversion of result value (B1-B4 for bytes, V for full value, + - * / << >> and constants are allowed)
    QString units;          // Units of result value ("C","F","km/h","mph","bar","psi","bit", for "bit" we check lowest bit)
} commands;

class xmlParser {

public:
    xmlParser();
    bool process(QString fileName);
    void printAll();

private:
    QList<commands> cmd;
    QList<QString> init;
    QXmlStreamReader xml;

    void parseInit();
    void parseRotation();
};

#endif // XMLPARSER_H
