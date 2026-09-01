#include "edge_client.hpp"

EdgeClient::EdgeClient(QObject* parent) : QObject(parent) {
    request_timer_.setSingleShot(true);
    request_timer_.setInterval(2500);
    connect(&socket_, &QTcpSocket::connected, this, &EdgeClient::onConnected);
    connect(&socket_, &QTcpSocket::readyRead, this, &EdgeClient::onReadyRead);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &EdgeClient::onError);
    connect(&request_timer_, &QTimer::timeout, this, &EdgeClient::onTimeout);
}

void EdgeClient::sendCommand(const QString& host, quint16 port, const QString& command) {
    if (busy_) {
        emit requestFailed(tr("上一条命令仍在执行 / Previous request is still active"));
        return;
    }
    if (host.trimmed().isEmpty() || port == 0 || command.contains('\n') || command.contains('\r')) {
        emit requestFailed(tr("连接参数或命令无效 / Invalid endpoint or command"));
        return;
    }
    busy_ = true;
    emit busyChanged(true);
    pending_command_ = command.toUtf8() + '\n';
    response_buffer_.clear();
    socket_.abort();
    request_timer_.start();
    socket_.connectToHost(host.trimmed(), port);
}

void EdgeClient::setRequestTimeoutMs(int timeout_ms) {
    request_timer_.setInterval(qMax(1, timeout_ms));
}

bool EdgeClient::busy() const noexcept {
    return busy_;
}

void EdgeClient::onConnected() {
    if (socket_.write(pending_command_) < 0) {
        emit requestFailed(tr("命令发送失败 / Command write failed"));
        finish();
    }
}

void EdgeClient::onReadyRead() {
    response_buffer_ += socket_.readAll();
    if (response_buffer_.size() > 8192) {
        emit requestFailed(tr("响应过大 / Response exceeds 8192 bytes"));
        finish();
        return;
    }
    const auto newline = response_buffer_.indexOf('\n');
    if (newline < 0) {
        return;
    }
    const QString response = QString::fromUtf8(response_buffer_.left(newline)).trimmed();
    emit responseReceived(response);
    finish();
}

void EdgeClient::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    if (!busy_) {
        return;
    }
    emit requestFailed(tr("网络错误 / Network error: %1").arg(socket_.errorString()));
    finish();
}

void EdgeClient::onTimeout() {
    if (!busy_) {
        return;
    }
    socket_.abort();
    pending_command_.clear();
    response_buffer_.clear();
    busy_ = false;
    emit busyChanged(false);
    emit requestFailed(tr("请求超时，稍后自动重连 / Request timed out; retrying automatically"));
}

void EdgeClient::finish() {
    request_timer_.stop();
    socket_.disconnectFromHost();
    pending_command_.clear();
    response_buffer_.clear();
    if (busy_) {
        busy_ = false;
        emit busyChanged(false);
    }
}
