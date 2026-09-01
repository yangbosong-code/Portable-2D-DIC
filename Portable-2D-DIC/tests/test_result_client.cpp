#include "result_client.hpp"

#include "p2dic/result_stream_server.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    constexpr std::uint16_t port = 39851;
    p2dic::ResultStreamServer server;
    try {
        server.start(port);
        p2dic::DicResult source;
        source.frame_sequence = 88;
        source.points.push_back(p2dic::DicPoint{
            32, 64, 2.5F, -1.0F, 0.95F, true,
            0.01F, 0.02F, 0.005F, true});
        server.publish(source);

        ResultClient client;
        client.setReconnectIntervalMs(50);
        QEventLoop loop;
        DicResultPtr received;
        QString failure;
        QObject::connect(&client, &ResultClient::resultReceived, [&](DicResultPtr result) {
            received = std::move(result);
            loop.quit();
        });
        QObject::connect(&client, &ResultClient::streamFailed, [&](const QString& message) {
            failure = message;
        });
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        client.start(QStringLiteral("127.0.0.1"), port);
        watchdog.start(2000);
        loop.exec();
        client.stop();
        server.stop();
        if (!received || received->frame_sequence != 88 || received->points.size() != 1 ||
            received->points.front().u != 2.5F) {
            std::cerr << "Qt result client did not receive the published field: "
                      << failure.toStdString() << '\n';
            return 1;
        }
    } catch (const std::exception& exception) {
        server.stop();
        std::cerr << exception.what() << '\n';
        return 2;
    }
    return 0;
}
