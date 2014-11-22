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
    void process2DCurSource(const QOscMessage &message);
    void process2DCurAlive(const QOscMessage &message);

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

    qDebug() << "Got TUIO source message from: " << arguments.at(1).toByteArray();
}

void TuioSocket::process2DCurAlive(const QOscMessage &message)
{
    QList<QVariant> arguments = message.arguments();
    if (arguments.count() < 1) {
        qWarning() << "Ignoring malformed TUIO alive message: " << arguments.count();
        return;
    }

    for (int i = 1; i < arguments.count(); ++i) {
        if (arguments.at(i).type() != QVariant::Int) {
            qWarning() << "Ignoring malformed TUIO alive message (bad argument on position" << i << arguments << ")";
            return;
        }
        qDebug() << "TUIO object: " << arguments.at(i).toInt() << " is alive: " << arguments;
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TuioSocket sock;

    return app.exec();
}

#include "main.moc"
