#include "MainWindow.h"

#include "RuleDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QBrush>
#include <QColorDialog>
#include <QCryptographicHash>
#include <QCursor>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QStatusBar>
#include <QStyle>
#include <QStyleHints>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextOption>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace {
constexpr int RuleIdRole = Qt::UserRole + 2;
constexpr int HeaderHeight = 50;
constexpr int FixedIconColumns = 4;
constexpr int WindowMinimumHeight = 560;
constexpr int SectionHorizontalMargin = 8;
constexpr int ScrollBarReserveWidth = 12;
constexpr int AutoHideSnapDistance = 36;
constexpr int AutoHideTriggerHeight = 4;
constexpr int AutoHideRevealDistance = 4;
constexpr int AutoHidePollIntervalMs = 80;

class IconGridDelegate final : public QStyledItemDelegate {
public:
    explicit IconGridDelegate(LauncherItemAppearance appearance, QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_appearance(std::move(appearance))
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QString text = m_appearance.multilineText ? opt.text : opt.text.simplified();
        const QIcon icon = opt.icon;
        opt.text.clear();
        opt.icon = QIcon();

        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        const QRect itemRect = option.rect.adjusted(2, 2, -2, -2);
        const QSize iconSize = option.decorationSize;
        const QRect iconRect(itemRect.left() + (itemRect.width() - iconSize.width()) / 2,
                             itemRect.top() + 3,
                             iconSize.width(),
                             iconSize.height());

        const QIcon::Mode iconMode = opt.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
        icon.paint(painter, iconRect, Qt::AlignCenter, iconMode);

        QRect textRect = itemRect;
        textRect.setTop(iconRect.bottom() + 3);
        textRect.adjust(0, 0, 0, -1);

        painter->save();
        const QVariant foreground = index.data(Qt::ForegroundRole);
        if (foreground.canConvert<QBrush>()) {
            painter->setPen(qvariant_cast<QBrush>(foreground).color());
        } else if (opt.state.testFlag(QStyle::State_Selected)) {
            painter->setPen(opt.palette.color(QPalette::HighlightedText));
        } else {
            painter->setPen(opt.palette.color(QPalette::Text));
        }
        QFont textFont = opt.font;
        if (!m_appearance.fontFamily.isEmpty()) {
            textFont.setFamily(m_appearance.fontFamily);
        }
        textFont.setPointSize(m_appearance.fontPointSize);
        painter->setFont(textFont);

        QTextOption textOption(Qt::AlignHCenter | Qt::AlignTop);
        textOption.setWrapMode(m_appearance.multilineText ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
        if (m_appearance.showEllipsis) {
            QFontMetrics metrics(textFont);
            if (m_appearance.multilineText) {
                const QString elided = metrics.elidedText(text.simplified(), Qt::ElideRight, textRect.width() * qMax(1, textRect.height() / qMax(1, metrics.lineSpacing())));
                painter->drawText(textRect, elided, textOption);
            } else {
                const QString elided = metrics.elidedText(text.simplified(), Qt::ElideRight, textRect.width());
                painter->drawText(textRect, elided, textOption);
            }
        } else {
            painter->drawText(textRect, text, textOption);
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return {m_appearance.itemWidth, m_appearance.itemHeight};
    }

private:
    LauncherItemAppearance m_appearance;
};
QVector<LauncherCategory> fixedCategories()
{
    return {LauncherCategory::Program, LauncherCategory::Folder, LauncherCategory::Website};
}

UiText::Key defaultSectionNameKey(const QString &sectionId)
{
    if (sectionId == "program-system") {
        return UiText::Key::SectionSystemTools;
    }
    if (sectionId == "program-user") {
        return UiText::Key::SectionMyPrograms;
    }
    if (sectionId == "folder-system") {
        return UiText::Key::SectionSystemFolders;
    }
    if (sectionId == "folder-user") {
        return UiText::Key::SectionMyFolders;
    }
    if (sectionId == "website-common") {
        return UiText::Key::SectionCommonWebsites;
    }
    if (sectionId == "website-user") {
        return UiText::Key::SectionMyWebsites;
    }
    return UiText::Key::NewSection;
}

bool isDefaultSectionId(const QString &sectionId)
{
    return sectionId == "program-system" || sectionId == "program-user" ||
        sectionId == "folder-system" || sectionId == "folder-user" ||
        sectionId == "website-common" || sectionId == "website-user";
}

QString sectionDisplayName(const QString &language, const LauncherSection &section)
{
    if (isDefaultSectionId(section.id)) {
        const UiText::Key key = defaultSectionNameKey(section.id);
        const QString zhName = UiText::text("zh-CN", key);
        const QString enName = UiText::text("en-US", key);
        if (section.name == zhName || section.name == enName) {
            return UiText::text(language, key);
        }
    }
    return section.name;
}

void localizeDialogButtons(QDialogButtonBox *buttons, const QString &language)
{
    if (!buttons) {
        return;
    }
    if (QPushButton *button = buttons->button(QDialogButtonBox::Ok)) {
        button->setText(UiText::text(language, UiText::Key::Ok));
    }
    if (QPushButton *button = buttons->button(QDialogButtonBox::Cancel)) {
        button->setText(UiText::text(language, UiText::Key::Cancel));
    }
}

void showWarning(QWidget *parent, const QString &language, const QString &title, const QString &message)
{
    QMessageBox box(QMessageBox::Warning, title, message, QMessageBox::Ok, parent);
    if (QAbstractButton *button = box.button(QMessageBox::Ok)) {
        button->setText(UiText::text(language, UiText::Key::Ok));
    }
    box.exec();
}

bool confirm(QWidget *parent, const QString &language, const QString &title, const QString &message)
{
    QMessageBox box(QMessageBox::Question, title, message, QMessageBox::Yes | QMessageBox::No, parent);
    if (QAbstractButton *button = box.button(QMessageBox::Yes)) {
        button->setText(UiText::text(language, UiText::Key::Yes));
    }
    if (QAbstractButton *button = box.button(QMessageBox::No)) {
        button->setText(UiText::text(language, UiText::Key::No));
    }
    return box.exec() == QMessageBox::Yes;
}

QString cssQuoted(const QString &value)
{
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return QString("\"%1\"").arg(escaped);
}

QString textAppearanceStyleSheet(const QString &fontFamily, int fontPointSize, const QString &textColor, int fontWeight)
{
    QStringList rules;
    if (!fontFamily.trimmed().isEmpty()) {
        rules << QString("font-family: %1;").arg(cssQuoted(fontFamily.trimmed()));
    }
    rules << QString("font-size: %1pt;").arg(fontPointSize);
    rules << QString("font-weight: %1;").arg(fontWeight);
    if (!textColor.trimmed().isEmpty()) {
        rules << QString("color: %1;").arg(textColor.trimmed());
    }
    return rules.join(' ');
}

void applyTextAppearanceFont(QWidget *widget, const QString &fontFamily, int fontPointSize, QFont::Weight fontWeight)
{
    if (!widget) {
        return;
    }
    QFont font = widget->font();
    if (!fontFamily.trimmed().isEmpty()) {
        font.setFamily(fontFamily.trimmed());
    }
    font.setPointSize(fontPointSize);
    font.setWeight(fontWeight);
    widget->setFont(font);
}

void updateColorButton(QPushButton *button, const QString &language, const QString &color)
{
    if (!button) {
        return;
    }
    if (color.trimmed().isEmpty()) {
        button->setText(UiText::text(language, UiText::Key::DefaultColor));
        button->setStyleSheet({});
        return;
    }
    button->setText(color.trimmed());
    button->setStyleSheet(QString("QPushButton { color: %1; font-weight: 700; }").arg(color.trimmed()));
}

QString passwordInput(QWidget *parent, const QString &language, const QString &title, const QString &prompt, bool *ok)
{
    QInputDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setLabelText(prompt);
    dialog.setTextEchoMode(QLineEdit::Password);
    dialog.setOkButtonText(UiText::text(language, UiText::Key::Ok));
    dialog.setCancelButtonText(UiText::text(language, UiText::Key::Cancel));
    const bool accepted = dialog.exec() == QDialog::Accepted;
    if (ok) {
        *ok = accepted;
    }
    return accepted ? dialog.textValue() : QString();
}

#ifdef Q_OS_WIN
void forceWindowForeground(QWidget *widget)
{
    if (!widget) {
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd) {
        return;
    }

    ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOW);

    const DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    const DWORD targetThread = GetWindowThreadProcessId(hwnd, nullptr);
    const DWORD currentThread = GetCurrentThreadId();

    if (foregroundThread != 0 && foregroundThread != currentThread) {
        AttachThreadInput(currentThread, foregroundThread, TRUE);
    }
    if (targetThread != 0 && targetThread != currentThread) {
        AttachThreadInput(currentThread, targetThread, TRUE);
    }

    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    if (targetThread != 0 && targetThread != currentThread) {
        AttachThreadInput(currentThread, targetThread, FALSE);
    }
    if (foregroundThread != 0 && foregroundThread != currentThread) {
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    }
}
#endif

QString lightStyleSheet()
{
    return R"(
        QMainWindow { background: #d7e7f7; }
        QWidget#root { background: #f6fbff; border: 1px solid #95b4d1; }
        QFrame#topPanel {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #5be3c5, stop:1 #2b9bd7);
            border-bottom: 1px solid #7fb3ca;
        }
        QLabel#brand {
            color: #ffffff;
            font-size: 18px;
            font-weight: 800;
            font-style: italic;
            letter-spacing: 0;
        }
        QLabel#countText { color: #e9fbff; font-size: 13px; font-weight: 600; }
        QWidget#content { background: #edf4fb; }
        QFrame#searchBand {
            background: #edf4fb;
            border: none;
        }
        QWidget#navBar {
            background: #edf4fb;
            border: none;
        }
        QWidget#navBar QToolButton {
            min-height: 28px;
            max-height: 30px;
            border: 1px solid #d4e2ee;
            border-bottom: none;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            border-bottom-left-radius: 0;
            border-bottom-right-radius: 0;
            background: #eef5fb;
            color: #4b6574;
            padding: 2px 12px;
            font-size: 13px;
            font-weight: 700;
        }
        QWidget#navBar QToolButton:hover {
            background: #f6fbff;
            border-color: #b8d2e4;
        }
        QWidget#navBar QToolButton:checked {
            background: #dfeaf3;
            color: #0f6d8b;
            border-color: #d4e2ee;
            border-bottom: none;
        }
        QToolButton#windowButton {
            min-width: 28px; min-height: 26px;
            max-width: 28px; max-height: 26px;
            border: 1px solid rgba(255,255,255,0.64);
            border-radius: 5px;
            background: rgba(255,255,255,0.22);
            color: #ffffff;
            font-size: 13px;
            font-weight: 800;
        }
        QToolButton#windowButton:hover {
            background: rgba(255,255,255,0.46);
            border-color: rgba(255,255,255,0.92);
        }
        QToolButton#windowButton:pressed {
            background: rgba(11,95,128,0.34);
            border-color: rgba(255,255,255,0.72);
            padding-top: 1px;
        }
        QToolButton#windowButton::menu-indicator { image: none; width: 0px; }
        QLineEdit {
            min-height: 22px;
            padding: 2px 10px;
            border: 1px solid #9dc3db;
            border-radius: 13px;
            background: white;
            color: #172033;
            font-size: 14px;
        }
        QScrollArea#sectionScroll {
            background: #dfeaf3;
            border-top: 1px solid #c7d7e4;
            border-left: none;
            border-right: none;
            border-bottom: none;
        }
        QWidget#sectionsContainer { background: #dfeaf3; }
        QFrame#sectionFrame {
            background: #f9fcff;
            border: 1px solid #d4e2ee;
            border-radius: 8px;
        }
        QFrame#sectionHeader {
            background: #f4f9fe;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            border-bottom: 1px solid #d4e2ee;
        }
        QLabel#sectionTitle {
            color: #3d5c70;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#sectionMeta {
            color: #829aad;
            font-size: 11px;
        }
        QLabel#sectionToggle {
            color: #6f8799;
            font-size: 13px;
            font-weight: 800;
        }
        QLabel#lockedHint {
            color: #8a98a6;
            background: #fbfdff;
            padding: 18px;
            font-size: 13px;
        }
        QListWidget#ruleGrid {
            background: #ffffff;
            border: none;
            outline: none;
        }
        QListWidget#ruleGrid::item {
            color: #1f2b3a;
            padding: 1px;
            border-radius: 4px;
            font-size: 8px;
            font-weight: 500;
        }
        QListWidget#ruleGrid::item:hover { background: #f0f8ff; }
        QListWidget#ruleGrid::item:selected { background: #ddf4ee; color: #111827; }
        QFrame#statusBand {
            background: #f8fbff;
            border-top: 1px solid #c5d7e7;
        }
        QLabel#statusText { color: #627789; font-size: 12px; }
    )";
}

