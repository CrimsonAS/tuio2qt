/****************************************************************************
**
** Copyright (C) 2014 Robin Burchell <robin.burchell@viroteck.net>
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QByteArray>
#include <QDebug>
#include <QtEndian>
#include <QVariant>
#include <QLoggingCategory>

#include "qoscmessage_p.h"

Q_LOGGING_CATEGORY(lcTuioMessage, "qt.qpa.tuio.message")

// TUIO packets are transmitted using the OSC protocol, located at:
//   http://opensoundcontrol.org/specification
// Snippets of this specification have been pasted into the source as a means of
// easily communicating requirements.

// TODO: we should communicate errors ('read past end of data') vs empty strings
static QByteArray readOscString(const QByteArray &data, quint32 &pos)
{
    QByteArray re;
    int end = data.indexOf('\0', pos);
    if (end < 0) {
        pos = data.size();
        return re;
    }

    re = data.mid(pos, end - pos);

    // Skip additional NULL bytes at the end of the string to make sure the
    // total number of bits a multiple of 32 bits ("OSC-string" in the
    // specification).
    end += 4 - ((end - pos) % 4);

    pos = end;
    return re;
}

QOscMessage::QOscMessage(const QByteArray &data)
    : m_isValid(false)
{
    qCDebug(lcTuioMessage) << data.toHex();
    quint32 parsedBytes = 0;

    // "An OSC message consists of an OSC Address Pattern"
    QByteArray addressPattern = readOscString(data, parsedBytes);
    if (addressPattern.size() == 0 || parsedBytes >= data.size())
        return;

    // "followed by an OSC Type Tag String"
    QByteArray typeTagString = readOscString(data, parsedBytes);

    // "Note: some older implementations of OSC may omit the OSC Type Tag string.
    // Until all such implementations are updated, OSC implementations should be
    // robust in the case of a missing OSC Type Tag String."
    //
    // (although, the editor notes one may question how exactly the hell one is
    // supposed to be robust when the behaviour is unspecified.)
    if (typeTagString.size() == 0 || typeTagString.at(0) != ',' || parsedBytes >= data.size())
        return;

    QList<QVariant> arguments;

    // "followed by zero or more OSC Arguments."
    for (int i = 1; i < typeTagString.size(); ++i) {
        char typeTag = typeTagString.at(i);
        if (typeTag == 's') { // osc-string
            // TODO: length check should be part of readOscString
            QByteArray aString = readOscString(data, parsedBytes);
            arguments.append(aString);
        } else if (typeTag == 'i') { // int32
            if (data.length() - parsedBytes < sizeof(quint32))
                return;

            quint32 anInt = qFromBigEndian<quint32>((const uchar*)data.constData() + parsedBytes);
            parsedBytes += sizeof(quint32);

            // TODO: is int32 in OSC signed, or unsigned?
            arguments.append((int)anInt);
        } else if (typeTag == 'f') { // float32
            if (data.length() - parsedBytes < sizeof(quint32))
                return;

            Q_STATIC_ASSERT(sizeof(float) == sizeof(quint32));
            union {
                quint32 u;
                float f;
            } value;
            value.u = qFromBigEndian<quint32>((const uchar*)data.constData() + parsedBytes);
            parsedBytes += sizeof(quint32);
            arguments.append(value.f);
        } else {
            qWarning() << "Reading argument of unknown type " << typeTag;
            return;
        }
    }

    m_isValid = true;
    m_addressPattern = addressPattern;
    m_arguments = arguments;

    qCDebug(lcTuioMessage) << "Message with address pattern: " << addressPattern << " arguments: " << arguments;
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


