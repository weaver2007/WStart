#include "MainWindow.h"
#include "TrayController.h"
#include "UiText.h"

#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QSystemTrayIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("HotKeyManager");
    QApplication::setOrganizationName("HotKeyManager");
    QApplication::setWindowIcon(QIcon(":/app.svg"));
    QApplication::setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::warning(nullptr, "HotKeyManager", UiText::text("zh-CN", UiText::Key::SystemTrayUnavailable));
    }

    MainWindow window;
    TrayController tray(&window);
    window.show();

    return app.exec();
}