QString darkStyleSheet()
{
    return R"(
        QMainWindow { background: #101822; }
        QWidget#root { background: #111923; border: 1px solid #355163; }
        QFrame#topPanel {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #13866f, stop:1 #176b9e);
            border-bottom: 1px solid #315a6c;
        }
        QLabel#brand {
            color: #f7fdff;
            font-size: 18px;
            font-weight: 800;
            font-style: italic;
            letter-spacing: 0;
        }
        QLabel#countText { color: #d5eef5; font-size: 13px; font-weight: 600; }
        QWidget#content { background: #172331; }
        QFrame#searchBand {
            background: #172331;
            border: none;
        }
        QWidget#navBar {
            background: #172331;
            border: none;
        }
        QWidget#navBar QToolButton {
            min-height: 28px;
            max-height: 30px;
            border: 1px solid #2d4254;
            border-bottom: none;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            border-bottom-left-radius: 0;
            border-bottom-right-radius: 0;
            background: #1d2b3a;
            color: #afc4d1;
            padding: 2px 12px;
            font-size: 13px;
            font-weight: 700;
        }
        QWidget#navBar QToolButton:hover {
            background: #24374a;
            border-color: #3a5368;
            color: #ffffff;
        }
        QWidget#navBar QToolButton:checked {
            background: #243448;
            color: #7ee2ce;
            border-color: #2d4254;
            border-bottom: none;
        }
        QToolButton#windowButton {
            min-width: 28px; min-height: 26px;
            max-width: 28px; max-height: 26px;
            border: 1px solid rgba(255,255,255,0.38);
            border-radius: 5px;
            background: rgba(255,255,255,0.14);
            color: #ffffff;
            font-size: 13px;
            font-weight: 800;
        }
        QToolButton#windowButton:hover {
            background: rgba(255,255,255,0.30);
            border-color: rgba(255,255,255,0.72);
        }
        QToolButton#windowButton:pressed {
            background: rgba(0,0,0,0.34);
            border-color: rgba(255,255,255,0.55);
            padding-top: 1px;
        }
        QToolButton#windowButton::menu-indicator { image: none; width: 0px; }
        QLineEdit {
            min-height: 22px;
            padding: 2px 10px;
            border: 1px solid #355163;
            border-radius: 13px;
            background: #172331;
            color: #eef7fb;
            font-size: 14px;
        }
        QScrollArea#sectionScroll {
            background: #243448;
            border-top: 1px solid #2d4254;
            border-left: none;
            border-right: none;
            border-bottom: none;
        }
        QWidget#sectionsContainer { background: #243448; }
        QFrame#sectionFrame {
            background: #16212f;
            border: 1px solid #2d4254;
            border-radius: 8px;
        }
        QFrame#sectionHeader {
            background: #1b2836;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            border-bottom: 1px solid #2d4254;
        }
        QLabel#sectionTitle {
            color: #d9edf5;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#sectionMeta {
            color: #91a8b8;
            font-size: 11px;
        }
        QLabel#sectionToggle {
            color: #8ca3b5;
            font-size: 13px;
            font-weight: 800;
        }
        QLabel#lockedHint {
            color: #9aaaba;
            background: #172331;
            padding: 18px;
            font-size: 13px;
        }
        QListWidget#ruleGrid {
            background: #172331;
            border: none;
            outline: none;
        }
        QListWidget#ruleGrid::item {
            color: #e7f0f5;
            padding: 1px;
            border-radius: 4px;
            font-size: 8px;
            font-weight: 500;
        }
        QListWidget#ruleGrid::item:hover { background: #203348; }
        QListWidget#ruleGrid::item:selected { background: #24463d; color: #ffffff; }
        QFrame#statusBand {
            background: #111923;
            border-top: 1px solid #2d4254;
        }
        QLabel#statusText { color: #91a8b8; font-size: 12px; }
    )";
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("HotKeyManager");
    setWindowIcon(QIcon(":/app.svg"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setMouseTracking(true);
    resize(fixedLauncherWidth(), 720);
    applyFixedLauncherWidth();
    buildUi();
    setAlwaysOnTop(true);
    qApp->installEventFilter(this);
    m_autoHideTimer.setInterval(AutoHidePollIntervalMs);
    connect(&m_autoHideTimer, &QTimer::timeout, this, &MainWindow::updateTopAutoHide);

    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
        if (m_document.settings.themeMode == "system") {
            applyTheme();
        }
    });

    connect(&m_hookService, &HotkeyHookService::hotkeyTriggered, this, &MainWindow::onHotkeyTriggered);
    connect(&m_hookService, &HotkeyHookService::hookError, this, &MainWindow::setStatus);

    loadDocument();

    QString hookError;
    if (!m_hookService.start(&hookError)) {
        showWarning(this, language(), uiText(UiText::Key::HotkeyHookFailed), hookError);
        setStatus(hookError);
    } else {
        setStatus(uiText(UiText::Key::HookRunning));
    }
}

void MainWindow::buildUi()
{
    auto *root = new QWidget(this);
    root->setObjectName("root");
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *header = new QFrame(root);
    header->setObjectName("topPanel");
    header->setFixedHeight(HeaderHeight);
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(14, 0, 10, 0);
    headerLayout->setSpacing(0);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(4);
    auto *brand = new QLabel("HSTART", header);
    brand->setObjectName("brand");
    brand->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto *brandIcon = new QLabel(header);
    brandIcon->setPixmap(windowIcon().pixmap(32, 32));
    m_ruleCountLabel = new QLabel(header);
    m_ruleCountLabel->setObjectName("countText");
    m_ruleCountLabel->setMinimumWidth(0);
    m_ruleCountLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_ruleCountLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_settingsButton = new QToolButton(header);
    m_settingsButton->setIcon(themedIcon("settings"));
    m_settingsButton->setPopupMode(QToolButton::DelayedPopup);
    m_settingsButton->setObjectName("windowButton");
    m_minButton = new QToolButton(header);
    m_minButton->setText("-");
    m_minButton->setObjectName("windowButton");
    m_closeButton = new QToolButton(header);
    m_closeButton->setText("X");
    m_closeButton->setObjectName("windowButton");
    for (QToolButton *button : {m_settingsButton, m_minButton, m_closeButton}) {
        button->setFixedSize(28, 26);
        button->setIconSize(QSize(18, 18));
    }
    m_settingsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_minButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_closeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_minButton, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(m_closeButton, &QToolButton::clicked, this, &QWidget::hide);
    connect(m_settingsButton, &QToolButton::clicked, this, [this]() {
        if (m_settingsMenu) {
            m_settingsMenu->popup(m_settingsButton->mapToGlobal(QPoint(0, m_settingsButton->height())));
        }
    });

    titleRow->addWidget(brandIcon, 0, Qt::AlignVCenter);
    titleRow->addWidget(brand);
    titleRow->addStretch(1);
    titleRow->addWidget(m_ruleCountLabel, 0);
    titleRow->addWidget(m_settingsButton, 0, Qt::AlignTop);
    titleRow->addWidget(m_minButton, 0, Qt::AlignTop);
    titleRow->addWidget(m_closeButton, 0, Qt::AlignTop);
    headerLayout->addLayout(titleRow);

    auto *content = new QWidget(root);
    content->setObjectName("content");
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto *searchBand = new QFrame(content);
    searchBand->setObjectName("searchBand");
    auto *searchLayout = new QHBoxLayout(searchBand);
    searchLayout->setContentsMargins(12, 6, 12, 4);
    searchLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(searchBand);
    m_searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(m_searchEdit, 1);

    m_navBar = new QWidget(content);
    m_navBar->setObjectName("navBar");
    auto *navBarLayout = new QHBoxLayout(m_navBar);
    navBarLayout->setContentsMargins(0, 0, 0, 0);
    navBarLayout->setSpacing(0);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    m_navButtons.clear();
    for (LauncherCategory category : fixedCategories()) {
        auto *button = new QToolButton(m_navBar);
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIcon(iconForCategory(category));
        button->setText(categoryDisplayName(category));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        button->setProperty("category", static_cast<int>(category));
        m_navGroup->addButton(button);
        navBarLayout->addWidget(button, 1);
        m_navButtons.insert(category, button);
        connect(button, &QToolButton::clicked, this, [this, category]() {
            setCurrentCategory(category);
        });
    }

    contentLayout->addWidget(searchBand);
    contentLayout->addWidget(m_navBar);

    m_scrollArea = new QScrollArea(content);
    m_scrollArea->setObjectName("sectionScroll");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_sectionsContainer = new QWidget(m_scrollArea);
    m_sectionsContainer->setObjectName("sectionsContainer");
    m_sectionsLayout = new QVBoxLayout(m_sectionsContainer);
    m_sectionsLayout->setContentsMargins(2, 12, 2, 12);
    m_sectionsLayout->setSpacing(10);
    m_scrollArea->setWidget(m_sectionsContainer);

    contentLayout->addWidget(m_scrollArea, 1);

    auto *statusBand = new QFrame(root);
    statusBand->setObjectName("statusBand");
    statusBand->setFixedHeight(34);
    auto *statusLayout = new QHBoxLayout(statusBand);
    statusLayout->setContentsMargins(12, 0, 12, 0);
    m_statusLabel = new QLabel(statusBand);
    m_statusLabel->setObjectName("statusText");
    statusLayout->addWidget(m_statusLabel);

    layout->addWidget(header);
    layout->addWidget(content, 1);
    layout->addWidget(statusBand);
    setCentralWidget(root);
    statusBar()->hide();
    enablePointerTracking(root);

    buildSettingsMenu();
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::refreshLauncher);
    applyTheme();
}

