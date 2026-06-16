#include "RuleDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QUuid>
#include <QVBoxLayout>

RuleDialog::RuleDialog(QWidget *parent)
    : RuleDialog("zh-CN", parent)
{
}

RuleDialog::RuleDialog(const QString &language, QWidget *parent)
    : QDialog(parent)
    , m_language(UiText::normalizeLanguage(language))
{
    resize(520, 300);

    m_hotkeyEdit = new HotkeyEdit(this);
    m_targetEdit = new QLineEdit(this);
    m_argumentsEdit = new QLineEdit(this);
    m_workingDirectoryEdit = new QLineEdit(this);
    m_descriptionEdit = new QLineEdit(this);

    auto *recordButton = new QPushButton(uiText(UiText::Key::Record), this);
    connect(recordButton, &QPushButton::clicked, m_hotkeyEdit, &HotkeyEdit::beginRecording);

    m_browseButton = new QPushButton(uiText(UiText::Key::Browse), this);
    connect(m_browseButton, &QPushButton::clicked, this, &RuleDialog::browseTarget);

    auto *hotkeyLayout = new QHBoxLayout;
    hotkeyLayout->addWidget(m_hotkeyEdit, 1);
    hotkeyLayout->addWidget(recordButton);

    auto *targetLayout = new QHBoxLayout;
    targetLayout->addWidget(m_targetEdit);
    targetLayout->addWidget(m_browseButton);

    m_argumentsRow = new QWidget(this);
    auto *argumentsLayout = new QHBoxLayout(m_argumentsRow);
    argumentsLayout->setContentsMargins(0, 0, 0, 0);
    argumentsLayout->addWidget(m_argumentsEdit);

    m_workingDirectoryRow = new QWidget(this);
    auto *workingDirectoryLayout = new QHBoxLayout(m_workingDirectoryRow);
    workingDirectoryLayout->setContentsMargins(0, 0, 0, 0);
    workingDirectoryLayout->addWidget(m_workingDirectoryEdit);

    auto *form = new QFormLayout;
    form->addRow(uiText(UiText::Key::Description), m_descriptionEdit);
    form->addRow(uiText(UiText::Key::Target), targetLayout);
    form->addRow(uiText(UiText::Key::Arguments), m_argumentsRow);
    form->addRow(uiText(UiText::Key::WorkingDirectory), m_workingDirectoryRow);
    form->addRow(uiText(UiText::Key::Hotkey), hotkeyLayout);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(UiText::Key::Ok));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(UiText::Key::Cancel));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    updateUiForCategory();
}

void RuleDialog::setContext(LauncherCategory category, const QString &sectionId)
{
    m_category = category;
    m_sectionId = sectionId;
    updateUiForCategory();
}

void RuleDialog::setRule(const HotkeyRule &rule)
{
    m_rule = rule;
    m_category = rule.category;
    m_sectionId = rule.sectionId;
    m_hotkeyEdit->setHotkey(rule.hotkey);
    m_targetEdit->setText(rule.action.target);
    m_argumentsEdit->setText(rule.action.arguments);
    m_workingDirectoryEdit->setText(rule.action.workingDirectory);
    m_descriptionEdit->setText(rule.description);
    updateUiForCategory();
}

HotkeyRule RuleDialog::rule() const
{
    HotkeyRule result = m_rule;
    if (result.id.isEmpty()) {
        result.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    result.enabled = true;
    result.category = m_category;
    result.sectionId = m_sectionId;
    result.hotkey = m_hotkeyEdit->hotkey();
    result.action.type = actionTypeForCategory();
    result.action.target = m_targetEdit->text().trimmed();
    result.action.arguments = m_category == LauncherCategory::Program ? m_argumentsEdit->text().trimmed() : QString();
    result.action.workingDirectory = m_category == LauncherCategory::Program ? m_workingDirectoryEdit->text().trimmed() : QString();
    result.description = m_descriptionEdit->text().trimmed();
    return result;
}

void RuleDialog::browseTarget()
{
    QString selected;
    if (m_category == LauncherCategory::Folder) {
        selected = QFileDialog::getExistingDirectory(this, uiText(UiText::Key::SelectFolder));
    } else if (m_category == LauncherCategory::Website) {
        return;
    } else {
        selected = QFileDialog::getOpenFileName(this, uiText(UiText::Key::SelectProgram));
    }
    if (!selected.isEmpty()) {
        m_targetEdit->setText(selected);
    }
}

QString RuleDialog::uiText(UiText::Key key) const
{
    return UiText::text(m_language, key);
}

void RuleDialog::updateUiForCategory()
{
    QString title;
    QString placeholder;
    switch (m_category) {
    case LauncherCategory::Program:
        title = uiText(UiText::Key::RuleDialogProgramTitle);
        placeholder = uiText(UiText::Key::ProgramTargetPlaceholder);
        m_browseButton->setVisible(true);
        m_argumentsRow->setVisible(true);
        m_workingDirectoryRow->setVisible(true);
        break;
    case LauncherCategory::Folder:
        title = uiText(UiText::Key::RuleDialogFolderTitle);
        placeholder = uiText(UiText::Key::FolderTargetPlaceholder);
        m_browseButton->setVisible(true);
        m_argumentsRow->setVisible(false);
        m_workingDirectoryRow->setVisible(false);
        break;
    case LauncherCategory::Website:
        title = uiText(UiText::Key::RuleDialogWebsiteTitle);
        placeholder = uiText(UiText::Key::WebsiteTargetPlaceholder);
        m_browseButton->setVisible(false);
        m_argumentsRow->setVisible(false);
        m_workingDirectoryRow->setVisible(false);
        break;
    }
    setWindowTitle(title);
    m_targetEdit->setPlaceholderText(placeholder);
    m_descriptionEdit->setPlaceholderText(uiText(UiText::Key::DescriptionPlaceholder));
    m_argumentsEdit->setPlaceholderText(uiText(UiText::Key::ArgumentsPlaceholder));
    m_workingDirectoryEdit->setPlaceholderText(uiText(UiText::Key::WorkingDirectoryPlaceholder));
    m_hotkeyEdit->setPlaceholderText(uiText(UiText::Key::HotkeyPlaceholder));
}

LaunchActionType RuleDialog::actionTypeForCategory() const
{
    switch (m_category) {
    case LauncherCategory::Program:
        return LaunchActionType::Application;
    case LauncherCategory::Folder:
        return LaunchActionType::Folder;
    case LauncherCategory::Website:
        return LaunchActionType::Url;
    }
    return LaunchActionType::Application;
}
