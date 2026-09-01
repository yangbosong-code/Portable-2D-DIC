#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QPixmap>
#include <QStringList>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Portable2DDIC"));
    QCoreApplication::setApplicationName(QStringLiteral("DIC Studio"));
    const QStringList arguments = application.arguments();
    const qsizetype snapshot_index = arguments.indexOf(QStringLiteral("--ui-snapshot"));
    application.setProperty("studioSnapshotWorkspace",
                            arguments.contains(QStringLiteral("--snapshot-workspace")));
    MainWindow window;
    if (snapshot_index >= 0 && snapshot_index + 1 < arguments.size()) {
        window.resize(1480, 920);
        window.show();
        window.ensurePolished();
        return window.grab().save(arguments.at(snapshot_index + 1), "PNG") ? 0 : 2;
    }
    window.show();
    // Force creation of the native window before entering the event loop. This
    // keeps first launch reliable on Windows shells that defer top-level HWNDs.
    window.winId();
    window.raise();
    window.activateWindow();
    QTimer::singleShot(0, &window, [&window] {
        window.show();
        window.raise();
        window.activateWindow();
    });
    return application.exec();
}