void MainWindow::rebuildNavItems()
{
    if (!m_navGroup) {
        return;
    }

    for (LauncherCategory category : fixedCategories()) {
        if (QToolButton *button = m_navButtons.value(category)) {
            const QSignalBlocker blocker(button);
            button->setText(categoryDisplayName(category));
            button->setIcon(iconForCategory(category));
            button->setChecked(category == m_currentCategory);
            button->setToolTip(categoryDisplayName(category));
        }
    }
    applyCategoryAppearance();
}

void MainWindow::buildSettingsMenu()
{
    if (!m_settingsButton) {
        return;
    }

    if (m_settingsMenu) {
        m_settingsMenu->deleteLater();
    }
    m_settingsMenu = new QMenu(this);
    m_hotkeysEnabledAction = m_settingsMenu->addAction(uiText(UiText::Key::HotkeysEnabled));
    m_hotkeysEnabledAction->setCheckable(true);
    connect(m_hotkeysEnabledAction, &QAction::toggled, this, &MainWindow::applyHotkeysEnabled);
    m_hotkeyListAction = m_settingsMenu->addAction(uiText(UiText::Key::HotkeyList));
    connect(m_hotkeyListAction, &QAction::triggered, this, &MainWindow::showHotkeyListDialog);

    auto *languageMenu = m_settingsMenu->addMenu(uiText(UiText::Key::Language));
    auto *languageGroup = new QActionGroup(languageMenu);
    languageGroup->setExclusive(true);
    m_chineseAction = languageMenu->addAction(uiText(UiText::Key::Chinese));
    m_englishAction = languageMenu->addAction(uiText(UiText::Key::English));
    for (QAction *action : {m_chineseAction, m_englishAction}) {
        action->setCheckable(true);
        languageGroup->addAction(action);
    }
    connect(m_chineseAction, &QAction::triggered, this, [this]() {
        setLanguage("zh-CN");
    });
    connect(m_englishAction, &QAction::triggered, this, [this]() {
        setLanguage("en-US");
    });

    auto *themeMenu = m_settingsMenu->addMenu(uiText(UiText::Key::Theme));
    auto *themeGroup = new QActionGroup(themeMenu);
    themeGroup->setExclusive(true);
    m_themeSystemAction = themeMenu->addAction(uiText(UiText::Key::ThemeSystem));
    m_themeLightAction = themeMenu->addAction(uiText(UiText::Key::ThemeLight));
    m_themeDarkAction = themeMenu->addAction(uiText(UiText::Key::ThemeDark));
    for (QAction *action : {m_themeSystemAction, m_themeLightAction, m_themeDarkAction}) {
        action->setCheckable(true);
        themeGroup->addAction(action);
    }
    connect(m_themeSystemAction, &QAction::triggered, this, [this]() { setThemeMode("system"); });
    connect(m_themeLightAction, &QAction::triggered, this, [this]() { setThemeMode("light"); });
    connect(m_themeDarkAction, &QAction::triggered, this, [this]() { setThemeMode("dark"); });

    m_itemAppearanceAction = m_settingsMenu->addAction(uiText(UiText::Key::ItemAppearance));
    connect(m_itemAppearanceAction, &QAction::triggered, this, &MainWindow::showItemAppearanceDialog);
    m_sectionAppearanceAction = m_settingsMenu->addAction(uiText(UiText::Key::SectionAppearance));
    connect(m_sectionAppearanceAction, &QAction::triggered, this, &MainWindow::showSectionAppearanceDialog);
    m_categoryAppearanceAction = m_settingsMenu->addAction(uiText(UiText::Key::CategoryAppearance));
    connect(m_categoryAppearanceAction, &QAction::triggered, this, &MainWindow::showCategoryAppearanceDialog);

    retranslateUi();
}

void MainWindow::retranslateUi()
{
    if (m_searchEdit) {
        m_searchEdit->setPlaceholderText(uiText(UiText::Key::SearchPlaceholder));
    }
    if (m_settingsButton) {
        m_settingsButton->setToolTip(uiText(UiText::Key::Settings));
    }
    if (m_minButton) {
        m_minButton->setToolTip(uiText(UiText::Key::Minimize));
    }
    if (m_closeButton) {
        m_closeButton->setToolTip(uiText(UiText::Key::Close));
    }
    if (m_hotkeysEnabledAction) {
        const QSignalBlocker blocker(m_hotkeysEnabledAction);
        m_hotkeysEnabledAction->setText(uiText(UiText::Key::HotkeysEnabled));
        m_hotkeysEnabledAction->setChecked(m_document.settings.hotkeysEnabled);
    }
    if (m_hotkeyListAction) {
        m_hotkeyListAction->setText(uiText(UiText::Key::HotkeyList));
    }
    if (m_settingsMenu && m_settingsMenu->actions().size() > 2 && m_settingsMenu->actions().at(2)->menu()) {
        m_settingsMenu->actions().at(2)->menu()->setTitle(uiText(UiText::Key::Language));
    }
    if (m_settingsMenu && m_settingsMenu->actions().size() > 3 && m_settingsMenu->actions().at(3)->menu()) {
        m_settingsMenu->actions().at(3)->menu()->setTitle(uiText(UiText::Key::Theme));
    }
    if (m_itemAppearanceAction) {
        m_itemAppearanceAction->setText(uiText(UiText::Key::ItemAppearance));
    }
    if (m_sectionAppearanceAction) {
        m_sectionAppearanceAction->setText(uiText(UiText::Key::SectionAppearance));
    }
    if (m_categoryAppearanceAction) {
        m_categoryAppearanceAction->setText(uiText(UiText::Key::CategoryAppearance));
    }
    if (m_chineseAction) {
        const QSignalBlocker blocker(m_chineseAction);
        m_chineseAction->setText(uiText(UiText::Key::Chinese));
        m_chineseAction->setChecked(language() == "zh-CN");
    }
    if (m_englishAction) {
        const QSignalBlocker blocker(m_englishAction);
        m_englishAction->setText(uiText(UiText::Key::English));
        m_englishAction->setChecked(language() == "en-US");
    }
    if (m_themeSystemAction) {
        const QSignalBlocker blocker(m_themeSystemAction);
        m_themeSystemAction->setText(uiText(UiText::Key::ThemeSystem));
        m_themeSystemAction->setChecked(m_document.settings.themeMode == "system");
    }
    if (m_themeLightAction) {
        const QSignalBlocker blocker(m_themeLightAction);
        m_themeLightAction->setText(uiText(UiText::Key::ThemeLight));
        m_themeLightAction->setChecked(m_document.settings.themeMode == "light");
    }
    if (m_themeDarkAction) {
        const QSignalBlocker blocker(m_themeDarkAction);
        m_themeDarkAction->setText(uiText(UiText::Key::ThemeDark));
        m_themeDarkAction->setChecked(m_document.settings.themeMode == "dark");
    }
    rebuildNavItems();
    refreshLauncher();
    applyTheme();
}

void MainWindow::showSettings()
{
    showNormal();
    setAlwaysOnTop(true);
    revealFromTopAutoHide();
    raise();
    activateWindow();
#ifdef Q_OS_WIN
    forceWindowForeground(this);
#endif
}

QString MainWindow::language() const
{
    return UiText::normalizeLanguage(m_document.settings.language);
}

bool MainWindow::hotkeysEnabled() const
{
    return m_document.settings.hotkeysEnabled;
}

void MainWindow::setHotkeysPaused(bool paused)
{
    applyHotkeysEnabled(!paused);
}

void MainWindow::applyHotkeysEnabled(bool enabled)
{
    if (m_document.settings.hotkeysEnabled != enabled) {
        m_document.settings.hotkeysEnabled = enabled;
        saveDocumentSilently();
    }

    m_hookService.setPaused(!enabled);
    if (m_hotkeysEnabledAction && m_hotkeysEnabledAction->isChecked() != enabled) {
        const QSignalBlocker blocker(m_hotkeysEnabledAction);
        m_hotkeysEnabledAction->setChecked(enabled);
    }
    emit hotkeysEnabledChanged(enabled);
    setStatus(enabled ? uiText(UiText::Key::HotkeysResumed) : uiText(UiText::Key::HotkeysPaused));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    hide();
    event->ignore();
}

