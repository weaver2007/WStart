#include "RuleDialog.h"

#include "HotkeyConflictDetector.h"
#include "PathUtils.h"
#include "QtCompat.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QUuid>
#include <QVBoxLayout>

namespace {
constexpr int kVkBack = 0x08;
constexpr int kVkTab = 0x09;
constexpr int kVkReturn = 0x0D;
constexpr int kVkEscape = 0x1B;
constexpr int kVkSpace = 0x20;
constexpr int kVkPrior = 0x21;
constexpr int kVkNext = 0x22;
constexpr int kVkEnd = 0x23;
constexpr int kVkHome = 0x24;
constexpr int kVkLeft = 0x25;
constexpr int kVkUp = 0x26;
constexpr int kVkRight = 0x27;
constexpr int kVkDown = 0x28;
constexpr int kVkInsert = 0x2D;
constexpr int kVkDelete = 0x2E;
constexpr int kVkF1 = 0x70;
constexpr int kVkOem1 = 0xBA;
constexpr int kVkOemPlus = 0xBB;
constexpr int kVkOemComma = 0xBC;
constexpr int kVkOemMinus = 0xBD;
constexpr int kVkOemPeriod = 0xBE;
constexpr int kVkOem2 = 0xBF;
constexpr int kVkOem3 = 0xC0;
constexpr int kVkOem4 = 0xDB;
constexpr int kVkOem5 = 0xDC;
constexpr int kVkOem6 = 0xDD;
constexpr int kVkOem7 = 0xDE;

bool targetLooksLikeExe(const QString& target) {
    return QFileInfo(target.trimmed()).suffix().compare("exe", Qt::CaseInsensitive) == 0;
}

int manualKeyFromText(const QString& rawText) {
    const QString text = rawText.trimmed();
    if (text.isEmpty()) {
        return 0;
    }

    const QString upper = text.toUpper();
    if (upper.size() == 1) {
        const ushort ch = upper.at(0).unicode();
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            return static_cast<int>(ch);
        }
        switch (ch) {
        case ' ':
            return kVkSpace;
        case '-':
            return kVkOemMinus;
        case '=':
            return kVkOemPlus;
        case '[':
            return kVkOem4;
        case ']':
            return kVkOem6;
        case '\\':
            return kVkOem5;
        case ';':
            return kVkOem1;
        case '\'':
            return kVkOem7;
        case ',':
            return kVkOemComma;
        case '.':
            return kVkOemPeriod;
        case '/':
            return kVkOem2;
        case '`':
            return kVkOem3;
        default:
            break;
        }
    }

    if (upper.startsWith(QString::fromLatin1("F"))) {
        bool ok = false;
        const int number = upper.mid(1).toInt(&ok);
        if (ok && number >= 1 && number <= 24) {
            return kVkF1 + number - 1;
        }
    }
    if (upper.startsWith(QString::fromLatin1("VK_"))) {
        bool ok = false;
        const int key = upper.mid(3).toInt(&ok, 0);
        return ok ? key : 0;
    }

    if (upper == QString::fromLatin1("SPACE") || text == QString::fromUtf8("空格")) {
        return kVkSpace;
    }
    if (upper == QString::fromLatin1("TAB")) {
        return kVkTab;
    }
    if (upper == QString::fromLatin1("ESC") || upper == QString::fromLatin1("ESCAPE")) {
        return kVkEscape;
    }
    if (upper == QString::fromLatin1("ENTER") || upper == QString::fromLatin1("RETURN")) {
        return kVkReturn;
    }
    if (upper == QString::fromLatin1("BACKSPACE") || upper == QString::fromLatin1("BACK")) {
        return kVkBack;
    }
    if (upper == QString::fromLatin1("DELETE") || upper == QString::fromLatin1("DEL")) {
        return kVkDelete;
    }
    if (upper == QString::fromLatin1("INSERT") || upper == QString::fromLatin1("INS")) {
        return kVkInsert;
    }
    if (upper == QString::fromLatin1("HOME")) {
        return kVkHome;
    }
    if (upper == QString::fromLatin1("END")) {
        return kVkEnd;
    }
    if (upper == QString::fromLatin1("PAGEUP") || upper == QString::fromLatin1("PGUP")) {
        return kVkPrior;
    }
    if (upper == QString::fromLatin1("PAGEDOWN") || upper == QString::fromLatin1("PGDN")) {
        return kVkNext;
    }
    if (upper == QString::fromLatin1("LEFT")) {
        return kVkLeft;
    }
    if (upper == QString::fromLatin1("RIGHT")) {
        return kVkRight;
    }
    if (upper == QString::fromLatin1("UP")) {
        return kVkUp;
    }
    if (upper == QString::fromLatin1("DOWN")) {
        return kVkDown;
    }
    return 0;
}

