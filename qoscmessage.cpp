#include <QByteArray>
#include <QDebug>
#include <QtEndian>
#include <QVariant>

#include "qoscmessage_p.h"

// TUIO packets are transmitted using the OSC protocol, located at:
//   http://opensoundcontrol.org/specification
// Snippets of this specification have been pasted into the source as a means of
// easily communicating requirements.

// XXX:
// - helper for OSC-string to reduce the magic and make it feel a little safer
// (no chance of read-past-data assert hitting)
// - do we need to check for read-past-bounds?

QOscMessage::QOscMessage(const QByteArray &data)
    : m_isValid(false)
{
    qDebug() << data.toHex();
    quint32 parsedBytes = 0;

    // "An OSC message consists of an OSC Address Pattern"
    QByteArray addressPattern = QByteArray(data.constData() + parsedBytes); // rely on the null to 'save' us
    parsedBytes += addressPattern.size();
    while ((data.constData() + parsedBytes) && *(data.constData() + parsedBytes) == '\0')
        parsedBytes++;

    if (addressPattern.size() == 0)
        return;

    // "followed by an OSC Type Tag String"
    QByteArray typeTagString = QByteArray(data.constData() + parsedBytes); // rely on the null to 'save' us
    parsedBytes += typeTagString.size();
    while ((data.constData() + parsedBytes) && *(data.constData() + parsedBytes) == '\0')
        parsedBytes++;

    if (typeTagString.size() == 0 || typeTagString.at(0) != ',')
        return;

    QList<QVariant> arguments;

    // "followed by zero or more OSC Arguments."
    for (int i = 1; i < typeTagString.size(); ++i) {
        char typeTag = typeTagString.at(i);
        if (typeTag == 's') { // osc-string
            QByteArray aString = QByteArray(data.constData() + parsedBytes); // rely on the null to 'save' us
            parsedBytes += aString.size();
            while ((data.constData() + parsedBytes) && *(data.constData() + parsedBytes) == '\0')
                parsedBytes++;
            arguments.append(aString);
        } else if (typeTag == 'i') { // int32
            quint32 anInt = qFromBigEndian<quint32>((const uchar*)data.constData() + parsedBytes);
            parsedBytes += sizeof(quint32);
            arguments.append((int)anInt);
        } else if (typeTag == 'f') { // float32
            quint32 anInt = qFromBigEndian<quint32>((const uchar*)data.constData() + parsedBytes);
            parsedBytes += sizeof(quint32);
            arguments.append((float)anInt);
        } else {
            qWarning() << "Reading argument of unknown type " << typeTag;
            return;
        }
    }

    m_isValid = true;
    m_addressPattern = addressPattern;
    m_arguments = arguments;

    qDebug() << "Message with address pattern: " << addressPattern;
    qDebug() << "Arguments: " << arguments;
    //
    // Note: some older implementations of OSC may omit the OSC Type Tag string.
    // Until all such implementations are updated, OSC implementations should be
    // robust in the case of a missing OSC Type Tag String."
    //
    // (although, the editor notes one may question how exactly the hell one is
    // supposed to be robust when the behaviour is unspecified.)
}

bool QOscMessage::isValid() const
{
    return m_isValid;
}

QByteArray QOscMessage::addressPattern() const
{
    return m_addressPattern;
}

QList<QVariant> QOscMessage::arguments() const
{
    return m_arguments;
}