void MainWindow::enablePointerTracking(QWidget *widget)
{
    if (!widget) {
        return;
    }
    widget->setMouseTracking(true);
    for (QObject *child : widget->children()) {
        if (auto *childWidget = qobject_cast<QWidget *>(child)) {
            enablePointerTracking(childWidget);
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove || event->type() == QEvent::Drop) {
        auto *widget = qobject_cast<QWidget *>(watched);
        const QString sectionId = widget ? widget->property("sectionId").toString() : QString();
        if (!sectionId.isEmpty() && sectionIndexById(sectionId) >= 0) {
            if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
                auto *dragEvent = static_cast<QDragMoveEvent *>(event);
                if (dragEvent->mimeData() && dragEvent->mimeData()->hasUrls()) {
                    dragEvent->acceptProposedAction();
                    return true;
                }
            } else {
                auto *dropEvent = static_cast<QDropEvent *>(event);
                if (dropEvent->mimeData() && dropEvent->mimeData()->hasUrls()) {
                    addDroppedPathsToSection(sectionId, dropEvent->mimeData()->urls());
                    dropEvent->acceptProposedAction();
                    return true;
                }
            }
        }
    }

    if (!isVisible()) {
        return QMainWindow::eventFilter(watched, event);
    }
    if (m_topAutoHidden) {
        return QMainWindow::eventFilter(watched, event);
    }
    if (event->type() != QEvent::MouseMove &&
        event->type() != QEvent::MouseButtonPress &&
        event->type() != QEvent::MouseButtonDblClick &&
        event->type() != QEvent::MouseButtonRelease) {
        return QMainWindow::eventFilter(watched, event);
    }

    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget || widget->window() != this) {
        return QMainWindow::eventFilter(watched, event);
    }

    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    const QPoint localPos = mapFromGlobal(mouseEvent->globalPosition().toPoint());

    if (event->type() == QEvent::MouseMove) {
        if (m_resizing) {
            performResize(mouseEvent->globalPosition().toPoint());
            return true;
        }
        if (mouseEvent->buttons().testFlag(Qt::LeftButton) && !m_dragPosition.isNull()) {
            if (m_topAutoHidden) {
                revealFromTopAutoHide();
                m_dragPosition = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
            }
            move(mouseEvent->globalPosition().toPoint() - m_dragPosition);
            return true;
        }
        updateResizeCursor(localPos);
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
        m_resizeRegion = resizeRegionAt(localPos);
        if (m_resizeRegion != ResizeRegion::None) {
            m_resizing = true;
            m_resizeStartGlobal = mouseEvent->globalPosition().toPoint();
            m_resizeStartGeometry = geometry();
            return true;
        }
        const bool draggableWidget = !qobject_cast<QAbstractButton *>(widget) && !qobject_cast<QLineEdit *>(widget);
        if (localPos.y() <= HeaderHeight && draggableWidget) {
            m_dragPosition = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonDblClick && mouseEvent->button() == Qt::LeftButton) {
        QWidget *sectionHeader = widget;
        while (sectionHeader && sectionHeader->objectName() != "sectionHeader") {
            sectionHeader = sectionHeader->parentWidget();
        }
        if (sectionHeader) {
            expandSectionOnly(sectionHeader->property("sectionId").toString());
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease && m_resizing) {
        m_resizing = false;
        m_resizeRegion = ResizeRegion::None;
        updateResizeCursor(localPos);
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease && mouseEvent->button() == Qt::LeftButton) {
        const bool wasDraggingWindow = !m_dragPosition.isNull();
        if (wasDraggingWindow) {
            m_dragPosition = {};
            finishInteractiveMove();
            return true;
        }
        QWidget *sectionHeader = widget;
        while (sectionHeader && sectionHeader->objectName() != "sectionHeader") {
            sectionHeader = sectionHeader->parentWidget();
        }
        if (sectionHeader) {
            expandSectionOnly(sectionHeader->property("sectionId").toString());
            return true;
        }
        m_dragPosition = {};
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (m_topAutoHidden) {
        revealFromTopAutoHide();
    }
    if (event->button() == Qt::LeftButton) {
        m_resizeRegion = resizeRegionAt(event->position().toPoint());
        if (m_resizeRegion != ResizeRegion::None) {
            m_resizing = true;
            m_resizeStartGlobal = event->globalPosition().toPoint();
            m_resizeStartGeometry = geometry();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && event->position().y() <= HeaderHeight) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_topAutoHidden) {
        QMainWindow::mouseMoveEvent(event);
        return;
    }
    if (m_resizing) {
        performResize(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    if (event->buttons().testFlag(Qt::LeftButton) && !m_dragPosition.isNull()) {
        if (m_topAutoHidden) {
            revealFromTopAutoHide();
            m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        }
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
        return;
    }
    updateResizeCursor(event->position().toPoint());
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    const bool wasDraggingWindow = !m_dragPosition.isNull();
    m_resizing = false;
    m_resizeRegion = ResizeRegion::None;
    m_dragPosition = {};
    updateResizeCursor(event->position().toPoint());
    if (wasDraggingWindow) {
        finishInteractiveMove();
        event->accept();
        return;
    }
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    rebuildNavItems();
    updateLauncherGrids();
}

void MainWindow::hideEvent(QHideEvent *event)
{
    if (m_topAutoHidden) {
        m_topAutoHidden = false;
        setMinimumHeight(WindowMinimumHeight);
        setMaximumHeight(QWIDGETSIZE_MAX);
        if (m_autoHideShownGeometry.isValid()) {
            setGeometry(m_autoHideShownGeometry);
        }
    }
    m_autoHideTimer.stop();
    QMainWindow::hideEvent(event);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    setAlwaysOnTop(true);
}

void MainWindow::leaveEvent(QEvent *event)
{
    if (!m_resizing) {
        unsetCursor();
    }
    QMainWindow::leaveEvent(event);
}

void MainWindow::onHotkeyTriggered(const HotkeyRule &rule)
{
    if (!ensureSectionUnlocked(rule.sectionId)) {
        setStatus(uiText(UiText::Key::SectionLockedCancelled));
        return;
    }

    QString error;
    if (!m_runner.run(rule.action, &error)) {
        setStatus(uiText(UiText::Key::LaunchFailed).arg(error));
        return;
    }
    setStatus(uiText(UiText::Key::Launched).arg(ruleTitle(rule)));
}

void MainWindow::loadDocument()
{
    QString error;
    m_document = m_store.loadDocument(&error);
    m_document.settings.language = UiText::normalizeLanguage(m_document.settings.language);
    applyFixedLauncherWidth();
    if (!error.isEmpty()) {
        setStatus(error);
    }
    refreshHooks();
    m_hookService.setPaused(!m_document.settings.hotkeysEnabled);
    retranslateUi();
    emit hotkeysEnabledChanged(m_document.settings.hotkeysEnabled);
    emit languageChanged(language());
}

void MainWindow::saveDocument()
{
    QString error;
    if (!m_store.saveDocument(m_document, &error)) {
        showWarning(this, language(), uiText(UiText::Key::SaveFailed), error);
        setStatus(error);
        return;
    }
    refreshHooks();
    refreshLauncher();
    setStatus(uiText(UiText::Key::SavedItems).arg(m_document.rules.size()));
}

void MainWindow::saveDocumentSilently()
{
    QString error;
    if (!m_store.saveDocument(m_document, &error)) {
        showWarning(this, language(), uiText(UiText::Key::SaveFailed), error);
        setStatus(error);
        return;
    }
    refreshHooks();
}

void MainWindow::refreshHooks()
{
    m_hookService.setRules(m_document.rules);
    m_hookService.setPaused(!m_document.settings.hotkeysEnabled);
}

void MainWindow::refreshLauncher()
{
    rebuildSections();

    const int enabledCount = std::count_if(m_document.rules.cbegin(), m_document.rules.cend(), [](const HotkeyRule &rule) {
        return rule.enabled;
    });
    if (m_ruleCountLabel) {
        m_ruleCountLabel->setText(uiText(UiText::Key::EnabledCount).arg(enabledCount).arg(m_document.rules.size()));
    }
}

void MainWindow::rebuildSections()
{
    if (!m_sectionsLayout) {
        return;
    }

    while (QLayoutItem *item = m_sectionsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_sectionLists.clear();

    QVector<LauncherSection> sections;
    for (const LauncherSection &section : m_document.sections) {
        if (section.category == m_currentCategory) {
            sections.push_back(section);
        }
    }
    std::sort(sections.begin(), sections.end(), [](const LauncherSection &left, const LauncherSection &right) {
        if (left.sortOrder == right.sortOrder) {
            return QString::localeAwareCompare(left.name, right.name) < 0;
        }
        return left.sortOrder < right.sortOrder;
    });

    QString openSectionId;
    for (const LauncherSection &section : sections) {
        if (!section.collapsed) {
            openSectionId = section.id;
            break;
        }
    }
    if (openSectionId.isEmpty() && !sections.isEmpty()) {
        openSectionId = sections.constFirst().id;
    }

    bool normalizedCollapsedState = false;
    for (LauncherSection &section : m_document.sections) {
        if (section.category != m_currentCategory) {
            continue;
        }
        const bool shouldCollapse = section.id != openSectionId;
        if (section.collapsed != shouldCollapse) {
            section.collapsed = shouldCollapse;
            normalizedCollapsedState = true;
        }
    }
    if (normalizedCollapsedState) {
        saveDocumentSilently();
        for (LauncherSection &section : sections) {
            section.collapsed = section.id != openSectionId;
        }
    }

    for (const LauncherSection &section : sections) {
        auto *sectionFrame = new QFrame(m_sectionsContainer);
        sectionFrame->setObjectName("sectionFrame");
        auto *sectionLayout = new QVBoxLayout(sectionFrame);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(0);

        auto *header = new QFrame(sectionFrame);
        header->setObjectName("sectionHeader");
        header->setContextMenuPolicy(Qt::CustomContextMenu);
        header->setProperty("sectionId", section.id);
        header->setProperty("collapsed", section.collapsed);
        const LauncherSectionAppearance sectionAppearance = m_document.settings.sectionAppearance;
        header->setFixedHeight(sectionAppearance.headerHeight);
        auto *headerLayout = new QHBoxLayout(header);
        const int headerVerticalMargin = qMax(2, (sectionAppearance.headerHeight - qMax(sectionAppearance.iconHeight, sectionAppearance.fontPointSize + 8)) / 2);
        headerLayout->setContentsMargins(8, headerVerticalMargin, 8, headerVerticalMargin);

        auto *iconLabel = new QLabel(header);
        iconLabel->setPixmap(iconForSection(section).pixmap(sectionAppearance.iconWidth, sectionAppearance.iconHeight));
        iconLabel->setFixedSize(sectionAppearance.iconWidth, sectionAppearance.iconHeight);
        iconLabel->setScaledContents(false);
        iconLabel->setAlignment(Qt::AlignCenter);
        auto *title = new QLabel(sectionDisplayName(language(), section), header);
        title->setObjectName("sectionTitle");
        applyTextAppearanceFont(title, sectionAppearance.fontFamily, sectionAppearance.fontPointSize, QFont::DemiBold);
        title->setStyleSheet(textAppearanceStyleSheet(
            sectionAppearance.fontFamily,
            sectionAppearance.fontPointSize,
            sectionAppearance.textColor,
            700));
        const int itemCount = std::count_if(m_document.rules.cbegin(), m_document.rules.cend(), [&section](const HotkeyRule &rule) {
            return rule.sectionId == section.id;
        });
        auto *meta = new QLabel(section.encrypted ? uiText(UiText::Key::EncryptedItemCount).arg(itemCount) : uiText(UiText::Key::ItemCount).arg(itemCount), header);
        meta->setObjectName("sectionMeta");
        auto *toggle = new QLabel(section.collapsed ? QStringLiteral("+") : QStringLiteral("-"), header);
        toggle->setObjectName("sectionToggle");
        toggle->setFixedWidth(16);
        toggle->setAlignment(Qt::AlignCenter);

        headerLayout->addWidget(iconLabel);
        headerLayout->addWidget(title, 1);
        headerLayout->addWidget(meta);
        headerLayout->addWidget(toggle);

        connect(header, &QWidget::customContextMenuRequested, this, [this, header](const QPoint &pos) {
            showSectionMenu(header->property("sectionId").toString(), header->mapToGlobal(pos));
        });

        sectionLayout->addWidget(header);

        auto *body = new QWidget(sectionFrame);
        auto *bodyLayout = new QVBoxLayout(body);
        bodyLayout->setContentsMargins(0, 0, 0, 0);
        bodyLayout->setSpacing(0);
        body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        if (!section.collapsed) {
            if (!isSectionUnlocked(section)) {
                auto *lockedHint = new QLabel(uiText(UiText::Key::LockedHint), body);
                lockedHint->setObjectName("lockedHint");
                lockedHint->setAlignment(Qt::AlignCenter);
                lockedHint->setMinimumHeight(72);
                bodyLayout->addWidget(lockedHint);
            } else {
                auto *list = new QListWidget(body);
                list->setObjectName("ruleGrid");
                list->setMovement(QListView::Static);
                list->setResizeMode(QListView::Adjust);
                list->setSelectionMode(QAbstractItemView::SingleSelection);
                list->setSpacing(0);
                list->setWordWrap(m_document.settings.itemAppearance.multilineText);
                list->setUniformItemSizes(true);
                list->setAcceptDrops(true);
                list->viewport()->setAcceptDrops(true);
                list->viewport()->installEventFilter(this);
                list->setContextMenuPolicy(Qt::CustomContextMenu);
                list->setProperty("sectionId", section.id);
                list->viewport()->setProperty("sectionId", section.id);
                list->setTextElideMode(m_document.settings.itemAppearance.showEllipsis ? Qt::ElideRight : Qt::ElideNone);
                list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                list->setItemDelegate(new IconGridDelegate(m_document.settings.itemAppearance, list));
                QFont itemFont = list->font();
                if (!m_document.settings.itemAppearance.fontFamily.isEmpty()) {
                    itemFont.setFamily(m_document.settings.itemAppearance.fontFamily);
                }
                itemFont.setPointSize(m_document.settings.itemAppearance.fontPointSize);
                list->setFont(itemFont);

                for (const HotkeyRule &rule : m_document.rules) {
                    if (rule.sectionId != section.id || !rulePassesFilters(rule)) {
                        continue;
                    }
                    QString text = ruleTitle(rule);
                    auto *item = new QListWidgetItem(iconForRule(rule), text);
                    item->setSizeHint(QSize(m_document.settings.itemAppearance.itemWidth, m_document.settings.itemAppearance.itemHeight));
                    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
                    const QString hotkeyTip = rule.hotkey.isValid() ? rule.hotkey.displayText() : uiText(UiText::Key::UnboundHotkey);
                    item->setToolTip(QString("%1\n%2\n%3").arg(ruleTitle(rule), rule.action.target, hotkeyTip));
                    item->setData(RuleIdRole, rule.id);
                    if (!rule.enabled) {
                        item->setForeground(QColor("#8a94a3"));
                    }
                    list->addItem(item);
                }

                connect(list, &QListWidget::customContextMenuRequested, this, [this, list](const QPoint &pos) {
                    showListMenu(list->property("sectionId").toString(), list, pos);
                });
                connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
                    runRule(item->data(RuleIdRole).toString());
                });

                bodyLayout->addWidget(list, 1);
                m_sectionLists.insert(section.id, list);
            }
        }

        body->setVisible(!section.collapsed);
        sectionLayout->addWidget(body, section.collapsed ? 0 : 1);

        sectionFrame->setSizePolicy(QSizePolicy::Expanding, section.collapsed ? QSizePolicy::Fixed : QSizePolicy::Expanding);
        m_sectionsLayout->addWidget(sectionFrame, section.collapsed ? 0 : 1);
    }

    enablePointerTracking(m_sectionsContainer);
    updateLauncherGrids();
    QTimer::singleShot(0, this, &MainWindow::updateLauncherGrids);
}

void MainWindow::updateLauncherGrids()
{
    const LauncherItemAppearance appearance = m_document.settings.itemAppearance;
    const QSize iconSize(appearance.iconWidth, appearance.iconHeight);
    const QSize gridSize(appearance.itemWidth + appearance.horizontalSpacing,
                         appearance.itemHeight + appearance.verticalSpacing);

    for (QListWidget *list : std::as_const(m_sectionLists)) {
        if (!list) {
            continue;
        }
        list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        list->setViewMode(QListView::IconMode);
        list->setFlow(QListView::LeftToRight);
        list->setWrapping(true);
        list->setSpacing(0);
        list->setWordWrap(appearance.multilineText);
        list->setTextElideMode(appearance.showEllipsis ? Qt::ElideRight : Qt::ElideNone);
        list->setIconSize(iconSize);
        list->setGridSize(gridSize);
        for (int row = 0; row < list->count(); ++row) {
            if (QListWidgetItem *item = list->item(row)) {
                item->setSizeHint(QSize(appearance.itemWidth, appearance.itemHeight));
            }
        }
        QFont itemFont = list->font();
        if (!appearance.fontFamily.isEmpty()) {
            itemFont.setFamily(appearance.fontFamily);
        }
        itemFont.setPointSize(appearance.fontPointSize);
        list->setFont(itemFont);
        auto *oldDelegate = dynamic_cast<IconGridDelegate *>(list->itemDelegate());
        list->setItemDelegate(new IconGridDelegate(appearance, list));
        if (oldDelegate) {
            oldDelegate->deleteLater();
        }
        list->setMinimumHeight(gridSize.height() + 4);
        list->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}

void MainWindow::setCurrentCategory(LauncherCategory category)
{
    if (m_currentCategory == category) {
        return;
    }
    m_currentCategory = category;
    rebuildNavItems();
    refreshLauncher();
}

void MainWindow::setLanguage(const QString &language)
{
    const QString normalized = UiText::normalizeLanguage(language);
    if (m_document.settings.language == normalized) {
        return;
    }
    m_document.settings.language = normalized;
    saveDocumentSilently();
    retranslateUi();
    emit languageChanged(normalized);
}

void MainWindow::setThemeMode(const QString &themeMode)
{
    const QString normalized = themeMode.compare("light", Qt::CaseInsensitive) == 0 ? "light" :
        themeMode.compare("dark", Qt::CaseInsensitive) == 0 ? "dark" : "system";
    if (m_document.settings.themeMode == normalized) {
        return;
    }
    m_document.settings.themeMode = normalized;
    saveDocumentSilently();
    applyTheme();
}

void MainWindow::showHotkeyListDialog()
{
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(uiText(UiText::Key::HotkeyList));
    dialog->setModal(false);

    auto *layout = new QVBoxLayout(dialog);
    auto *table = new QTableWidget(dialog);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({
        uiText(UiText::Key::HotkeyListCategory),
        uiText(UiText::Key::HotkeyListItem),
        uiText(UiText::Key::HotkeyListHotkey),
        uiText(UiText::Key::HotkeyListTarget)
    });
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);

    for (const HotkeyRule &rule : m_document.rules) {
        if (!rule.hotkey.isValid()) {
            continue;
        }
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(categoryDisplayName(rule.category)));
        table->setItem(row, 1, new QTableWidgetItem(ruleTitle(rule)));
        table->setItem(row, 2, new QTableWidgetItem(rule.hotkey.displayText()));
        table->setItem(row, 3, new QTableWidgetItem(rule.action.target));
    }
    table->resizeColumnsToContents();

    layout->addWidget(table, 1);
    if (table->rowCount() == 0) {
        auto *emptyLabel = new QLabel(uiText(UiText::Key::HotkeyListEmpty), dialog);
        emptyLabel->setObjectName("emptyHint");
        emptyLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(emptyLabel);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);

    dialog->resize(620, 420);
    dialog->show();
}

void MainWindow::showItemAppearanceDialog()
{
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(uiText(UiText::Key::ItemAppearance));
    dialog->setModal(false);

    auto *layout = new QVBoxLayout(dialog);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(form);

    auto makeSpin = [dialog](int minimum, int maximum, int value, bool pixelSuffix = true) {
        auto *spin = new QSpinBox(dialog);
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        if (pixelSuffix) {
            spin->setSuffix(" px");
        }
        return spin;
    };

    const LauncherItemAppearance appearance = m_document.settings.itemAppearance;
    auto *iconWidth = makeSpin(16, 128, appearance.iconWidth);
    auto *iconHeight = makeSpin(16, 128, appearance.iconHeight);
    auto *itemWidth = makeSpin(40, 180, appearance.itemWidth);
    auto *itemHeight = makeSpin(44, 220, appearance.itemHeight);
    auto *fontFamily = new QFontComboBox(dialog);
    if (!appearance.fontFamily.isEmpty()) {
        fontFamily->setCurrentFont(QFont(appearance.fontFamily));
    }
    auto *fontPointSize = makeSpin(6, 18, appearance.fontPointSize, false);
    auto *horizontalSpacing = makeSpin(0, 40, appearance.horizontalSpacing);
    auto *verticalSpacing = makeSpin(0, 40, appearance.verticalSpacing);
    auto *multilineText = new QCheckBox(dialog);
    multilineText->setChecked(appearance.multilineText);
    auto *showEllipsis = new QCheckBox(dialog);
    showEllipsis->setChecked(appearance.showEllipsis);

    form->addRow(uiText(UiText::Key::IconWidth), iconWidth);
    form->addRow(uiText(UiText::Key::IconHeight), iconHeight);
    form->addRow(uiText(UiText::Key::ItemWidth), itemWidth);
    form->addRow(uiText(UiText::Key::ItemHeight), itemHeight);
    form->addRow(uiText(UiText::Key::FontFamily), fontFamily);
    form->addRow(uiText(UiText::Key::FontPointSize), fontPointSize);
    form->addRow(uiText(UiText::Key::HorizontalSpacing), horizontalSpacing);
    form->addRow(uiText(UiText::Key::VerticalSpacing), verticalSpacing);
    form->addRow(uiText(UiText::Key::MultilineText), multilineText);
    form->addRow(uiText(UiText::Key::ShowEllipsis), showEllipsis);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);

    auto apply = [this, iconWidth, iconHeight, itemWidth, itemHeight, fontFamily, fontPointSize,
                  horizontalSpacing, verticalSpacing, multilineText, showEllipsis]() {
        LauncherItemAppearance next;
        next.iconWidth = iconWidth->value();
        next.iconHeight = iconHeight->value();
        next.itemWidth = itemWidth->value();
        next.itemHeight = itemHeight->value();
        next.fontFamily = fontFamily->currentFont().family();
        next.fontPointSize = fontPointSize->value();
        next.horizontalSpacing = horizontalSpacing->value();
        next.verticalSpacing = verticalSpacing->value();
        next.multilineText = multilineText->isChecked();
        next.showEllipsis = showEllipsis->isChecked();
        m_document.settings.itemAppearance = LauncherItemAppearance::fromJson(next.toJson());
        applyItemAppearanceChange();
    };

    for (QSpinBox *spin : {iconWidth, iconHeight, itemWidth, itemHeight, fontPointSize, horizontalSpacing, verticalSpacing}) {
        connect(spin, &QSpinBox::valueChanged, this, apply);
    }
    connect(fontFamily, &QFontComboBox::currentFontChanged, this, apply);
    connect(multilineText, &QCheckBox::toggled, this, apply);
    connect(showEllipsis, &QCheckBox::toggled, this, apply);

    dialog->resize(320, dialog->sizeHint().height());
    dialog->show();
}

void MainWindow::showSectionAppearanceDialog()
{
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(uiText(UiText::Key::SectionAppearance));
    dialog->setModal(false);

    auto *layout = new QVBoxLayout(dialog);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(form);

    auto makeSpin = [dialog](int minimum, int maximum, int value, bool pixelSuffix = true) {
        auto *spin = new QSpinBox(dialog);
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        if (pixelSuffix) {
            spin->setSuffix(" px");
        }
        return spin;
    };

    const LauncherSectionAppearance appearance = m_document.settings.sectionAppearance;
    auto *iconWidth = makeSpin(12, 96, appearance.iconWidth);
    auto *iconHeight = makeSpin(12, 96, appearance.iconHeight);
    auto *headerHeight = makeSpin(24, 96, appearance.headerHeight);
    auto *fontFamily = new QFontComboBox(dialog);
    if (!appearance.fontFamily.isEmpty()) {
        fontFamily->setCurrentFont(QFont(appearance.fontFamily));
    }
    auto *fontPointSize = makeSpin(6, 18, appearance.fontPointSize, false);
    auto *colorButton = new QPushButton(dialog);
    auto *defaultColorButton = new QPushButton(uiText(UiText::Key::DefaultColor), dialog);
    updateColorButton(colorButton, language(), appearance.textColor);

    auto *colorLayout = new QHBoxLayout;
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->addWidget(colorButton, 1);
    colorLayout->addWidget(defaultColorButton);
    auto *colorWidget = new QWidget(dialog);
    colorWidget->setLayout(colorLayout);

    form->addRow(uiText(UiText::Key::IconWidth), iconWidth);
    form->addRow(uiText(UiText::Key::IconHeight), iconHeight);
    form->addRow(uiText(UiText::Key::SectionHeight), headerHeight);
    form->addRow(uiText(UiText::Key::FontFamily), fontFamily);
    form->addRow(uiText(UiText::Key::FontPointSize), fontPointSize);
    form->addRow(uiText(UiText::Key::TextColor), colorWidget);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);

    auto apply = [this, iconWidth, iconHeight, headerHeight, fontFamily, fontPointSize, colorButton]() {
        LauncherSectionAppearance next;
        next.iconWidth = iconWidth->value();
        next.iconHeight = iconHeight->value();
        next.headerHeight = headerHeight->value();
        next.fontFamily = fontFamily->currentFont().family();
        next.fontPointSize = fontPointSize->value();
        next.textColor = colorButton->property("selectedColor").toString();
        m_document.settings.sectionAppearance = LauncherSectionAppearance::fromJson(next.toJson());
        applySectionAppearanceChange();
    };
    colorButton->setProperty("selectedColor", appearance.textColor);

    for (QSpinBox *spin : {iconWidth, iconHeight, headerHeight, fontPointSize}) {
        connect(spin, &QSpinBox::valueChanged, this, apply);
    }
    connect(fontFamily, &QFontComboBox::currentFontChanged, this, apply);
    connect(colorButton, &QPushButton::clicked, this, [this, colorButton, apply]() {
        const QString currentColor = colorButton->property("selectedColor").toString();
        const QColor initial = currentColor.isEmpty() ? palette().color(QPalette::Text) : QColor(currentColor);
        const QColor selected = QColorDialog::getColor(initial, this, uiText(UiText::Key::ChooseColor));
        if (!selected.isValid()) {
            return;
        }
        const QString color = selected.name(QColor::HexRgb);
        colorButton->setProperty("selectedColor", color);
        updateColorButton(colorButton, language(), color);
        apply();
    });
    connect(defaultColorButton, &QPushButton::clicked, this, [this, colorButton, apply]() {
        colorButton->setProperty("selectedColor", QString());
        updateColorButton(colorButton, language(), QString());
        apply();
    });

    dialog->resize(320, dialog->sizeHint().height());
    dialog->show();
}

