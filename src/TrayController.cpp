#include "TrayController.h"

#include "AppIcon.h"
#include "LauncherWindowInterface.h"
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include "MainWindow.h"
#endif

#include <QApplication>
#include <QIcon>
#include <QSignalBlocker>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
TrayController::TrayController(MainWindow* window, QObject* parent) : TrayController(window, window, parent) {}
#endif

TrayController::TrayController(QMainWindow* window, LauncherWindowInterface* launcher, QObject* parent)
    : QObject(parent), m_window(window), m_launcher(launcher) {
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(AppIcon::launcherIcon());
    m_tray->setToolTip(uiText(UiText::Key::TrayTooltip));

    m_menu = new QMenu;
    m_openAction = m_menu->addAction(QString(), m_window, SLOT(showSettings()));
    m_hotkeysAction = m_menu->addAction(QString());
    m_hotkeysAction->setCheckable(true);
    connect(m_hotkeysAction, SIGNAL(toggled(bool)), m_window, SLOT(applyHotkeysEnabled(bool)));
    m_menu->addSeparator();
    m_exitAction = m_menu->addAction(QString(), qApp, SLOT(quit()));

    m_tray->setContextMenu(m_menu);
    connect(m_tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), m_window, SLOT(showSettings()));
    connect(m_window, SIGNAL(languageChanged(QString)), this, SLOT(retranslateUi()));
    connect(m_window, SIGNAL(hotkeysEnabledChanged(bool)), this, SLOT(setHotkeysEnabled(bool)));

    retranslateUi();
    setHotkeysEnabled(m_launcher ? m_launcher->hotkeysEnabled() : true);
    m_tray->show();
}

void TrayController::retranslateUi() {
    if (m_tray) {
        m_tray->setToolTip(uiText(UiText::Key::TrayTooltip));
    }
    if (m_openAction) {
        m_openAction->setText(uiText(UiText::Key::OpenMainWindow));
    }
    if (m_hotkeysAction) {
        m_hotkeysAction->setText(uiText(UiText::Key::HotkeysEnabled));
    }
    if (m_exitAction) {
        m_exitAction->setText(uiText(UiText::Key::Exit));
    }
}

void TrayController::setHotkeysEnabled(bool enabled) {
    if (!m_hotkeysAction || m_hotkeysAction->isChecked() == enabled) {
        return;
    }
    const QSignalBlocker blocker(m_hotkeysAction);
    m_hotkeysAction->setChecked(enabled);
}

QString TrayController::uiText(UiText::Key key) const {
    return UiText::text(m_launcher ? m_launcher->language() : QString(), key);
}
