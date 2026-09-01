#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class EdgeClient final : public QObject {
    Q_OBJECT

public:
    explicit EdgeClient(QObject* parent = nullptr);
    void sendCommand(const QString& host, quint16 port, const QString& command);
    void setRequestTimeoutMs(int timeout_ms);
    [[nodiscard]] bool busy() const noexcept;

signals:
    void responseReceived(const QString& response);
    void requestFailed(const QString& message);
    void busyChanged(bool busy);

private slots:
    void onConnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void onTimeout();

private:
    void finish();
    QTcpSocket socket_;
    QTimer request_timer_;
    QByteArray pending_command_;
    QByteArray response_buffer_;
    bool busy_{false};
};