void MainWindow::showCategoryAppearanceDialog()
{
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(uiText(UiText::Key::CategoryAppearance));
    dialog->setModal(false);

    auto *layout = new QVBoxLayout(dialog);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(form);

    auto makeSpin = [dialog](int minimum, int maximum, int value, bool pixelSuffix = true) {
        auto *spin = new QSpinBox(dialog);
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        if (pixelSuffix) {
            spin->setSuffix(" px");
        }
        return spin;
    };

    const LauncherCategoryAppearance appearance = m_document.settings.categoryAppearance;
    auto *iconWidth = makeSpin(12, 96, appearance.iconWidth);
    auto *iconHeight = makeSpin(12, 96, appearance.iconHeight);
    auto *buttonHeight = makeSpin(24, 96, appearance.buttonHeight);
    auto *fontFamily = new QFontComboBox(dialog);
    if (!appearance.fontFamily.isEmpty()) {
        fontFamily->setCurrentFont(QFont(appearance.fontFamily));
    }
    auto *fontPointSize = makeSpin(6, 18, appearance.fontPointSize, false);
    auto *colorButton = new QPushButton(dialog);
    auto *defaultColorButton = new QPushButton(uiText(UiText::Key::DefaultColor), dialog);
    updateColorButton(colorButton, language(), appearance.textColor);

    auto *colorLayout = new QHBoxLayout;
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->addWidget(colorButton, 1);
    colorLayout->addWidget(defaultColorButton);
    auto *colorWidget = new QWidget(dialog);
    colorWidget->setLayout(colorLayout);

    form->addRow(uiText(UiText::Key::IconWidth), iconWidth);
    form->addRow(uiText(UiText::Key::IconHeight), iconHeight);
    form->addRow(uiText(UiText::Key::CategoryHeight), buttonHeight);
    form->addRow(uiText(UiText::Key::FontFamily), fontFamily);
    form->addRow(uiText(UiText::Key::FontPointSize), fontPointSize);
    form->addRow(uiText(UiText::Key::TextColor), colorWidget);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);

    auto apply = [this, iconWidth, iconHeight, buttonHeight, fontFamily, fontPointSize, colorButton]() {
        LauncherCategoryAppearance next;
        next.iconWidth = iconWidth->value();
        next.iconHeight = iconHeight->value();
        next.buttonHeight = buttonHeight->value();
        next.fontFamily = fontFamily->currentFont().family();
        next.fontPointSize = fontPointSize->value();
        next.textColor = colorButton->property("selectedColor").toString();
        m_document.settings.categoryAppearance = LauncherCategoryAppearance::fromJson(next.toJson());
        applyCategoryAppearanceChange();
    };
    colorButton->setProperty("selectedColor", appearance.textColor);

    for (QSpinBox *spin : {iconWidth, iconHeight, buttonHeight, fontPointSize}) {
        connect(spin, &QSpinBox::valueChanged, this, apply);
    }
    connect(fontFamily, &QFontComboBox::currentFontChanged, this, apply);
    connect(colorButton, &QPushButton::clicked, this, [this, colorButton, apply]() {
        const QString currentColor = colorButton->property("selectedColor").toString();
        const QColor initial = currentColor.isEmpty() ? palette().color(QPalette::Text) : QColor(currentColor);
        const QColor selected = QColorDialog::getColor(initial, this, uiText(UiText::Key::ChooseColor));
        if (!selected.isValid()) {
            return;
        }
        const QString color = selected.name(QColor::HexRgb);
        colorButton->setProperty("selectedColor", color);
        updateColorButton(colorButton, language(), color);
        apply();
    });
    connect(defaultColorButton, &QPushButton::clicked, this, [this, colorButton, apply]() {
        colorButton->setProperty("selectedColor", QString());
        updateColorButton(colorButton, language(), QString());
        apply();
    });

    dialog->resize(320, dialog->sizeHint().height());
    dialog->show();
}

