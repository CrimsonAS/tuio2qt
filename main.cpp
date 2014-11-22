#include <QCoreApplication>
#include <QUdpSocket>
#include <QLoggingCategory>

#include "qoscbundle_p.h"

Q_LOGGING_CATEGORY(lcTuioSource, "qt.qpa.tuio.source")

class TuioCursor
{
public:
    TuioCursor(int id)
        : m_id(id)
        , m_x(0)
        , m_y(0)
        , m_vx(0)
        , m_vy(0)
        , m_acceleration(0)
    {
    }

    int id() const { return m_id; }

    void setX(float x) { m_x = x; }
    float x() const { return m_x; }

    void setY(float y) { m_y = y; }
    float y() const { return m_y; }

    void setVX(float vx) { m_vx = vx; }
    float vx() const { return m_vx; }

    void setVY(float vy) { m_vy = vy; }
    float vy() const { return m_vy; }

    void setAcceleration(float acceleration) { m_acceleration = acceleration; }
    float acceleration() const { return m_acceleration; }

private:
    int m_id;
    float m_x;
    float m_y;
    float m_vx;
    float m_vy;
    float m_acceleration;
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
            qDebug() << "New TUIO object: " << cursorId << " is alive";
        } else {
            // we already know about it, remove it so it isn't marked as released
            oldActiveCursors.remove(cursorId);
        }

        newActiveCursors.insert(cursorId, TuioCursor(cursorId));
    }

    // anything left is dead now
    QMap<int, TuioCursor>::ConstIterator it = oldActiveCursors.constBegin();
    while (it != oldActiveCursors.constEnd()) {
        qDebug() << "Dead TUIO object: " << it.key();
        ++it;
    }

    m_activeCursors = newActiveCursors;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TuioSocket sock;

    return app.exec();
}

#include "main.moc"
