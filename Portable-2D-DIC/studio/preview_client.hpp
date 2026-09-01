#pragma once

#include "p2dic/preview_frame.hpp"

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

#include <memory>

using PreviewFramePtr = std::shared_ptr<const p2dic::PreviewFrame>;
Q_DECLARE_METATYPE(PreviewFramePtr)

class PreviewClient final : public QObject {
    Q_OBJECT

public:
    explicit PreviewClient(QObject* parent = nullptr);
    void start(const QString& host, quint16 port);
    void stop();
    void setReconnectIntervalMs(int interval_ms);

signals:
    void previewReceived(PreviewFramePtr preview);
    void onlineChanged(bool online);
    void streamFailed(const QString& message);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void reconnect();

private:
    void scheduleReconnect();
    void failProtocol(const QString& message);

    QTcpSocket socket_;
    QTimer reconnect_timer_;
    QByteArray buffer_;
    QString host_;
    quint16 port_{0};
    bool desired_{false};
    bool online_{false};
};