void MainWindow::applyItemAppearanceChange()
{
    applyFixedLauncherWidth();
    saveDocumentSilently();
    refreshLauncher();
}

void MainWindow::applySectionAppearanceChange()
{
    saveDocumentSilently();
    refreshLauncher();
}

void MainWindow::applyCategoryAppearanceChange()
{
    saveDocumentSilently();
    applyCategoryAppearance();
    rebuildNavItems();
}

void MainWindow::applyCategoryAppearance()
{
    const LauncherCategoryAppearance appearance = m_document.settings.categoryAppearance;
    for (QToolButton *button : std::as_const(m_navButtons)) {
        if (!button) {
            continue;
        }
        button->setFixedHeight(appearance.buttonHeight);
        button->setIconSize(QSize(appearance.iconWidth, appearance.iconHeight));
        applyTextAppearanceFont(button, appearance.fontFamily, appearance.fontPointSize, QFont::DemiBold);
        button->setStyleSheet(textAppearanceStyleSheet(
            appearance.fontFamily,
            appearance.fontPointSize,
            appearance.textColor,
            700));
    }
}

int MainWindow::fixedLauncherWidth() const
{
    const LauncherItemAppearance appearance = m_document.settings.itemAppearance;
    const int cellWidth = appearance.itemWidth + appearance.horizontalSpacing;
    return FixedIconColumns * cellWidth + SectionHorizontalMargin * 2 + ScrollBarReserveWidth;
}

void MainWindow::applyFixedLauncherWidth()
{
    const int width = fixedLauncherWidth();
    setMinimumSize(width, WindowMinimumHeight);
    setMaximumSize(width, QWIDGETSIZE_MAX);
    setMaximumWidth(width);
    if (this->width() != width) {
        resize(width, height());
    }
}

bool MainWindow::effectiveDarkTheme() const
{
    if (m_document.settings.themeMode == "dark") {
        return true;
    }
    if (m_document.settings.themeMode == "light") {
        return false;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qApp->styleHints() && qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

void MainWindow::applyTheme()
{
    setStyleSheet(effectiveDarkTheme() ? darkStyleSheet() : lightStyleSheet());
    applyCategoryAppearance();
    if (m_themeSystemAction) {
        const QSignalBlocker blocker(m_themeSystemAction);
        m_themeSystemAction->setChecked(m_document.settings.themeMode == "system");
    }
    if (m_themeLightAction) {
        const QSignalBlocker blocker(m_themeLightAction);
        m_themeLightAction->setChecked(m_document.settings.themeMode == "light");
    }
    if (m_themeDarkAction) {
        const QSignalBlocker blocker(m_themeDarkAction);
        m_themeDarkAction->setChecked(m_document.settings.themeMode == "dark");
    }
}

void MainWindow::upsertRule(const HotkeyRule &rule)
{
    const int index = ruleIndexById(rule.id);
    if (index >= 0) {
        m_document.rules[index] = rule;
    } else {
        m_document.rules.push_back(rule);
    }
    saveDocument();
}

void MainWindow::setStatus(const QString &message)
{
    if (m_statusLabel) {
        m_statusLabel->setText(message);
    }
}

void MainWindow::showSectionMenu(const QString &sectionId, const QPoint &globalPos)
{
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }

    QMenu menu(this);
    menu.addAction(uiText(UiText::Key::NewSection), this, [this]() {
        addSection(m_currentCategory);
    });
    menu.addAction(uiText(UiText::Key::EditSection), this, [this, sectionId]() {
        editSection(sectionId);
    });
    menu.addAction(uiText(UiText::Key::DeleteSection), this, [this, sectionId]() {
        deleteSection(sectionId);
    });
    menu.addAction(uiText(UiText::Key::EncryptSection), this, [this, sectionId]() {
        encryptSection(sectionId);
    });
    if (m_document.sections[index].encrypted && !m_unlockedSectionIds.contains(sectionId)) {
        menu.addSeparator();
        menu.addAction(uiText(UiText::Key::UnlockSection), this, [this, sectionId]() {
            if (ensureSectionUnlocked(sectionId)) {
                refreshLauncher();
            }
        });
    }
    menu.exec(globalPos);
}

void MainWindow::showListMenu(const QString &sectionId, QListWidget *list, const QPoint &viewportPos)
{
    if (sectionIndexById(sectionId) < 0) {
        return;
    }
    QListWidgetItem *item = list ? list->itemAt(viewportPos) : nullptr;
    QMenu menu(this);
    if (item) {
        const QString ruleId = item->data(RuleIdRole).toString();
        menu.addAction(uiText(UiText::Key::Run), this, [this, ruleId]() {
            runRule(ruleId);
        });
        menu.addAction(uiText(UiText::Key::Edit), this, [this, ruleId]() {
            editRule(ruleId);
        });
        menu.addAction(uiText(UiText::Key::Delete), this, [this, ruleId]() {
            deleteRule(ruleId);
        });
    } else {
        menu.addAction(uiText(UiText::Key::AddItem), this, [this, sectionId]() {
            addRuleToSection(sectionId);
        });
    }
    menu.exec(list->viewport()->mapToGlobal(viewportPos));
}

void MainWindow::addSection(LauncherCategory category)
{
    int nextOrder = 0;
    for (const LauncherSection &section : m_document.sections) {
        if (section.category == category) {
            nextOrder = qMax(nextOrder, section.sortOrder + 1);
        }
    }

    QDialog dialog(this);
    dialog.setWindowTitle(uiText(UiText::Key::NewSectionTitle));
    auto *form = new QFormLayout;
    auto *nameEdit = new QLineEdit(&dialog);
    auto *iconEdit = new QLineEdit(LauncherSection::categoryName(category).toLower(), &dialog);
    auto *orderEdit = new QLineEdit(QString::number(nextOrder), &dialog);
    form->addRow(uiText(UiText::Key::Name), nameEdit);
    form->addRow(uiText(UiText::Key::IconKey), iconEdit);
    form->addRow(uiText(UiText::Key::SortOrder), orderEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    localizeDialogButtons(buttons, language());
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        showWarning(this, language(), uiText(UiText::Key::InvalidSection), uiText(UiText::Key::SectionNameRequired));
        return;
    }

    bool ok = false;
    const int order = orderEdit->text().toInt(&ok);

    LauncherSection section;
    section.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    section.category = category;
    section.name = name;
    section.iconKey = iconEdit->text().trimmed();
    section.sortOrder = ok ? order : nextOrder;
    m_document.sections.push_back(section);
    saveDocument();
}

void MainWindow::editSection(const QString &sectionId)
{
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }

    LauncherSection &section = m_document.sections[index];
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(uiText(UiText::Key::EditSectionTitle));
    auto *form = new QFormLayout;
    auto *nameEdit = new QLineEdit(section.name, dialog);
    auto *iconEdit = new QLineEdit(section.iconKey, dialog);
    auto *orderEdit = new QLineEdit(QString::number(section.sortOrder), dialog);
    form->addRow(uiText(UiText::Key::Name), nameEdit);
    form->addRow(uiText(UiText::Key::IconKey), iconEdit);
    form->addRow(uiText(UiText::Key::SortOrder), orderEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    localizeDialogButtons(buttons, language());
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog->exec() == QDialog::Accepted) {
        const QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            showWarning(this, language(), uiText(UiText::Key::InvalidSection), uiText(UiText::Key::SectionNameRequired));
        } else {
            section.name = name;
            section.iconKey = iconEdit->text().trimmed();
            bool ok = false;
            const int order = orderEdit->text().toInt(&ok);
            if (ok) {
                section.sortOrder = order;
            }
            saveDocument();
        }
    }
    dialog->deleteLater();
}

