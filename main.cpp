#include <QCoreApplication>
#include <QUdpSocket>

#include "qoscbundle_p.h"

class TuioSocket : public QObject
{
    Q_OBJECT

public:
    explicit TuioSocket();
    virtual ~TuioSocket();

private slots:
    void processPackets();

private:
    QUdpSocket m_socket;
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
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TuioSocket sock;

    return app.exec();
}

#include "main.moc"
