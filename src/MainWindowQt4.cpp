#include "AppIcon.h"
#include "MainWindowQt4.h"

#include "UiText.h"

#include <QApplication>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindowQt4::MainWindowQt4(QWidget *parent)
    : QMainWindow(parent)
    , m_list(0)
    , m_status(0)
    , m_hotkeysEnabled(true)
{
    setWindowTitle("HSTART");
    setWindowIcon(AppIcon::launcherIcon());
    resize(360, 560);
    buildUi();
    loadDocument();
    refreshList();

    connect(&m_hookService, SIGNAL(hotkeyTriggered(HotkeyRule)), this, SLOT(onHotkeyTriggered(HotkeyRule)));
    connect(&m_hookService, SIGNAL(hookError(QString)), this, SLOT(setStatus(QString)));

    QString hookError;
    if (!m_hookService.start(&hookError)) {
        QMessageBox::warning(this, "HotKeyManager", hookError);
        setStatus(hookError);
    } else {
        setStatus(UiText::text(language(), UiText::Key::HookRunning));
    }
}

QString MainWindowQt4::language() const
{
    return m_document.settings.language;
}

bool MainWindowQt4::hotkeysEnabled() const
{
    return m_hotkeysEnabled;
}

void MainWindowQt4::showSettings()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindowQt4::setHotkeysPaused(bool paused)
{
    applyHotkeysEnabled(!paused);
}

void MainWindowQt4::applyHotkeysEnabled(bool enabled)
{
    if (m_hotkeysEnabled == enabled) {
        return;
    }
    m_hotkeysEnabled = enabled;
    m_hookService.setPaused(!enabled);
    emit hotkeysEnabledChanged(enabled);
}

void MainWindowQt4::runCurrentItem(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    const QString ruleId = item->data(Qt::UserRole).toString();
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    QString error;
    if (!m_runner.run(m_document.rules.at(index).action, &error)) {
        setStatus(error);
        QMessageBox::warning(this, UiText::text(language(), UiText::Key::LaunchFailed), error);
    } else {
        setStatus(UiText::text(language(), UiText::Key::Launched).arg(m_document.rules.at(index).description));
    }
}

void MainWindowQt4::onHotkeyTriggered(const HotkeyRule &rule)
{
    QString error;
    if (!m_runner.run(rule.action, &error)) {
        setStatus(error);
    } else {
        setStatus(UiText::text(language(), UiText::Key::Launched).arg(rule.description));
    }
}

void MainWindowQt4::setStatus(const QString &message)
{
    if (m_status) {
        m_status->setText(message);
    }
}

void MainWindowQt4::buildUi()
{
    QWidget *root = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(root);
    QLabel *title = new QLabel("HSTART", root);
    QFont titleFont = title->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    m_list = new QListWidget(root);
    m_list->setIconSize(QSize(32, 32));
    connect(m_list, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(runCurrentItem(QListWidgetItem*)));
    layout->addWidget(m_list, 1);

    m_status = new QLabel(root);
    layout->addWidget(m_status);

    setCentralWidget(root);
}

void MainWindowQt4::loadDocument()
{
    QString error;
    m_document = m_store.loadDocument(&error);
    if (!error.isEmpty()) {
        setStatus(error);
    }
    m_hotkeysEnabled = m_document.settings.hotkeysEnabled;
    m_hookService.setPaused(!m_hotkeysEnabled);
    m_hookService.setRules(m_document.rules);
}

void MainWindowQt4::refreshList()
{
    m_list->clear();
    for (int i = 0; i < m_document.rules.size(); ++i) {
        const HotkeyRule &rule = m_document.rules.at(i);
        QListWidgetItem *item = new QListWidgetItem(AppIcon::launcherIcon(), rule.description.isEmpty() ? rule.action.target : rule.description);
        item->setData(Qt::UserRole, rule.id);
        if (rule.hotkey.isValid()) {
            item->setToolTip(rule.hotkey.displayText());
        }
        m_list->addItem(item);
    }
}

int MainWindowQt4::ruleIndexById(const QString &ruleId) const
{
    for (int i = 0; i < m_document.rules.size(); ++i) {
        if (m_document.rules.at(i).id == ruleId) {
            return i;
        }
    }
    return -1;
}
