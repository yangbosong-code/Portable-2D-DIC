#include "edge_client.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTcpServer>
#include <QTimer>

#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    QTcpServer silent_server;
    if (!silent_server.listen(QHostAddress::LocalHost, 0)) {
        std::cerr << "Could not start silent TCP test server\n";
        return 1;
    }

    EdgeClient client;
    client.setRequestTimeoutMs(150);
    QEventLoop loop;
    QString failure;
    bool connected = false;
    QObject::connect(&silent_server, &QTcpServer::newConnection, [&] {
        connected = true;
        // Keep the accepted socket open without replying to exercise the
        // client's response timeout instead of the connection error path.
        silent_server.nextPendingConnection()->setParent(&silent_server);
    });
    QObject::connect(&client, &EdgeClient::requestFailed, [&](const QString& message) {
        failure = message;
        loop.quit();
    });
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);

    client.sendCommand(QStringLiteral("127.0.0.1"), silent_server.serverPort(),
                       QStringLiteral("STATUS"));
    watchdog.start(2000);
    loop.exec();

    if (!connected || client.busy() || !failure.contains(QStringLiteral("timed out"))) {
        std::cerr << "EdgeClient did not recover from a silent server\n";
        return 2;
    }
    return 0;
}