QString manualKeyText(int virtualKey) {
    if ((virtualKey >= 'A' && virtualKey <= 'Z') || (virtualKey >= '0' && virtualKey <= '9')) {
        return QString(QChar(static_cast<ushort>(virtualKey)));
    }
    if (virtualKey >= kVkF1 && virtualKey <= kVkF1 + 23) {
        return QString::fromLatin1("F%1").arg(virtualKey - kVkF1 + 1);
    }
    switch (virtualKey) {
    case kVkSpace:
        return QString::fromLatin1("Space");
    case kVkTab:
        return QString::fromLatin1("Tab");
    case kVkEscape:
        return QString::fromLatin1("Esc");
    case kVkReturn:
        return QString::fromLatin1("Enter");
    case kVkBack:
        return QString::fromLatin1("Backspace");
    case kVkDelete:
        return QString::fromLatin1("Delete");
    case kVkInsert:
        return QString::fromLatin1("Insert");
    case kVkHome:
        return QString::fromLatin1("Home");
    case kVkEnd:
        return QString::fromLatin1("End");
    case kVkPrior:
        return QString::fromLatin1("PageUp");
    case kVkNext:
        return QString::fromLatin1("PageDown");
    case kVkLeft:
        return QString::fromLatin1("Left");
    case kVkRight:
        return QString::fromLatin1("Right");
    case kVkUp:
        return QString::fromLatin1("Up");
    case kVkDown:
        return QString::fromLatin1("Down");
    case kVkOemMinus:
        return QString::fromLatin1("-");
    case kVkOemPlus:
        return QString::fromLatin1("=");
    case kVkOem4:
        return QString::fromLatin1("[");
    case kVkOem6:
        return QString::fromLatin1("]");
    case kVkOem5:
        return QString::fromLatin1("\\");
    case kVkOem1:
        return QString::fromLatin1(";");
    case kVkOem7:
        return QString::fromLatin1("'");
    case kVkOemComma:
        return QString::fromLatin1(",");
    case kVkOemPeriod:
        return QString::fromLatin1(".");
    case kVkOem2:
        return QString::fromLatin1("/");
    case kVkOem3:
        return QString::fromLatin1("`");
    default:
        break;
    }
    return virtualKey > 0 ? QString::fromLatin1("VK_%1").arg(virtualKey) : QString();
}
} // namespace

RuleDialog::RuleDialog(QWidget* parent) : RuleDialog("zh-CN", parent) {}

