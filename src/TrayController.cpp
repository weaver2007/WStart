#include "TrayController.h"

#include <QApplication>
#include <QIcon>

TrayController::TrayController(MainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon(":/app.svg"));
    m_tray->setToolTip(uiText(UiText::Key::TrayTooltip));

    m_menu = new QMenu;
    m_openAction = m_menu->addAction(QString(), m_window, &MainWindow::showSettings);
    m_hotkeysAction = m_menu->addAction(QString());
    m_hotkeysAction->setCheckable(true);
    connect(m_hotkeysAction, &QAction::toggled, m_window, &MainWindow::applyHotkeysEnabled);
    m_menu->addSeparator();
    m_exitAction = m_menu->addAction(QString(), qApp, &QCoreApplication::quit);

    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            m_window->showSettings();
        }
    });
    connect(m_window, &MainWindow::languageChanged, this, &TrayController::retranslateUi);
    connect(m_window, &MainWindow::hotkeysEnabledChanged, this, &TrayController::setHotkeysEnabled);

    retranslateUi();
    setHotkeysEnabled(m_window->hotkeysEnabled());
    m_tray->show();
}

void TrayController::retranslateUi()
{
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

void TrayController::setHotkeysEnabled(bool enabled)
{
    if (!m_hotkeysAction || m_hotkeysAction->isChecked() == enabled) {
        return;
    }
    const QSignalBlocker blocker(m_hotkeysAction);
    m_hotkeysAction->setChecked(enabled);
}

QString TrayController::uiText(UiText::Key key) const
{
    return UiText::text(m_window ? m_window->language() : QString(), key);
}
