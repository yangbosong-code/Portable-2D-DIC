#include "result_client.hpp"

#include "p2dic/result_packet.hpp"

#include <algorithm>
#include <span>

namespace {

quint32 packetSize(const QByteArray& buffer) {
    const auto byte = [&buffer](int index) {
        return static_cast<quint32>(static_cast<unsigned char>(buffer[index]));
    };
    return byte(8) | (byte(9) << 8U) | (byte(10) << 16U) | (byte(11) << 24U);
}

constexpr quint32 maximumPacketSize =
    p2dic::result_packet_header_size + 1'000'000U * p2dic::result_packet_point_size;

}  // namespace

ResultClient::ResultClient(QObject* parent) : QObject(parent) {
    qRegisterMetaType<DicResultPtr>("DicResultPtr");
    reconnect_timer_.setSingleShot(true);
    reconnect_timer_.setInterval(1000);
    connect(&socket_, &QTcpSocket::connected, this, &ResultClient::onConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &ResultClient::onDisconnected);
    connect(&socket_, &QTcpSocket::readyRead, this, &ResultClient::onReadyRead);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &ResultClient::onError);
    connect(&reconnect_timer_, &QTimer::timeout, this, &ResultClient::reconnect);
}

void ResultClient::start(const QString& host, quint16 port) {
    const QString normalized = host.trimmed();
    if (normalized.isEmpty() || port == 0) {
        emit streamFailed(tr("结果流地址无效 / Invalid result stream endpoint"));
        return;
    }
    const bool same_endpoint = desired_ && normalized == host_ && port == port_;
    host_ = normalized;
    port_ = port;
    desired_ = true;
    if (same_endpoint && socket_.state() != QAbstractSocket::UnconnectedState) return;
    reconnect_timer_.stop();
    buffer_.clear();
    socket_.abort();
    socket_.connectToHost(host_, port_);
}

void ResultClient::stop() {
    desired_ = false;
    reconnect_timer_.stop();
    buffer_.clear();
    socket_.abort();
    if (online_) {
        online_ = false;
        emit onlineChanged(false);
    }
}

void ResultClient::setReconnectIntervalMs(int interval_ms) {
    reconnect_timer_.setInterval(std::max(1, interval_ms));
}

bool ResultClient::online() const noexcept { return online_; }

void ResultClient::onConnected() {
    if (!online_) {
        online_ = true;
        emit onlineChanged(true);
    }
}

void ResultClient::onDisconnected() {
    if (online_) {
        online_ = false;
        emit onlineChanged(false);
    }
    buffer_.clear();
    scheduleReconnect();
}

void ResultClient::onReadyRead() {
    buffer_ += socket_.readAll();
    while (true) {
        if (buffer_.size() < static_cast<int>(p2dic::result_packet_header_size)) return;
        const quint32 size = packetSize(buffer_);
        if (size < p2dic::result_packet_header_size || size > maximumPacketSize) {
            failProtocol(tr("结果包长度无效 / Invalid result packet length"));
            return;
        }
        if (buffer_.size() < static_cast<int>(size)) return;
        const QByteArray packet = buffer_.left(static_cast<int>(size));
        buffer_.remove(0, static_cast<int>(size));
        try {
            const auto bytes = std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(packet.constData()),
                static_cast<std::size_t>(packet.size()));
            emit resultReceived(std::make_shared<const p2dic::DicResult>(
                p2dic::decode_result_packet(bytes)));
        } catch (const std::exception& exception) {
            failProtocol(tr("结果包校验失败 / Result packet verification failed: %1")
                             .arg(QString::fromUtf8(exception.what())));
            return;
        }
    }
}

void ResultClient::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    if (!desired_) return;
    emit streamFailed(tr("结果流网络错误 / Result stream error: %1").arg(socket_.errorString()));
    scheduleReconnect();
}

void ResultClient::reconnect() {
    if (!desired_ || socket_.state() != QAbstractSocket::UnconnectedState) return;
    socket_.connectToHost(host_, port_);
}

void ResultClient::scheduleReconnect() {
    if (desired_ && !reconnect_timer_.isActive()) reconnect_timer_.start();
}

void ResultClient::failProtocol(const QString& message) {
    emit streamFailed(message);
    buffer_.clear();
    socket_.abort();
    scheduleReconnect();
}