void MainWindow::deleteSection(const QString &sectionId)
{
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }

    const LauncherSection section = m_document.sections[index];
    const int itemCount = std::count_if(m_document.rules.cbegin(), m_document.rules.cend(), [&sectionId](const HotkeyRule &rule) {
        return rule.sectionId == sectionId;
    });
    const QString message = itemCount > 0
        ? uiText(UiText::Key::DeleteSectionWithItems).arg(sectionDisplayName(language(), section)).arg(itemCount)
        : uiText(UiText::Key::DeleteSectionConfirm).arg(sectionDisplayName(language(), section));
    if (!confirm(this, language(), uiText(UiText::Key::DeleteSectionTitle), message)) {
        return;
    }

    m_document.sections.removeAt(index);
    m_document.rules.erase(std::remove_if(m_document.rules.begin(), m_document.rules.end(), [&sectionId](const HotkeyRule &rule) {
        return rule.sectionId == sectionId;
    }), m_document.rules.end());
    m_unlockedSectionIds.remove(sectionId);
    saveDocument();
}

void MainWindow::encryptSection(const QString &sectionId)
{
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }

    LauncherSection &section = m_document.sections[index];
    if (section.encrypted && !ensureSectionUnlocked(sectionId)) {
        return;
    }

    bool ok = false;
    const QString password = passwordInput(this, language(), uiText(UiText::Key::EncryptSectionTitle), uiText(UiText::Key::EncryptSectionPrompt), &ok);
    if (!ok) {
        return;
    }

    section.encrypted = !password.isEmpty();
    section.passwordHash = password.isEmpty() ? QString() : passwordHash(password);
    if (section.encrypted) {
        m_unlockedSectionIds.insert(sectionId);
    } else {
        m_unlockedSectionIds.remove(sectionId);
    }
    saveDocument();
}

void MainWindow::expandSectionOnly(const QString &sectionId)
{
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }

    const LauncherCategory category = m_document.sections[index].category;
    bool changed = false;
    for (LauncherSection &section : m_document.sections) {
        if (section.category != category) {
            continue;
        }
        const bool shouldCollapse = section.id != sectionId;
        if (section.collapsed != shouldCollapse) {
            section.collapsed = shouldCollapse;
            changed = true;
        }
    }

    if (changed) {
        saveDocument();
    }
}

void MainWindow::toggleSectionCollapsed(const QString &sectionId)
{
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }
    m_document.sections[index].collapsed = !m_document.sections[index].collapsed;
    saveDocument();
    rebuildSections();
}

void MainWindow::addRuleToSection(const QString &sectionId)
{
    const int sectionIndex = sectionIndexById(sectionId);
    if (sectionIndex < 0) {
        return;
    }
    const LauncherSection &section = m_document.sections[sectionIndex];
    if (!ensureSectionUnlocked(sectionId)) {
        return;
    }

    RuleDialog dialog(language(), this);
    dialog.setContext(section.category, section.id);
    const bool hookWasPaused = m_hookService.isPaused();
    m_hookService.setPaused(true);
    const int dialogResult = dialog.exec();
    m_hookService.setPaused(hookWasPaused);
    if (dialogResult != QDialog::Accepted) {
        return;
    }

    HotkeyRule rule = dialog.rule();
    if (!rule.isValid()) {
        showWarning(this, language(), uiText(UiText::Key::InvalidItem), uiText(UiText::Key::ItemTargetRequired));
        return;
    }
    const QStringList warnings = m_store.warningsForRule(rule, m_document.rules, language());
    if (!warnings.isEmpty()) {
        showWarning(this, language(), uiText(UiText::Key::HotkeyWarning), warnings.join("\n"));
    }
    upsertRule(rule);
}

void MainWindow::addDroppedPathsToSection(const QString &sectionId, const QList<QUrl> &urls)
{
    const int sectionIndex = sectionIndexById(sectionId);
    if (sectionIndex < 0 || urls.isEmpty()) {
        return;
    }
    const LauncherSection section = m_document.sections[sectionIndex];
    if (!ensureSectionUnlocked(sectionId)) {
        return;
    }

    int addedCount = 0;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QFileInfo info(url.toLocalFile());
        if (!info.exists()) {
            continue;
        }

        HotkeyRule rule;
        rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        rule.enabled = true;
        rule.category = section.category;
        rule.sectionId = section.id;
        rule.action.target = info.absoluteFilePath();
        rule.description = info.isDir() ? info.fileName() : info.completeBaseName();

        if (section.category == LauncherCategory::Website || (section.category == LauncherCategory::Program && info.isDir())) {
            continue;
        }
        if (section.category == LauncherCategory::Folder) {
            rule.action.type = info.isDir() ? LaunchActionType::Folder : LaunchActionType::File;
        } else {
            rule.action.type = info.isFile() && info.suffix().compare("exe", Qt::CaseInsensitive) == 0
                ? LaunchActionType::Application
                : LaunchActionType::File;
        }
        if (rule.action.type == LaunchActionType::Application) {
            rule.action.workingDirectory = info.absolutePath();
        }

        m_document.rules.push_back(rule);
        ++addedCount;
    }

    if (addedCount > 0) {
        saveDocument();
    }
}

void MainWindow::editRule(const QString &ruleId)
{
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    if (!ensureSectionUnlocked(m_document.rules[index].sectionId)) {
        return;
    }

    RuleDialog dialog(language(), this);
    dialog.setRule(m_document.rules[index]);
    const bool hookWasPaused = m_hookService.isPaused();
    m_hookService.setPaused(true);
    const int dialogResult = dialog.exec();
    m_hookService.setPaused(hookWasPaused);
    if (dialogResult != QDialog::Accepted) {
        return;
    }

    HotkeyRule rule = dialog.rule();
    rule.enabled = m_document.rules[index].enabled;
    if (!rule.isValid()) {
        showWarning(this, language(), uiText(UiText::Key::InvalidItem), uiText(UiText::Key::ItemTargetRequired));
        return;
    }
    const QStringList warnings = m_store.warningsForRule(rule, m_document.rules, language());
    if (!warnings.isEmpty()) {
        showWarning(this, language(), uiText(UiText::Key::HotkeyWarning), warnings.join("\n"));
    }
    upsertRule(rule);
}

void MainWindow::deleteRule(const QString &ruleId)
{
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    if (!ensureSectionUnlocked(m_document.rules[index].sectionId)) {
        return;
    }

    if (!confirm(this, language(), uiText(UiText::Key::DeleteItemTitle), uiText(UiText::Key::DeleteItemConfirm).arg(ruleTitle(m_document.rules[index])))) {
        return;
    }
    m_document.rules.removeAt(index);
    saveDocument();
}

void MainWindow::runRule(const QString &ruleId)
{
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    if (!ensureSectionUnlocked(m_document.rules[index].sectionId)) {
        return;
    }
    onHotkeyTriggered(m_document.rules[index]);
}

bool MainWindow::ensureSectionUnlocked(const QString &sectionId)
{
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return false;
    }
    const LauncherSection &section = m_document.sections[index];
    if (isSectionUnlocked(section)) {
        return true;
    }

    bool ok = false;
    const QString password = passwordInput(this, language(), uiText(UiText::Key::UnlockSectionTitle), uiText(UiText::Key::UnlockSectionPrompt), &ok);
    if (!ok) {
        return false;
    }
    if (passwordHash(password) != section.passwordHash) {
        showWarning(this, language(), uiText(UiText::Key::WrongPasswordTitle), uiText(UiText::Key::WrongPasswordMessage));
        return false;
    }
    m_unlockedSectionIds.insert(sectionId);
    return true;
}

bool MainWindow::isSectionUnlocked(const LauncherSection &section) const
{
    return !section.encrypted || m_unlockedSectionIds.contains(section.id);
}

int MainWindow::sectionIndexById(const QString &sectionId) const
{
    for (int i = 0; i < m_document.sections.size(); ++i) {
        if (m_document.sections[i].id == sectionId) {
            return i;
        }
    }
    return -1;
}

int MainWindow::ruleIndexById(const QString &ruleId) const
{
    for (int i = 0; i < m_document.rules.size(); ++i) {
        if (m_document.rules[i].id == ruleId) {
            return i;
        }
    }
    return -1;
}

