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

#include <QLoggingCategory>

#include "qtuiocursor_p.h"
#include "qtuiohandler_p.h"
#include "qoscbundle_p.h"

Q_LOGGING_CATEGORY(lcTuioSource, "qt.qpa.tuio.source")
Q_LOGGING_CATEGORY(lcTuioAlive, "qt.qpa.tuio.alive")
Q_LOGGING_CATEGORY(lcTuioSet, "qt.qpa.tuio.set")

QTuioHandler::QTuioHandler()
{
    if (!m_socket.bind(QHostAddress::Any, 40001)) {
        qWarning() << "Failed to bind TUIO socket: " << m_socket.errorString();
        return;
    }

    connect(&m_socket, &QUdpSocket::readyRead, this, &QTuioHandler::processPackets);
}

QTuioHandler::~QTuioHandler()
{
}

void QTuioHandler::processPackets()
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
                process2DCurFseq(message);
            } else {
                qWarning() << "Ignoring unknown TUIO message type: " << messageType;
                continue;
            }
        }
    }
}

void QTuioHandler::process2DCurSource(const QOscMessage &message)
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

void QTuioHandler::process2DCurAlive(const QOscMessage &message)
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
    QMap<int, QTuioCursor> oldActiveCursors = m_activeCursors;
    QMap<int, QTuioCursor> newActiveCursors;

    for (int i = 1; i < arguments.count(); ++i) {
        if (arguments.at(i).type() != QVariant::Int) {
            qWarning() << "Ignoring malformed TUIO alive message (bad argument on position" << i << arguments << ")";
            return;
        }

        int cursorId = arguments.at(i).toInt();
        if (!oldActiveCursors.contains(cursorId)) {
            // newly active
            QTuioCursor cursor(cursorId);
            cursor.setState(Qt::TouchPointPressed);
            newActiveCursors.insert(cursorId, cursor);
        } else {
            // we already know about it, remove it so it isn't marked as released
            QTuioCursor cursor = oldActiveCursors.value(cursorId);
            cursor.setState(Qt::TouchPointStationary); // position change in SET will update if needed
            newActiveCursors.insert(cursorId, cursor);
            oldActiveCursors.remove(cursorId);
        }
    }

    // anything left is dead now
    QMap<int, QTuioCursor>::ConstIterator it = oldActiveCursors.constBegin();
    m_deadCursors.reserve(oldActiveCursors.size());
    while (it != oldActiveCursors.constEnd()) {
        m_deadCursors.append(it.value());
        ++it;
    }

    m_activeCursors = newActiveCursors;
}

void QTuioHandler::process2DCurSet(const QOscMessage &message)
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

    QMap<int, QTuioCursor>::Iterator it = m_activeCursors.find(cursorId);
    if (it == m_activeCursors.end()) {
        qWarning() << "Ignoring malformed TUIO set for nonexistent cursor " << cursorId;
        return;
    }

    qDebug(lcTuioSet) << "Processing SET for " << cursorId << " x: " << x << y << vx << vy << acceleration;
    QTuioCursor &cur = *it;
    cur.setX(x);
    cur.setY(y);
    cur.setVX(vx);
    cur.setVY(vy);
    cur.setAcceleration(acceleration);
}

void QTuioHandler::process2DCurFseq(const QOscMessage &message)
{
    Q_UNUSED(message); // TODO: do we need to do anything with the frame id?

    foreach (const QTuioCursor &tc, m_activeCursors) {
        int cursorId = tc.id();
        if (tc.state() == Qt::TouchPointPressed) {
            qDebug(lcTuioAlive) << "New TUIO object: " << cursorId << " is alive";
        } else if (tc.state() == Qt::TouchPointStationary) {
            qDebug() << "Stationary TUIO cursor: " << cursorId;
        } else if (tc.state() == Qt::TouchPointMoved) {
            qDebug() << "Moved TUIO cursor: " << cursorId;
        }
    }

    foreach (const QTuioCursor &tc, m_deadCursors) {
        qDebug(lcTuioAlive) << "Dead TUIO object: " << tc.id();
    }
}