RuleDialog::RuleDialog(const QString& language, QWidget* parent)
    : QDialog(parent), m_language(UiText::normalizeLanguage(language)) {
    resize(520, 340);

    m_ctrlCheck = new QCheckBox(QString::fromLatin1("Ctrl"), this);
    m_altCheck = new QCheckBox(QString::fromLatin1("Alt"), this);
    m_shiftCheck = new QCheckBox(QString::fromLatin1("Shift"), this);
    m_winCheck = new QCheckBox(QString::fromLatin1("Win"), this);
    m_hotkeyKeyEdit = new QLineEdit(this);
    m_hotkeyKeyEdit->setFixedWidth(72);
    m_hotkeyKeyEdit->setMaxLength(16);

    m_targetEdit = new QLineEdit(this);
    m_argumentsEdit = new QLineEdit(this);
    m_workingDirectoryEdit = new QLineEdit(this);
    m_descriptionEdit = new QLineEdit(this);
    m_windowStateCombo = new QComboBox(this);
    m_windowStateCombo->addItem(uiText(UiText::Key::WindowStateNormal), QString::fromLatin1("Normal"));
    m_windowStateCombo->addItem(uiText(UiText::Key::WindowStateMinimized), QString::fromLatin1("Minimized"));
    m_windowStateCombo->addItem(uiText(UiText::Key::WindowStateMaximized), QString::fromLatin1("Maximized"));
    m_singleInstanceCheck = new QCheckBox(uiText(UiText::Key::SingleInstance), this);
    connect(m_targetEdit, SIGNAL(textChanged(QString)), this, SLOT(updateSingleInstanceAvailability()));

    connect(m_ctrlCheck, SIGNAL(toggled(bool)), this, SLOT(updateManualHotkeyPreview()));
    connect(m_altCheck, SIGNAL(toggled(bool)), this, SLOT(updateManualHotkeyPreview()));
    connect(m_shiftCheck, SIGNAL(toggled(bool)), this, SLOT(updateManualHotkeyPreview()));
    connect(m_winCheck, SIGNAL(toggled(bool)), this, SLOT(updateManualHotkeyPreview()));
    connect(m_hotkeyKeyEdit, SIGNAL(textChanged(QString)), this, SLOT(updateManualHotkeyPreview()));
    connect(m_hotkeyKeyEdit, SIGNAL(editingFinished()), this, SLOT(applyManualHotkey()));

    auto* applyHotkeyButton = new QPushButton(uiText(UiText::Key::Apply), this);
    connect(applyHotkeyButton, SIGNAL(clicked()), this, SLOT(applyManualHotkey()));
    auto* checkHotkeyButton = new QPushButton(uiText(UiText::Key::HotkeyCheck), this);
    connect(checkHotkeyButton, SIGNAL(clicked()), this, SLOT(checkHotkeyOccupancy()));

    m_browseButton = new QPushButton(uiText(UiText::Key::Browse), this);
    connect(m_browseButton, SIGNAL(clicked()), this, SLOT(browseTarget()));

    auto* hotkeyLayout = new QHBoxLayout;
    hotkeyLayout->addWidget(m_ctrlCheck);
    hotkeyLayout->addWidget(m_altCheck);
    hotkeyLayout->addWidget(m_shiftCheck);
    hotkeyLayout->addWidget(m_winCheck);
    hotkeyLayout->addWidget(new QLabel(QString::fromLatin1("+"), this));
    hotkeyLayout->addWidget(m_hotkeyKeyEdit);
    hotkeyLayout->addWidget(applyHotkeyButton);
    hotkeyLayout->addWidget(checkHotkeyButton);
    hotkeyLayout->addStretch();

    auto* targetLayout = new QHBoxLayout;
    targetLayout->addWidget(m_targetEdit);
    targetLayout->addWidget(m_browseButton);

    m_argumentsRow = new QWidget(this);
    auto* argumentsLayout = new QHBoxLayout(m_argumentsRow);
    argumentsLayout->setContentsMargins(0, 0, 0, 0);
    argumentsLayout->addWidget(m_argumentsEdit);

    m_workingDirectoryRow = new QWidget(this);
    auto* workingDirectoryLayout = new QHBoxLayout(m_workingDirectoryRow);
    workingDirectoryLayout->setContentsMargins(0, 0, 0, 0);
    workingDirectoryLayout->addWidget(m_workingDirectoryEdit);

    m_singleInstanceRow = new QWidget(this);
    auto* singleInstanceLayout = new QHBoxLayout(m_singleInstanceRow);
    singleInstanceLayout->setContentsMargins(0, 0, 0, 0);
    singleInstanceLayout->addWidget(m_singleInstanceCheck);
    singleInstanceLayout->addStretch();

    auto* form = new QFormLayout;
    form->addRow(uiText(UiText::Key::Description), m_descriptionEdit);
    form->addRow(uiText(UiText::Key::Target), targetLayout);
    form->addRow(uiText(UiText::Key::WindowState), m_windowStateCombo);
    form->addRow(QString(), m_singleInstanceRow);
    form->addRow(uiText(UiText::Key::Arguments), m_argumentsRow);
    form->addRow(uiText(UiText::Key::WorkingDirectory), m_workingDirectoryRow);
    form->addRow(uiText(UiText::Key::Hotkey), hotkeyLayout);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(UiText::Key::Ok));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(UiText::Key::Cancel));
    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    updateUiForCategory();
}

void RuleDialog::setContext(LauncherCategory category, const QString& sectionId) {
    m_category = category;
    m_sectionId = sectionId;
    updateUiForCategory();
}

void RuleDialog::setRule(const HotkeyRule& rule) {
    m_rule = rule;
    m_category = rule.category;
    m_sectionId = rule.sectionId;
    setManualHotkey(rule.hotkey);
    m_targetEdit->setText(rule.action.target);
    m_argumentsEdit->setText(rule.action.arguments);
    m_workingDirectoryEdit->setText(rule.action.workingDirectory);
    setSelectedWindowState(rule.action.windowState);
    m_singleInstanceCheck->setChecked(rule.action.singleInstance);
    m_descriptionEdit->setText(rule.description);
    updateUiForCategory();
}