QIcon MainWindow::iconForRule(const HotkeyRule &rule) const
{
    switch (rule.category) {
    case LauncherCategory::Program: {
        const QString target = QFileInfo(rule.action.target).fileName().toLower();
        const QString title = rule.description.toLower();
        if (target == "control.exe" || title.contains(QString::fromUtf8("控制面板")) || title.contains("control")) {
            return themedIcon("control-panel");
        }
        if (target == "taskmgr.exe" || title.contains(QString::fromUtf8("任务管理器")) || title.contains("task")) {
            return themedIcon("task-manager");
        }
        if (target == "cmd.exe" || target == "powershell.exe" || title.contains(QString::fromUtf8("命令")) || title.contains("terminal")) {
            return themedIcon("terminal");
        }
        if (target == "regedit.exe" || title.contains(QString::fromUtf8("注册表")) || title.contains("registry")) {
            return themedIcon("registry");
        }
        if (target == "services.msc" || title.contains(QString::fromUtf8("服务")) || title.contains("services")) {
            return themedIcon("services");
        }
        if (target == "devmgmt.msc" || title.contains(QString::fromUtf8("设备")) || title.contains("device")) {
            return themedIcon("device-manager");
        }
        if (target == "calc.exe" || title.contains(QString::fromUtf8("计算器")) || title.contains("calculator")) {
            return themedIcon("calculator");
        }
        if (target == "msinfo32.exe" || title.contains(QString::fromUtf8("系统信息")) || title.contains("system info")) {
            return themedIcon("system-info");
        }
        const QFileInfo targetInfo(rule.action.target);
        if (targetInfo.isFile() && targetInfo.suffix().compare("exe", Qt::CaseInsensitive) == 0) {
            const QIcon fileIcon = QFileIconProvider().icon(targetInfo);
            if (!fileIcon.isNull()) {
                return fileIcon;
            }
        }
        if (rule.action.type == LaunchActionType::File && targetInfo.exists()) {
            const QIcon fileIcon = QFileIconProvider().icon(targetInfo);
            if (!fileIcon.isNull()) {
                return fileIcon;
            }
        }
        return style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    case LauncherCategory::Folder: {
        const QFileInfo targetInfo(rule.action.target);
        if (rule.action.type == LaunchActionType::File && targetInfo.exists()) {
            const QIcon fileIcon = QFileIconProvider().icon(targetInfo);
            if (!fileIcon.isNull()) {
                return fileIcon;
            }
        }
        const QString target = QFileInfo(rule.action.target).fileName().toLower();
        const QString title = rule.description.toLower();
        if (target == "desktop" || title.contains(QString::fromUtf8("桌面")) || title.contains("desktop")) {
            return themedIcon("desktop");
        }
        if (target == "documents" || title.contains(QString::fromUtf8("文档")) || title.contains("documents")) {
            return themedIcon("documents");
        }
        if (target == "downloads" || title.contains(QString::fromUtf8("下载")) || title.contains("downloads")) {
            return themedIcon("downloads");
        }
        return themedIcon("folder");
    }
    case LauncherCategory::Website:
        return themedIcon("globe");
    }
    return style()->standardIcon(QStyle::SP_FileIcon);
}

QIcon MainWindow::iconForCategory(LauncherCategory category) const
{
    switch (category) {
    case LauncherCategory::Program:
        return style()->standardIcon(QStyle::SP_ComputerIcon);
    case LauncherCategory::Folder:
        return themedIcon("folder");
    case LauncherCategory::Website:
        return themedIcon("globe");
    }
    return style()->standardIcon(QStyle::SP_FileIcon);
}

QIcon MainWindow::iconForSection(const LauncherSection &section) const
{
    if (section.encrypted) {
        return style()->standardIcon(QStyle::SP_MessageBoxWarning);
    }
    if (section.iconKey == "folder-system") {
        return themedIcon("folder");
    }
    if (section.iconKey == "website" || section.iconKey == "website-user") {
        return themedIcon("globe");
    }
    return iconForCategory(section.category);
}

QIcon MainWindow::themedIcon(const QString &key) const
{
    const QString normalized = key.toLower();
    const QString path = QString(":/icon-%1.svg").arg(normalized);
    QIcon icon(path);
    if (!icon.isNull()) {
        return icon;
    }
    return QIcon(":/app.svg");
}

bool MainWindow::rulePassesFilters(const HotkeyRule &rule) const
{
    if (rule.category != m_currentCategory) {
        return false;
    }

    const QString search = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    if (!search.isEmpty()) {
        const QString haystack = QString("%1 %2 %3 %4")
            .arg(ruleTitle(rule), rule.action.target, rule.description, rule.hotkey.displayText());
        if (!haystack.contains(search, Qt::CaseInsensitive)) {
            return false;
        }
    }
    return true;
}

QString MainWindow::ruleTitle(const HotkeyRule &rule) const
{
    if (!rule.description.trimmed().isEmpty()) {
        return rule.description.trimmed();
    }
    if (rule.category == LauncherCategory::Website) {
        return rule.action.target;
    }
    const QFileInfo info(rule.action.target);
    if (!info.fileName().isEmpty()) {
        return info.fileName();
    }
    return rule.action.target;
}

QString MainWindow::categoryDisplayName(LauncherCategory category) const
{
    return UiText::categoryName(language(), category);
}

QString MainWindow::passwordHash(const QString &password) const
{
    return QString::fromLatin1(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString MainWindow::uiText(UiText::Key key) const
{
    return UiText::text(language(), key);
}

MainWindow::ResizeRegion MainWindow::resizeRegionAt(const QPoint &position) const
{
    constexpr int edge = 18;
    if (!rect().contains(position)) {
        return ResizeRegion::None;
    }
    const bool bottom = position.y() >= height() - edge;
    if (bottom) {
        return ResizeRegion::Bottom;
    }
    return ResizeRegion::None;
}

void MainWindow::updateResizeCursor(const QPoint &position)
{
    if (m_resizing) {
        return;
    }
    if (!rect().contains(position)) {
        unsetCursor();
        return;
    }

    switch (resizeRegionAt(position)) {
    case ResizeRegion::Left:
    case ResizeRegion::Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case ResizeRegion::Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case ResizeRegion::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case ResizeRegion::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case ResizeRegion::None:
        unsetCursor();
        break;
    }
}

void MainWindow::performResize(const QPoint &globalPosition)
{
    QRect next = m_resizeStartGeometry;
    const QPoint delta = globalPosition - m_resizeStartGlobal;
    const int minW = minimumWidth();
    const int minH = minimumHeight();

    switch (m_resizeRegion) {
    case ResizeRegion::Left:
    case ResizeRegion::BottomLeft:
        next.setLeft(qMin(next.right() - minW + 1, m_resizeStartGeometry.left() + delta.x()));
        break;
    case ResizeRegion::Right:
    case ResizeRegion::BottomRight:
        next.setRight(qMax(next.left() + minW - 1, m_resizeStartGeometry.right() + delta.x()));
        break;
    case ResizeRegion::Bottom:
    case ResizeRegion::None:
        break;
    }

    switch (m_resizeRegion) {
    case ResizeRegion::Bottom:
    case ResizeRegion::BottomLeft:
    case ResizeRegion::BottomRight:
        next.setBottom(qMax(next.top() + minH - 1, m_resizeStartGeometry.bottom() + delta.y()));
        break;
    case ResizeRegion::Left:
    case ResizeRegion::Right:
    case ResizeRegion::None:
        break;
    }

    setGeometry(next);
}

void MainWindow::finishInteractiveMove()
{
    if (!m_resizing) {
        snapToTopIfNeeded();
    }
}

void MainWindow::snapToTopIfNeeded()
{
    if (m_topAutoHidden || !isVisible() || isMinimized()) {
        return;
    }

    const QPoint cursorPosition = QCursor::pos();
    QScreen *targetScreen = QGuiApplication::screenAt(cursorPosition);
    const QRect screenGeometry = targetScreen ? targetScreen->availableGeometry() : currentScreenAvailableGeometry();
    if (screenGeometry.isNull()) {
        return;
    }

    const int windowTopDistance = qAbs(frameGeometry().top() - screenGeometry.top());
    const int cursorTopDistance = qAbs(cursorPosition.y() - screenGeometry.top());
    if (windowTopDistance > AutoHideSnapDistance && cursorTopDistance > AutoHideSnapDistance) {
        m_autoHideShownGeometry = {};
        m_autoHideTimer.stop();
        return;
    }

    QRect next = geometry();
    next.moveTop(screenGeometry.top());
    next.moveLeft(qBound(screenGeometry.left(), next.left(), screenGeometry.right() - next.width() + 1));
    setGeometry(next);
    m_autoHideShownGeometry = geometry();
    setAlwaysOnTop(true);
    m_autoHideTimer.start();
}

void MainWindow::setTopAutoHidden(bool hidden)
{
    if (m_topAutoHidden == hidden) {
        return;
    }

    if (hidden) {
        m_autoHideShownGeometry = geometry();
        const QRect screenGeometry = currentScreenAvailableGeometry();
        QRect hiddenGeometry = m_autoHideShownGeometry;
        hiddenGeometry.setHeight(AutoHideTriggerHeight);
        hiddenGeometry.moveTop(screenGeometry.isNull() ? m_autoHideShownGeometry.top() : screenGeometry.top());
        if (!screenGeometry.isNull()) {
            hiddenGeometry.moveLeft(qBound(screenGeometry.left(), hiddenGeometry.left(), screenGeometry.right() - hiddenGeometry.width() + 1));
        }
        m_topAutoHidden = true;
        setAlwaysOnTop(true);
        setMinimumHeight(AutoHideTriggerHeight);
        setMaximumHeight(AutoHideTriggerHeight);
        setGeometry(hiddenGeometry);
        unsetCursor();
        m_autoHideTimer.start();
        return;
    }

    m_topAutoHidden = false;
    setAlwaysOnTop(true);
    setMinimumHeight(WindowMinimumHeight);
    setMaximumHeight(QWIDGETSIZE_MAX);
    if (m_autoHideShownGeometry.isValid()) {
        const QRect screenGeometry = currentScreenAvailableGeometry();
        QRect shownGeometry = m_autoHideShownGeometry;
        if (!screenGeometry.isNull()) {
            shownGeometry.moveLeft(qBound(screenGeometry.left(), shownGeometry.left(), screenGeometry.right() - shownGeometry.width() + 1));
            shownGeometry.moveTop(screenGeometry.top());
        }
        setGeometry(shownGeometry);
    }
}

void MainWindow::revealFromTopAutoHide()
{
    if (m_topAutoHidden) {
        setTopAutoHidden(false);
    }
}

void MainWindow::updateTopAutoHide()
{
    if (!isVisible() || isMinimized()) {
        m_autoHideTimer.stop();
        return;
    }

    const QPoint cursorPosition = QCursor::pos();
    if (m_topAutoHidden) {
        setAlwaysOnTop(true);
        const QRect triggerRect = geometry().adjusted(0, 0, 0, AutoHideRevealDistance);
        if (triggerRect.contains(cursorPosition)) {
            revealFromTopAutoHide();
        }
        return;
    }

    if (!m_autoHideShownGeometry.isValid()) {
        m_autoHideTimer.stop();
        return;
    }

    if (m_resizing || !m_dragPosition.isNull()) {
        return;
    }

    if (!geometry().contains(cursorPosition)) {
        setTopAutoHidden(true);
    }
}

void MainWindow::setAlwaysOnTop(bool enabled)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return;
    }
    const HWND insertAfter = enabled ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(hwnd,
                 insertAfter,
                 0,
                 0,
                 0,
                 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
#else
    Q_UNUSED(enabled);
#endif
}

QScreen *MainWindow::currentScreen() const
{
    if (QScreen *windowScreen = screen()) {
        return windowScreen;
    }
    return QGuiApplication::screenAt(frameGeometry().center());
}

QRect MainWindow::currentScreenAvailableGeometry() const
{
    if (QScreen *targetScreen = currentScreen()) {
        return targetScreen->availableGeometry();
    }
    if (QScreen *primary = QGuiApplication::primaryScreen()) {
        return primary->availableGeometry();
    }
    return {};
}
