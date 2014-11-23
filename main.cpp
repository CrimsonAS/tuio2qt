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

#include <QCoreApplication>
#include <QUdpSocket>
#include <QLoggingCategory>

#include "qoscbundle_p.h"

Q_LOGGING_CATEGORY(lcTuioSource, "qt.qpa.tuio.source")
Q_LOGGING_CATEGORY(lcTuioAlive, "qt.qpa.tuio.alive")
Q_LOGGING_CATEGORY(lcTuioSet, "qt.qpa.tuio.set")

class TuioCursor
{
public:
    TuioCursor(int id = -1)
        : m_id(id)
        , m_x(0)
        , m_y(0)
        , m_vx(0)
        , m_vy(0)
        , m_acceleration(0)
        , m_state(Qt::TouchPointPressed)
    {
    }

    int id() const { return m_id; }

    void setX(float x)
    {
        if (state() == Qt::TouchPointStationary &&
            !qFuzzyCompare(m_x + 2.0, x + 2.0)) { // +2 because 1 is a valid value, and qFuzzyCompare can't cope with 0.0
            setState(Qt::TouchPointMoved);
        }
        m_x = x;
    }
    float x() const { return m_x; }

    void setY(float y)
    {
        if (state() == Qt::TouchPointStationary &&
            !qFuzzyCompare(m_y + 2.0, y + 2.0)) { // +2 because 1 is a valid value, and qFuzzyCompare can't cope with 0.0
            setState(Qt::TouchPointMoved);
        }
        m_y = y;
    }
    float y() const { return m_y; }

    void setVX(float vx) { m_vx = vx; }
    float vx() const { return m_vx; }

    void setVY(float vy) { m_vy = vy; }
    float vy() const { return m_vy; }

    void setAcceleration(float acceleration) { m_acceleration = acceleration; }
    float acceleration() const { return m_acceleration; }

    void setState(const Qt::TouchPointState &state) { if (m_state == state) return; m_state = state; qDebug() << "State for " << id() << " is now " << m_state; }
    Qt::TouchPointState state() const { return m_state; }

private:
    int m_id;
    float m_x;
    float m_y;
    float m_vx;
    float m_vy;
    float m_acceleration;
    Qt::TouchPointState m_state;
};

class TuioSocket : public QObject
{
    Q_OBJECT

public:
    explicit TuioSocket();
    virtual ~TuioSocket();

private slots:
    void processPackets();
    void process2DCurSource(const QOscMessage &message);
    void process2DCurAlive(const QOscMessage &message);
    void process2DCurSet(const QOscMessage &message);

private:
    QUdpSocket m_socket;
    QMap<int, TuioCursor> m_activeCursors;
};

TuioSocket::TuioSocket()
{
    if (!m_socket.bind(QHostAddress::Any, 40001)) {
        qWarning() << "Failed to bind TUIO socket: " << m_socket.errorString();
        return;
    }

    connect(&m_socket, &QUdpSocket::readyRead, this, &TuioSocket::processPackets);
}

TuioSocket::~TuioSocket()
{
}

void TuioSocket::processPackets()
{
    while (m_socket.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket.pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_socket.readDatagram(datagram.data(), datagram.size(),
                              &sender, &senderPort);

        QOscBundle bundle(datagram);
        if (!bundle.isValid())
            continue;

        // "A typical TUIO bundle will contain an initial ALIVE message,
        // followed by an arbitrary number of SET messages that can fit into the
        // actual bundle capacity and a concluding FSEQ message. A minimal TUIO
        // bundle needs to contain at least the compulsory ALIVE and FSEQ
        // messages. The FSEQ frame ID is incremented for each delivered bundle,
        // while redundant bundles can be marked using the frame sequence ID
        // -1."
        QList<QOscMessage> messages = bundle.messages();

        foreach (const QOscMessage &message, messages) {
            if (message.addressPattern() != "/tuio/2Dcur") {
                qWarning() << "Ignoring unknown address pattern " << message.addressPattern();
                continue;
            }

            QList<QVariant> arguments = message.arguments();
            if (arguments.count() == 0) {
                qWarning() << "Ignoring TUIO message with no arguments";
                continue;
            }

            QByteArray messageType = arguments.at(0).toByteArray();
            if (messageType == "source") {
                process2DCurSource(message);
            } else if (messageType == "alive") {
                process2DCurAlive(message);
            } else if (messageType == "set") {
                process2DCurSet(message);
            } else if (messageType == "fseq") {
            } else {
                qWarning() << "Ignoring unknown TUIO message type: " << messageType;
                continue;
            }
        }
    }
}