HotkeyRule RuleDialog::rule() const {
    HotkeyRule result = m_rule;
    if (result.id.isEmpty()) {
        result.id = QtCompat::uuidWithoutBraces();
    }
    result.enabled = true;
    result.category = m_category;
    result.sectionId = m_sectionId;
    result.hotkey = manualHotkey();
    result.action.type = actionTypeForCategory();
    result.action.target = m_targetEdit->text().trimmed();
    result.action.arguments = m_category == LauncherCategory::Program ? m_argumentsEdit->text().trimmed() : QString();
    result.action.workingDirectory =
        m_category == LauncherCategory::Program ? m_workingDirectoryEdit->text().trimmed() : QString();
    result.action.windowState = selectedWindowState();
    result.action.singleInstance = m_category == LauncherCategory::Program &&
                                   targetLooksLikeExe(result.action.target) && m_singleInstanceCheck->isChecked();
    if (m_category == LauncherCategory::Program && result.action.workingDirectory.isEmpty()) {
        const QFileInfo targetInfo(PathUtils::toAbsolutePath(result.action.target));
        if (targetInfo.suffix().compare("exe", Qt::CaseInsensitive) == 0 && !targetInfo.absolutePath().isEmpty()) {
            result.action.workingDirectory = targetInfo.absolutePath();
        }
    }
    result.description = m_descriptionEdit->text().trimmed();
    return result;
}

void RuleDialog::browseTarget() {
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
        if (m_category == LauncherCategory::Program) {
            const QFileInfo selectedInfo(selected);
            if (selectedInfo.suffix().compare("exe", Qt::CaseInsensitive) == 0 &&
                m_descriptionEdit->text().trimmed().isEmpty()) {
                m_descriptionEdit->setText(selectedInfo.completeBaseName());
            }
            if (selectedInfo.suffix().compare("exe", Qt::CaseInsensitive) == 0 &&
                m_workingDirectoryEdit->text().trimmed().isEmpty()) {
                m_workingDirectoryEdit->setText(selectedInfo.absolutePath());
            }
        }
    }
}

void RuleDialog::checkHotkeyOccupancy() {
    const HotkeyConflictDetector::Result result = HotkeyConflictDetector::check(manualHotkey(), m_language);
    const QMessageBox::Icon icon = result.availability == HotkeyConflictDetector::Availability::Available
                                       ? QMessageBox::Information
                                       : QMessageBox::Warning;
    QMessageBox box(icon, uiText(UiText::Key::HotkeyCheck), result.notes.join("\n"), QMessageBox::Ok, this);
    if (QAbstractButton* button = box.button(QMessageBox::Ok)) {
        button->setText(uiText(UiText::Key::Ok));
    }
    box.exec();
}

void RuleDialog::updateSingleInstanceAvailability() {
    const bool available = m_category == LauncherCategory::Program && targetLooksLikeExe(m_targetEdit->text());
    m_singleInstanceCheck->setEnabled(available);
    if (!available) {
        m_singleInstanceCheck->setChecked(false);
    }
}
void RuleDialog::applyManualHotkey() {
    if (!m_hotkeyKeyEdit) {
        return;
    }

    const QString typedKey = m_hotkeyKeyEdit->text().trimmed();
    if (typedKey.isEmpty()) {
        setManualHotkey(HotkeyCombination());
        return;
    }

    const HotkeyCombination hotkey = manualHotkey();
    if (!hotkey.isValid()) {
        QMessageBox::warning(this, uiText(UiText::Key::HotkeyWarning), uiText(UiText::Key::HotkeyManualInvalid));
        m_hotkeyKeyEdit->setFocus();
        m_hotkeyKeyEdit->selectAll();
        return;
    }
    setManualHotkey(hotkey);
}

void RuleDialog::updateManualHotkeyPreview() {
    if (!m_hotkeyKeyEdit) {
        return;
    }

    const HotkeyCombination hotkey = manualHotkey();
    m_hotkeyKeyEdit->setToolTip(hotkey.isValid() ? hotkey.displayText() : uiText(UiText::Key::HotkeyPlaceholder));
}

