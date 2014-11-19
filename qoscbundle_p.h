#ifndef QOSCBUNDLE_P_H
#define QOSCBUNDLE_P_H

#include "qoscmessage_p.h"

class QOscBundle
{
public:
    QOscBundle(const QByteArray &data);
    bool isValid() const;

private:
    bool m_isValid;
    bool m_immediate;
    quint32 m_timeEpoch;
    quint32 m_timePico;
    QList<QOscBundle> m_bundles;
    QList<QOscMessage> m_messages;
};

#endif // QOSCBUNDLE_P_H