void TuioSocket::process2DCurSource(const QOscMessage &message)
{
    QList<QVariant> arguments = message.arguments();
    if (arguments.count() != 2) {
        qWarning() << "Ignoring malformed TUIO source message: " << arguments.count();
        return;
    }

    if (arguments.at(1).type() != QVariant::ByteArray) {
        qWarning() << "Ignoring malformed TUIO source message (bad argument type)";
        return;
    }

    qCDebug(lcTuioSource) << "Got TUIO source message from: " << arguments.at(1).toByteArray();
}

void TuioSocket::process2DCurAlive(const QOscMessage &message)
{
    QList<QVariant> arguments = message.arguments();
    if (arguments.count() < 1) {
        qWarning() << "Ignoring malformed TUIO alive message: " << arguments.count();
        return;
    }

    // delta the notified cursors that are active, against the ones we already
    // know of.
    //
    // TBD: right now we're assuming one 2Dcur alive message corresponds to a
    // new data source from the input. is this correct, or do we need to store
    // changes and only process the deltas on fseq?
    QMap<int, TuioCursor> oldActiveCursors = m_activeCursors;
    QMap<int, TuioCursor> newActiveCursors;

    for (int i = 1; i < arguments.count(); ++i) {
        if (arguments.at(i).type() != QVariant::Int) {
            qWarning() << "Ignoring malformed TUIO alive message (bad argument on position" << i << arguments << ")";
            return;
        }

        int cursorId = arguments.at(i).toInt();
        if (!oldActiveCursors.contains(cursorId)) {
            // newly active
            qDebug(lcTuioAlive) << "New TUIO object: " << cursorId << " is alive";
            TuioCursor cursor(cursorId);
            cursor.setState(Qt::TouchPointPressed);
            newActiveCursors.insert(cursorId, cursor);
        } else {
            // we already know about it, remove it so it isn't marked as released
            TuioCursor cursor = oldActiveCursors.value(cursorId);
            cursor.setState(Qt::TouchPointStationary); // position change in SET will update if needed
            newActiveCursors.insert(cursorId, cursor);
            oldActiveCursors.remove(cursorId);
        }
    }

    // anything left is dead now
    QMap<int, TuioCursor>::ConstIterator it = oldActiveCursors.constBegin();
    while (it != oldActiveCursors.constEnd()) {
        qDebug(lcTuioAlive) << "Dead TUIO object: " << it.key();
        ++it;
    }

    m_activeCursors = newActiveCursors;
}

void TuioSocket::process2DCurSet(const QOscMessage &message)
{
    QList<QVariant> arguments = message.arguments();
    if (arguments.count() < 6) {
        qWarning() << "Ignoring malformed TUIO set message with too few arguments: " << arguments.count();
        return;
    }

    if (arguments.at(1).type() != QVariant::Int ||
        arguments.at(2).type() != QMetaType::Float ||
        arguments.at(3).type() != QMetaType::Float ||
        arguments.at(4).type() != QMetaType::Float ||
        arguments.at(5).type() != QMetaType::Float ||
        arguments.at(6).type() != QMetaType::Float
       ) {
        qWarning() << "Ignoring malformed TUIO set message with bad types: " << arguments;
        return;
    }

    int cursorId = arguments.at(1).toInt();
    float x = arguments.at(2).toFloat();
    float y = arguments.at(3).toFloat();
    float vx = arguments.at(4).toFloat();
    float vy = arguments.at(5).toFloat();
    float acceleration = arguments.at(6).toFloat();

    QMap<int, TuioCursor>::Iterator it = m_activeCursors.find(cursorId);
    if (it == m_activeCursors.end()) {
        qWarning() << "Ignoring malformed TUIO set for nonexistent cursor " << cursorId;
        return;
    }

    qDebug(lcTuioSet) << "Processing SET for " << cursorId << " x: " << x << y << vx << vy << acceleration;
    TuioCursor &cur = *it;
    cur.setX(x);
    cur.setY(y);
    cur.setVX(vx);
    cur.setVY(vy);
    cur.setAcceleration(acceleration);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TuioSocket sock;

    return app.exec();
}

#include "main.moc"
