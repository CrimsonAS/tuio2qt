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

#include <QtTest>

#include "../qoscbundle_p.h"
#include "../qoscmessage_p.h"

class tst_osc : public QObject
{
    Q_OBJECT

private slots:
    void testBasics();
    void simpleBundle();
    void complexBundle();
};

void tst_osc::testBasics()
{
    QOscBundle bundle = QOscBundle(QByteArray());
    QVERIFY(!bundle.isValid());
    QOscMessage message = QOscMessage(QByteArray());
    QVERIFY(!message.isValid());
}

void tst_osc::simpleBundle()
{
    QByteArray payload = QByteArray::fromHex("2362756e646c65000000000000000001000000182f7475696f2f3244637572002c730000616c6976650000000000001c2f7475696f2f3244637572002c7369006673657100000000ffffffff");

    QOscBundle bundle(payload);
    QVERIFY(bundle.isValid());
}

void tst_osc::complexBundle()
{
    QByteArray payload = QByteArray::fromHex("2362756e646c65000000000000000001000000302f7475696f2f3244637572002c737300736f7572636500005475696f5061644031302e31302e31302e31323000000000000000282f7475696f2f3244637572002c73696969000000616c697665000000000000010000000200000003000000342f7475696f2f3244637572002c736966666666660000000073657400000000013ee666663f14cccdbfc8001200000000410236b7000000342f7475696f2f3244637572002c736966666666660000000073657400000000023f0666663e8ccccdbfe95565be47ffb4418158c3000000342f7475696f2f3244637572002c736966666666660000000073657400000000033e6666683f333333bf47fff33e480031c23d4d1d0000001c2f7475696f2f3244637572002c736900667365710000000000000671");

    QOscBundle bundle(payload);
    QVERIFY(bundle.isValid());
}

QTEST_GUILESS_MAIN(tst_osc)

#include "main.moc"
