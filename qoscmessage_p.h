#ifndef QOSCMESSAGE_P_H
#define QOSCMESSAGE_P_H

class QOscMessage
{
public:
    QOscMessage(const QByteArray &data);
    bool isValid() const;

    QByteArray addressPattern() const;
    QList<QVariant> arguments() const;

private:
    bool m_isValid;
    QByteArray m_addressPattern;
    QList<QVariant> m_arguments;
};

#endif