HotkeyCombination RuleDialog::manualHotkey() const {
    HotkeyCombination hotkey;
    const int key = m_hotkeyKeyEdit ? manualKeyFromText(m_hotkeyKeyEdit->text()) : 0;
    if (key <= 0) {
        return hotkey;
    }

    HotkeyModifiers modifiers = ModifierNone;
    if (m_ctrlCheck && m_ctrlCheck->isChecked()) {
        modifiers |= ModifierCtrl;
    }
    if (m_altCheck && m_altCheck->isChecked()) {
        modifiers |= ModifierAlt;
    }
    if (m_shiftCheck && m_shiftCheck->isChecked()) {
        modifiers |= ModifierShift;
    }
    if (m_winCheck && m_winCheck->isChecked()) {
        modifiers |= ModifierWin;
    }
    hotkey.modifiers = modifiers;
    hotkey.key = key;
    return hotkey;
}

void RuleDialog::setManualHotkey(const HotkeyCombination& hotkey) {
    if (m_ctrlCheck) {
        m_ctrlCheck->setChecked(hotkey.modifiers.testFlag(ModifierCtrl));
    }
    if (m_altCheck) {
        m_altCheck->setChecked(hotkey.modifiers.testFlag(ModifierAlt));
    }
    if (m_shiftCheck) {
        m_shiftCheck->setChecked(hotkey.modifiers.testFlag(ModifierShift));
    }
    if (m_winCheck) {
        m_winCheck->setChecked(hotkey.modifiers.testFlag(ModifierWin));
    }
    if (m_hotkeyKeyEdit) {
        m_hotkeyKeyEdit->setText(manualKeyText(hotkey.key));
    }
    updateManualHotkeyPreview();
}

QString RuleDialog::uiText(UiText::Key key) const {
    return UiText::text(m_language, key);
}

void RuleDialog::updateUiForCategory() {
    QString title;
    QString placeholder;
    switch (m_category) {
    case LauncherCategory::Program:
        title = uiText(UiText::Key::RuleDialogProgramTitle);
        placeholder = uiText(UiText::Key::ProgramTargetPlaceholder);
        m_browseButton->setVisible(true);
        m_argumentsRow->setVisible(true);
        m_workingDirectoryRow->setVisible(true);
        m_singleInstanceRow->setVisible(true);
        break;
    case LauncherCategory::Folder:
        title = uiText(UiText::Key::RuleDialogFolderTitle);
        placeholder = uiText(UiText::Key::FolderTargetPlaceholder);
        m_browseButton->setVisible(true);
        m_argumentsRow->setVisible(false);
        m_workingDirectoryRow->setVisible(false);
        m_singleInstanceRow->setVisible(false);
        break;
    case LauncherCategory::Website:
        title = uiText(UiText::Key::RuleDialogWebsiteTitle);
        placeholder = uiText(UiText::Key::WebsiteTargetPlaceholder);
        m_browseButton->setVisible(false);
        m_argumentsRow->setVisible(false);
        m_workingDirectoryRow->setVisible(false);
        m_singleInstanceRow->setVisible(false);
        break;
    }
    setWindowTitle(title);
    m_targetEdit->setPlaceholderText(placeholder);
    m_descriptionEdit->setPlaceholderText(uiText(UiText::Key::DescriptionPlaceholder));
    m_argumentsEdit->setPlaceholderText(uiText(UiText::Key::ArgumentsPlaceholder));
    m_workingDirectoryEdit->setPlaceholderText(uiText(UiText::Key::WorkingDirectoryPlaceholder));
    m_hotkeyKeyEdit->setPlaceholderText(uiText(UiText::Key::HotkeyKeyPlaceholder));
    updateManualHotkeyPreview();
    updateSingleInstanceAvailability();
}

LaunchActionType RuleDialog::actionTypeForCategory() const {
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

LaunchWindowState RuleDialog::selectedWindowState() const {
    if (!m_windowStateCombo) {
        return LaunchWindowState::Normal;
    }
    return LaunchAction::windowStateFromName(
        m_windowStateCombo->itemData(m_windowStateCombo->currentIndex()).toString());
}

void RuleDialog::setSelectedWindowState(LaunchWindowState windowState) {
    if (!m_windowStateCombo) {
        return;
    }
    LaunchAction action;
    action.windowState = windowState;
    const QString stateName = action.windowStateName();
    for (int index = 0; index < m_windowStateCombo->count(); ++index) {
        if (m_windowStateCombo->itemData(index).toString().compare(stateName, Qt::CaseInsensitive) == 0) {
            m_windowStateCombo->setCurrentIndex(index);
            return;
        }
    }
    m_windowStateCombo->setCurrentIndex(0);
}
