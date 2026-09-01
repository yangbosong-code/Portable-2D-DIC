#include "preview_client.hpp"

#include "p2dic/result_stream_server.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    constexpr std::uint16_t port = 39853;
    p2dic::ResultStreamServer server;
    try {
        server.start(port);
        p2dic::PreviewFrame source;
        source.width = 32;
        source.height = 24;
        source.sequence = 17;
        source.pixels.assign(static_cast<std::size_t>(source.width) * source.height, 123);
        server.publish_packet(p2dic::encode_preview_packet(source));

        PreviewClient client;
        QEventLoop loop;
        PreviewFramePtr received;
        QObject::connect(&client, &PreviewClient::previewReceived, [&](PreviewFramePtr preview) {
            received = std::move(preview);
            loop.quit();
        });
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        client.start(QStringLiteral("127.0.0.1"), port);
        watchdog.start(2000);
        loop.exec();
        client.stop();
        server.stop();
        if (!received || received->sequence != 17 || received->pixels.size() != 768) {
            std::cerr << "Qt preview client did not receive the frame\n";
            return 1;
        }
    } catch (const std::exception& exception) {
        server.stop();
        std::cerr << exception.what() << '\n';
        return 2;
    }
    return 0;
}
