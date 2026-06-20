#include "MainWindow.h"

#include "AppIcon.h"
#include "QtCompat.h"
#include "RuleDialog.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QCursor>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QStyleHints>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextOption>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wincrypt.h>
#include <windows.h>
#endif

#ifdef Q_OS_WIN
#ifndef MSGFLT_ALLOW
#define MSGFLT_ALLOW 1
#endif
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

#if defined(Q_OS_WIN) && !defined(CALG_SHA_256)
#define CALG_SHA_256 (ALG_CLASS_HASH | ALG_TYPE_ANY | ALG_SID_SHA_256)
#endif

class IconGridDelegate final : public QStyledItemDelegate {
public:
    explicit IconGridDelegate(LauncherItemAppearance appearance, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_appearance(std::move(appearance)) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        QStyleOptionViewItemV4 opt(option);
#else
        QStyleOptionViewItem opt(option);
#endif
        initStyleOption(&opt, index);
        QString text = index.data(Qt::DisplayRole).toString();
        if (!m_appearance.multilineText) {
            text = text.simplified();
        }
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (icon.isNull()) {
            icon = opt.icon;
        }
        opt.text.clear();
        opt.icon = QIcon();

        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        const QRect itemRect = option.rect.adjusted(2, 2, -2, -2);
        QSize iconSize(m_appearance.iconWidth, m_appearance.iconHeight);
        if (option.decorationSize.isValid() && !option.decorationSize.isEmpty()) {
            iconSize = option.decorationSize;
        }
        const QRect iconRect(itemRect.left() + (itemRect.width() - iconSize.width()) / 2, itemRect.top() + 3,
                             iconSize.width(), iconSize.height());

        const QIcon::Mode iconMode = (opt.state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
        icon.paint(painter, iconRect, Qt::AlignCenter, iconMode);

        QRect textRect = itemRect;
        textRect.setTop(iconRect.bottom() + 3);
        textRect.adjust(0, 0, 0, -1);

        painter->save();
        const QVariant foreground = index.data(Qt::ForegroundRole);
        if (foreground.canConvert<QBrush>()) {
            painter->setPen(qvariant_cast<QBrush>(foreground).color());
        } else if (opt.state & QStyle::State_Selected) {
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
        textOption.setWrapMode(m_appearance.multilineText ? QTextOption::WrapAtWordBoundaryOrAnywhere
                                                          : QTextOption::NoWrap);
        if (m_appearance.showEllipsis) {
            QFontMetrics metrics(textFont);
            if (m_appearance.multilineText) {
                const QString elided =
                    metrics.elidedText(text.simplified(), Qt::ElideRight,
                                       textRect.width() * qMax(1, textRect.height() / qMax(1, metrics.lineSpacing())));
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

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return {m_appearance.itemWidth, m_appearance.itemHeight};
    }

private:
    LauncherItemAppearance m_appearance;
};
QVector<LauncherCategory> fixedCategories() {
    QVector<LauncherCategory> categories;
    categories << LauncherCategory::Program << LauncherCategory::Folder << LauncherCategory::Website;
    return categories;
}

UiText::Key defaultSectionNameKey(const QString& sectionId) {
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

bool isDefaultSectionId(const QString& sectionId) {
    return sectionId == "program-system" || sectionId == "program-user" || sectionId == "folder-system" ||
           sectionId == "folder-user" || sectionId == "website-common" || sectionId == "website-user";
}

QString sectionDisplayName(const QString& language, const LauncherSection& section) {
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

void localizeDialogButtons(QDialogButtonBox* buttons, const QString& language) {
    if (!buttons) {
        return;
    }
    if (QPushButton* button = buttons->button(QDialogButtonBox::Ok)) {
        button->setText(UiText::text(language, UiText::Key::Ok));
    }
    if (QPushButton* button = buttons->button(QDialogButtonBox::Cancel)) {
        button->setText(UiText::text(language, UiText::Key::Cancel));
    }
}

LauncherItemAppearance scaledAppearance(LauncherItemAppearance appearance) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    appearance.iconWidth = QtCompat::scaleInt(appearance.iconWidth);
    appearance.iconHeight = QtCompat::scaleInt(appearance.iconHeight);
    appearance.itemWidth = QtCompat::scaleInt(appearance.itemWidth);
    appearance.itemHeight = QtCompat::scaleInt(appearance.itemHeight);
    appearance.fontPointSize = QtCompat::scaleInt(appearance.fontPointSize);
    appearance.horizontalSpacing = QtCompat::scaleInt(appearance.horizontalSpacing);
    appearance.verticalSpacing = QtCompat::scaleInt(appearance.verticalSpacing);
#endif
    return appearance;
}

LauncherSectionAppearance scaledAppearance(LauncherSectionAppearance appearance) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    appearance.iconWidth = QtCompat::scaleInt(appearance.iconWidth);
    appearance.iconHeight = QtCompat::scaleInt(appearance.iconHeight);
    appearance.headerHeight = QtCompat::scaleInt(appearance.headerHeight);
    appearance.fontPointSize = QtCompat::scaleInt(appearance.fontPointSize);
#endif
    return appearance;
}

LauncherCategoryAppearance scaledAppearance(LauncherCategoryAppearance appearance) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    appearance.iconWidth = QtCompat::scaleInt(appearance.iconWidth);
    appearance.iconHeight = QtCompat::scaleInt(appearance.iconHeight);
    appearance.buttonHeight = QtCompat::scaleInt(appearance.buttonHeight);
    appearance.fontPointSize = QtCompat::scaleInt(appearance.fontPointSize);
#endif
    return appearance;
}

QDialogButtonBox* makeDialogButtons(QDialogButtonBox::StandardButtons buttons, QWidget* parent) {
    return new QDialogButtonBox(buttons, Qt::Horizontal, parent);
}

void connectDialogClose(QDialogButtonBox* buttons, QDialog* dialog) {
    QObject::connect(buttons, SIGNAL(rejected()), dialog, SLOT(close()));
}

void connectDialogAcceptReject(QDialogButtonBox* buttons, QDialog* dialog) {
    QObject::connect(buttons, SIGNAL(accepted()), dialog, SLOT(accept()));
    QObject::connect(buttons, SIGNAL(rejected()), dialog, SLOT(reject()));
}

QListWidget* ancestorListWidget(QWidget* widget) {
    while (widget) {
        if (auto* list = qobject_cast<QListWidget*>(widget)) {
            return list;
        }
        widget = widget->parentWidget();
    }
    return nullptr;
}

void showWarning(QWidget* parent, const QString& language, const QString& title, const QString& message) {
    QMessageBox box(QMessageBox::Warning, title, message, QMessageBox::Ok, parent);
    if (QAbstractButton* button = box.button(QMessageBox::Ok)) {
        button->setText(UiText::text(language, UiText::Key::Ok));
    }
    box.exec();
}

bool confirm(QWidget* parent, const QString& language, const QString& title, const QString& message) {
    QMessageBox box(QMessageBox::Question, title, message, QMessageBox::Yes | QMessageBox::No, parent);
    if (QAbstractButton* button = box.button(QMessageBox::Yes)) {
        button->setText(UiText::text(language, UiText::Key::Yes));
    }
    if (QAbstractButton* button = box.button(QMessageBox::No)) {
        button->setText(UiText::text(language, UiText::Key::No));
    }
    return box.exec() == QMessageBox::Yes;
}

QString cssQuoted(const QString& value) {
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return QString("\"%1\"").arg(escaped);
}

QString textAppearanceStyleSheet(const QString& fontFamily, int fontPointSize, const QString& textColor,
                                 int fontWeight) {
    QStringList rules;
    if (!fontFamily.trimmed().isEmpty()) {
        rules << QString("font-family: %1;").arg(cssQuoted(fontFamily.trimmed()));
    }
    rules << QString("font-size: %1pt;").arg(fontPointSize);
    rules << QString("font-weight: %1;").arg(fontWeight);
    if (!textColor.trimmed().isEmpty()) {
        rules << QString("color: %1;").arg(textColor.trimmed());
    }
    return rules.join(QString(" "));
}

void applyTextAppearanceFont(QWidget* widget, const QString& fontFamily, int fontPointSize, QFont::Weight fontWeight) {
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

QPoint eventPosition(QDropEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

QPoint eventPosition(QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

QPoint eventGlobalPosition(QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

Qt::MouseButton eventButton(QMouseEvent* event) {
    return event ? event->button() : Qt::NoButton;
}

void updateColorButton(QPushButton* button, const QString& language, const QString& color) {
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

QString passwordInput(QWidget* parent, const QString& language, const QString& title, const QString& prompt, bool* ok) {
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
struct ComInitializer {
    ComInitializer() {
        result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    }

    ~ComInitializer() {
        if (SUCCEEDED(result)) {
            CoUninitialize();
        }
    }

    HRESULT result = E_FAIL;
};

void forceWindowForeground(QWidget* widget) {
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

void allowElevatedDragDrop(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    using ChangeWindowMessageFilterExFn = BOOL(WINAPI*)(HWND, UINT, DWORD, void*);
    auto* user32 = GetModuleHandleW(L"user32.dll");
    auto* changeFilter = reinterpret_cast<ChangeWindowMessageFilterExFn>(
        user32 ? GetProcAddress(user32, "ChangeWindowMessageFilterEx") : nullptr);
    if (!changeFilter) {
        return;
    }
    changeFilter(hwnd, WM_DROPFILES, MSGFLT_ALLOW, nullptr);
    changeFilter(hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
    changeFilter(hwnd, 0x0049, MSGFLT_ALLOW, nullptr);
}

QString fromWideString(const wchar_t* value, int length = -1) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(value), length);
#else
    return QString::fromUtf16(reinterpret_cast<const ushort*>(value), length);
#endif
}

std::wstring toWideString(const QString& value) {
    return std::wstring(reinterpret_cast<const wchar_t*>(value.utf16()), static_cast<size_t>(value.length()));
}

bool shellExecutePath(const QString& path, const wchar_t* verb, const QString& parameters = {},
                      const QString& workingDirectory = {}) {
    const std::wstring nativePath = toWideString(QDir::toNativeSeparators(path));
    const std::wstring nativeParameters = toWideString(parameters);
    const std::wstring nativeWorkingDirectory = toWideString(QDir::toNativeSeparators(workingDirectory));

    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.lpVerb = verb;
    info.lpFile = nativePath.c_str();
    info.lpParameters = nativeParameters.empty() ? nullptr : nativeParameters.c_str();
    info.lpDirectory = nativeWorkingDirectory.empty() ? nullptr : nativeWorkingDirectory.c_str();
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info);
}

QString windowsKnownExecutablePath(const QString& fileName) {
    const QString normalized = fileName.trimmed();
    if (normalized.isEmpty() || QFileInfo(normalized).isAbsolute()) {
        return normalized;
    }

    wchar_t windowsPath[MAX_PATH] = {};
    const UINT windowsLength = GetWindowsDirectoryW(windowsPath, MAX_PATH);
    if (windowsLength > 0 && windowsLength < MAX_PATH) {
        const QDir windowsDir(fromWideString(windowsPath, static_cast<int>(windowsLength)));
        const QString system32Path = windowsDir.filePath(QString::fromLatin1("System32/%1").arg(normalized));
        if (QFileInfo(system32Path).exists()) {
            return system32Path;
        }
        const QString windowsFilePath = windowsDir.filePath(normalized);
        if (QFileInfo(windowsFilePath).exists()) {
            return windowsFilePath;
        }
    }

    return normalized;
}

QPixmap pixmapFromNativeIcon(HICON icon, int width, int height) {
    if (!icon || width <= 0 || height <= 0) {
        return QPixmap();
    }

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        return QPixmap();
    }

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC dc = bitmap ? CreateCompatibleDC(screenDc) : nullptr;
    ReleaseDC(nullptr, screenDc);

    if (!bitmap || !dc || !bits) {
        if (dc) {
            DeleteDC(dc);
        }
        if (bitmap) {
            DeleteObject(bitmap);
        }
        return QPixmap();
    }

    std::memset(bits, 0, static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    HGDIOBJ previousBitmap = SelectObject(dc, bitmap);
    DrawIconEx(dc, 0, 0, icon, width, height, 0, nullptr, DI_NORMAL);

    QImage image(static_cast<uchar*>(bits), width, height, QImage::Format_ARGB32);
    QPixmap pixmap = QPixmap::fromImage(image.copy());

    SelectObject(dc, previousBitmap);
    DeleteDC(dc);
    DeleteObject(bitmap);
    return pixmap;
}

QIcon nativeFileIcon(const QString& path) {
    const QString resolvedPath = windowsKnownExecutablePath(path);
    const bool exists = QFileInfo(resolvedPath).exists();
    const QString nativePath = QDir::toNativeSeparators(resolvedPath);
    const std::wstring nativePathW = toWideString(nativePath);
    SHFILEINFOW fileInfo = {};
    const DWORD_PTR result =
        SHGetFileInfoW(nativePathW.c_str(), exists ? 0 : FILE_ATTRIBUTE_NORMAL, &fileInfo, sizeof(fileInfo),
                       SHGFI_ICON | SHGFI_SMALLICON | (exists ? 0 : SHGFI_USEFILEATTRIBUTES));
    if (!result || !fileInfo.hIcon) {
        return QIcon();
    }
    const QPixmap pixmap =
        pixmapFromNativeIcon(fileInfo.hIcon, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    QIcon icon(pixmap);
    DestroyIcon(fileInfo.hIcon);
    return icon;
}

bool revealInExplorer(const QString& path) {
    const QFileInfo info(path);
    if (!info.exists()) {
        return false;
    }
    if (info.isDir()) {
        return shellExecutePath(info.absoluteFilePath(), L"open");
    }
    return shellExecutePath("explorer.exe", L"open",
                            QString("/select,\"%1\"").arg(QDir::toNativeSeparators(info.absoluteFilePath())));
}

QString desktopDirectoryPath() {
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return fromWideString(path);
    }
    return QDir::home().filePath("Desktop");
}

QString startupDirectoryPath() {
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return fromWideString(path);
    }
    return QDir::home().filePath("AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Startup");
}

bool createShortcutFile(const QString& shortcutPath, const HotkeyRule& rule, const QString& title) {
    ComInitializer com;
    Q_UNUSED(com);

    IShellLinkW* link = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                  reinterpret_cast<void**>(&link));
    if (FAILED(hr) || !link) {
        return false;
    }

    const QString target = QDir::toNativeSeparators(rule.action.target);
    const QString workingDirectory = QDir::toNativeSeparators(rule.action.workingDirectory);
    const std::wstring targetW = toWideString(target);
    const std::wstring argumentsW = toWideString(rule.action.arguments);
    const std::wstring workingDirectoryW = toWideString(workingDirectory);
    const std::wstring descriptionW = toWideString(title);

    link->SetPath(targetW.c_str());
    if (!argumentsW.empty()) {
        link->SetArguments(argumentsW.c_str());
    }
    if (!workingDirectoryW.empty()) {
        link->SetWorkingDirectory(workingDirectoryW.c_str());
    }
    link->SetDescription(descriptionW.c_str());

    IPersistFile* persistFile = nullptr;
    hr = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistFile));
    if (FAILED(hr) || !persistFile) {
        link->Release();
        return false;
    }

    const std::wstring shortcutW = toWideString(QDir::toNativeSeparators(shortcutPath));
    hr = persistFile->Save(shortcutW.c_str(), TRUE);
    persistFile->Release();
    link->Release();
    return SUCCEEDED(hr);
}

bool showShellContextMenu(QWidget* parent, const QString& path, const QPoint& globalPos) {
    const QFileInfo info(path);
    if (!info.exists()) {
        return false;
    }

    ComInitializer com;
    Q_UNUSED(com);

    const QString absolutePath = QDir::toNativeSeparators(info.absoluteFilePath());
    PIDLIST_ABSOLUTE absolutePidl = nullptr;
    SFGAOF attributes = 0;
    HRESULT hr = SHParseDisplayName(reinterpret_cast<const wchar_t*>(absolutePath.utf16()), nullptr, &absolutePidl, 0,
                                    &attributes);
    if (FAILED(hr) || !absolutePidl) {
        return false;
    }

    PCUITEMID_CHILD child = nullptr;
    IShellFolder* parentFolder = nullptr;
    hr = SHBindToParent(absolutePidl, IID_IShellFolder, reinterpret_cast<void**>(&parentFolder), &child);
    if (FAILED(hr) || !parentFolder || !child) {
        CoTaskMemFree(absolutePidl);
        return false;
    }

    IContextMenu* contextMenu = nullptr;
    hr = parentFolder->GetUIObjectOf(parent ? reinterpret_cast<HWND>(parent->winId()) : nullptr, 1, &child,
                                     IID_IContextMenu, nullptr, reinterpret_cast<void**>(&contextMenu));
    if (FAILED(hr) || !contextMenu) {
        parentFolder->Release();
        CoTaskMemFree(absolutePidl);
        return false;
    }

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        contextMenu->Release();
        parentFolder->Release();
        CoTaskMemFree(absolutePidl);
        return false;
    }

    constexpr UINT commandBase = 1;
    constexpr UINT commandMax = 0x7FFF;
    hr = contextMenu->QueryContextMenu(menu, 0, commandBase, commandMax, CMF_NORMAL);
    if (SUCCEEDED(hr)) {
        const UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, globalPos.x(), globalPos.y(),
                                              parent ? reinterpret_cast<HWND>(parent->winId()) : nullptr, nullptr);
        if (command >= commandBase) {
            CMINVOKECOMMANDINFOEX invoke = {};
            invoke.cbSize = sizeof(invoke);
            invoke.fMask = CMIC_MASK_UNICODE | CMIC_MASK_PTINVOKE;
            invoke.hwnd = parent ? reinterpret_cast<HWND>(parent->winId()) : nullptr;
            invoke.lpVerb = MAKEINTRESOURCEA(command - commandBase);
            invoke.lpVerbW = MAKEINTRESOURCEW(command - commandBase);
            invoke.nShow = SW_SHOWNORMAL;
            invoke.ptInvoke.x = globalPos.x();
            invoke.ptInvoke.y = globalPos.y();
            contextMenu->InvokeCommand(reinterpret_cast<LPCMINVOKECOMMANDINFO>(&invoke));
        }
    }

    DestroyMenu(menu);
    contextMenu->Release();
    parentFolder->Release();
    CoTaskMemFree(absolutePidl);
    return SUCCEEDED(hr);
}
#endif

QString lightStyleSheet() {
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

QString darkStyleSheet() {
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
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("HotKeyManager");
    setWindowIcon(AppIcon::launcherIcon());
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setMouseTracking(true);
    resize(fixedLauncherWidth(), QtCompat::scaleInt(720));
    applyFixedLauncherWidth();
    buildUi();
    setAlwaysOnTop(true);
    qApp->installEventFilter(this);
    m_autoHideTimer.setInterval(AutoHidePollIntervalMs);
    connect(&m_autoHideTimer, SIGNAL(timeout()), this, SLOT(updateTopAutoHide()));

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
        if (m_document.settings.themeMode == "system") {
            applyTheme();
        }
    });
#endif

    connect(&m_hookService, SIGNAL(hotkeyTriggered(HotkeyRule)), this, SLOT(onHotkeyTriggered(HotkeyRule)));
    connect(&m_hookService, SIGNAL(hookError(QString)), this, SLOT(setStatus(QString)));

    loadDocument();

    QString hookError;
    if (!m_hookService.start(&hookError)) {
        showWarning(this, language(), uiText(UiText::Key::HotkeyHookFailed), hookError);
        setStatus(hookError);
    } else {
        setStatus(uiText(UiText::Key::HookRunning));
    }
}

void MainWindow::buildUi() {
    auto* root = new QWidget(this);
    root->setObjectName("root");
    root->setAcceptDrops(true);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QFrame(root);
    header->setObjectName("topPanel");
    header->setFixedHeight(QtCompat::scaleInt(HeaderHeight));
    header->setAcceptDrops(true);
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(14, 0, 10, 0);
    headerLayout->setSpacing(0);

    auto* titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(4);
    auto* brand = new QLabel("HSTART", header);
    brand->setObjectName("brand");
    brand->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto* brandIcon = new QLabel(header);
    brandIcon->setPixmap(windowIcon().pixmap(32, 32));
    m_ruleCountLabel = new QLabel(header);
    m_ruleCountLabel->setObjectName("countText");
    m_ruleCountLabel->setMinimumWidth(0);
    m_ruleCountLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_ruleCountLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_settingsButton = new QToolButton(header);
    m_settingsButton->setIcon(themedIcon("settings"));
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    m_settingsButton->setText("*");
#endif
    m_settingsButton->setPopupMode(QToolButton::DelayedPopup);
    m_settingsButton->setObjectName("windowButton");
    m_minButton = new QToolButton(header);
    m_minButton->setText("-");
    m_minButton->setObjectName("windowButton");
    m_closeButton = new QToolButton(header);
    m_closeButton->setText("X");
    m_closeButton->setObjectName("windowButton");
    QList<QToolButton*> windowButtons;
    windowButtons << m_settingsButton << m_minButton << m_closeButton;
    for (QToolButton* button : windowButtons) {
        button->setFixedSize(QtCompat::scaleInt(28), QtCompat::scaleInt(26));
        button->setIconSize(QSize(QtCompat::scaleInt(18), QtCompat::scaleInt(18)));
    }
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    m_settingsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
#else
    m_settingsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
#endif
    m_minButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_closeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_minButton, SIGNAL(clicked()), this, SLOT(showMinimized()));
    connect(m_closeButton, SIGNAL(clicked()), this, SLOT(hide()));
    connect(m_settingsButton, SIGNAL(clicked()), this, SLOT(showSettingsMenu()));

    titleRow->addWidget(brandIcon, 0, Qt::AlignVCenter);
    titleRow->addWidget(brand);
    titleRow->addStretch(1);
    titleRow->addWidget(m_ruleCountLabel, 0);
    titleRow->addWidget(m_settingsButton, 0, Qt::AlignTop);
    titleRow->addWidget(m_minButton, 0, Qt::AlignTop);
    titleRow->addWidget(m_closeButton, 0, Qt::AlignTop);
    headerLayout->addLayout(titleRow);

    auto* content = new QWidget(root);
    content->setObjectName("content");
    content->setAcceptDrops(true);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto* searchBand = new QFrame(content);
    searchBand->setObjectName("searchBand");
    searchBand->setAcceptDrops(true);
    auto* searchLayout = new QHBoxLayout(searchBand);
    searchLayout->setContentsMargins(12, 6, 12, 4);
    searchLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(searchBand);
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
    m_searchEdit->setClearButtonEnabled(true);
#endif
    searchLayout->addWidget(m_searchEdit, 1);

    m_navBar = new QWidget(content);
    m_navBar->setObjectName("navBar");
    m_navBar->setAcceptDrops(true);
    auto* navBarLayout = new QHBoxLayout(m_navBar);
    navBarLayout->setContentsMargins(0, 0, 0, 0);
    navBarLayout->setSpacing(0);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    m_navButtons.clear();
    for (LauncherCategory category : fixedCategories()) {
        auto* button = new QToolButton(m_navBar);
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIcon(iconForCategory(category));
        button->setText(categoryDisplayName(category));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        button->setProperty("category", static_cast<int>(category));
        m_navGroup->addButton(button, static_cast<int>(category));
        navBarLayout->addWidget(button, 1);
        m_navButtons.insert(category, button);
    }
    connect(m_navGroup, SIGNAL(buttonClicked(int)), this, SLOT(onCategoryButtonClicked(int)));

    contentLayout->addWidget(searchBand);
    contentLayout->addWidget(m_navBar);

    m_scrollArea = new QScrollArea(content);
    m_scrollArea->setObjectName("sectionScroll");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setAcceptDrops(true);
    m_scrollArea->viewport()->setAcceptDrops(true);

    m_sectionsContainer = new QWidget(m_scrollArea);
    m_sectionsContainer->setObjectName("sectionsContainer");
    m_sectionsContainer->setAcceptDrops(true);
    m_sectionsLayout = new QVBoxLayout(m_sectionsContainer);
    m_sectionsLayout->setContentsMargins(2, 12, 2, 12);
    m_sectionsLayout->setSpacing(10);
    m_scrollArea->setWidget(m_sectionsContainer);

    contentLayout->addWidget(m_scrollArea, 1);

    auto* statusBand = new QFrame(root);
    statusBand->setObjectName("statusBand");
    statusBand->setFixedHeight(QtCompat::scaleInt(34));
    auto* statusLayout = new QHBoxLayout(statusBand);
    statusLayout->setContentsMargins(12, 0, 12, 0);
    m_statusLabel = new QLabel(statusBand);
    m_statusLabel->setObjectName("statusText");
    statusLayout->addWidget(m_statusLabel);

    layout->addWidget(header);
    layout->addWidget(content, 1);
    layout->addWidget(statusBand);
    setCentralWidget(root);
    statusBar()->hide();
    setAcceptDrops(true);
    enablePointerTracking(root);
#ifdef Q_OS_WIN
    DragAcceptFiles(reinterpret_cast<HWND>(winId()), TRUE);
    DragAcceptFiles(reinterpret_cast<HWND>(root->winId()), TRUE);
    DragAcceptFiles(reinterpret_cast<HWND>(header->winId()), TRUE);
    DragAcceptFiles(reinterpret_cast<HWND>(content->winId()), TRUE);
    DragAcceptFiles(reinterpret_cast<HWND>(m_scrollArea->winId()), TRUE);
    DragAcceptFiles(reinterpret_cast<HWND>(m_scrollArea->viewport()->winId()), TRUE);
    DragAcceptFiles(reinterpret_cast<HWND>(m_sectionsContainer->winId()), TRUE);
    allowElevatedDragDrop(reinterpret_cast<HWND>(winId()));
    allowElevatedDragDrop(reinterpret_cast<HWND>(root->winId()));
    allowElevatedDragDrop(reinterpret_cast<HWND>(header->winId()));
    allowElevatedDragDrop(reinterpret_cast<HWND>(content->winId()));
    allowElevatedDragDrop(reinterpret_cast<HWND>(m_scrollArea->winId()));
    allowElevatedDragDrop(reinterpret_cast<HWND>(m_scrollArea->viewport()->winId()));
    allowElevatedDragDrop(reinterpret_cast<HWND>(m_sectionsContainer->winId()));
#endif

    buildSettingsMenu();
    connect(m_searchEdit, SIGNAL(textChanged(QString)), this, SLOT(refreshLauncher()));
    applyTheme();
}

void MainWindow::rebuildNavItems() {
    if (!m_navGroup) {
        return;
    }

    for (LauncherCategory category : fixedCategories()) {
        if (QToolButton* button = m_navButtons.value(category)) {
            const QSignalBlocker blocker(button);
            button->setText(categoryDisplayName(category));
            button->setIcon(iconForCategory(category));
            button->setChecked(category == m_currentCategory);
            button->setToolTip(categoryDisplayName(category));
        }
    }
    applyCategoryAppearance();
}

void MainWindow::buildSettingsMenu() {
    if (!m_settingsButton) {
        return;
    }

    if (m_settingsMenu) {
        m_settingsMenu->deleteLater();
    }
    m_settingsMenu = new QMenu(this);
    m_hotkeysEnabledAction = m_settingsMenu->addAction(uiText(UiText::Key::HotkeysEnabled));
    m_hotkeysEnabledAction->setCheckable(true);
    connect(m_hotkeysEnabledAction, SIGNAL(toggled(bool)), this, SLOT(applyHotkeysEnabled(bool)));
    m_hotkeyListAction = m_settingsMenu->addAction(uiText(UiText::Key::HotkeyList));
    connect(m_hotkeyListAction, SIGNAL(triggered()), this, SLOT(showHotkeyListDialog()));

    auto* languageMenu = m_settingsMenu->addMenu(uiText(UiText::Key::Language));
    auto* languageGroup = new QActionGroup(languageMenu);
    languageGroup->setExclusive(true);
    m_chineseAction = languageMenu->addAction(uiText(UiText::Key::Chinese));
    m_englishAction = languageMenu->addAction(uiText(UiText::Key::English));
    QList<QAction*> languageActions;
    languageActions << m_chineseAction << m_englishAction;
    for (QAction* action : languageActions) {
        action->setCheckable(true);
        languageGroup->addAction(action);
    }
    connect(m_chineseAction, SIGNAL(triggered()), this, SLOT(setLanguageChinese()));
    connect(m_englishAction, SIGNAL(triggered()), this, SLOT(setLanguageEnglish()));

    auto* themeMenu = m_settingsMenu->addMenu(uiText(UiText::Key::Theme));
    auto* themeGroup = new QActionGroup(themeMenu);
    themeGroup->setExclusive(true);
    m_themeSystemAction = themeMenu->addAction(uiText(UiText::Key::ThemeSystem));
    m_themeLightAction = themeMenu->addAction(uiText(UiText::Key::ThemeLight));
    m_themeDarkAction = themeMenu->addAction(uiText(UiText::Key::ThemeDark));
    QList<QAction*> themeActions;
    themeActions << m_themeSystemAction << m_themeLightAction << m_themeDarkAction;
    for (QAction* action : themeActions) {
        action->setCheckable(true);
        themeGroup->addAction(action);
    }
    connect(m_themeSystemAction, SIGNAL(triggered()), this, SLOT(setThemeSystem()));
    connect(m_themeLightAction, SIGNAL(triggered()), this, SLOT(setThemeLight()));
    connect(m_themeDarkAction, SIGNAL(triggered()), this, SLOT(setThemeDark()));

    m_itemAppearanceAction = m_settingsMenu->addAction(uiText(UiText::Key::ItemAppearance));
    connect(m_itemAppearanceAction, SIGNAL(triggered()), this, SLOT(showItemAppearanceDialog()));
    m_sectionAppearanceAction = m_settingsMenu->addAction(uiText(UiText::Key::SectionAppearance));
    connect(m_sectionAppearanceAction, SIGNAL(triggered()), this, SLOT(showSectionAppearanceDialog()));
    m_categoryAppearanceAction = m_settingsMenu->addAction(uiText(UiText::Key::CategoryAppearance));
    connect(m_categoryAppearanceAction, SIGNAL(triggered()), this, SLOT(showCategoryAppearanceDialog()));

    retranslateUi();
}

void MainWindow::retranslateUi() {
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

void MainWindow::showSettings() {
    showNormal();
    setAlwaysOnTop(true);
    revealFromTopAutoHide();
    raise();
    activateWindow();
#ifdef Q_OS_WIN
    forceWindowForeground(this);
#endif
}

QString MainWindow::language() const {
    return UiText::normalizeLanguage(m_document.settings.language);
}

bool MainWindow::hotkeysEnabled() const {
    return m_document.settings.hotkeysEnabled;
}

void MainWindow::showSettingsMenu() {
    if (m_settingsMenu && m_settingsButton) {
        m_settingsMenu->popup(m_settingsButton->mapToGlobal(QPoint(0, m_settingsButton->height())));
    }
}

void MainWindow::onCategoryButtonClicked(int id) {
    if (id == static_cast<int>(LauncherCategory::Program)) {
        setCurrentCategory(LauncherCategory::Program);
    } else if (id == static_cast<int>(LauncherCategory::Folder)) {
        setCurrentCategory(LauncherCategory::Folder);
    } else if (id == static_cast<int>(LauncherCategory::Website)) {
        setCurrentCategory(LauncherCategory::Website);
    }
}

void MainWindow::setLanguageChinese() {
    setLanguage("zh-CN");
}

void MainWindow::setLanguageEnglish() {
    setLanguage("en-US");
}

void MainWindow::setThemeSystem() {
    setThemeMode("system");
}

void MainWindow::setThemeLight() {
    setThemeMode("light");
}

void MainWindow::setThemeDark() {
    setThemeMode("dark");
}

void MainWindow::setHotkeysPaused(bool paused) {
    applyHotkeysEnabled(!paused);
}

void MainWindow::applyHotkeysEnabled(bool enabled) {
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

void MainWindow::showSectionContextMenu(const QPoint& pos) {
    QWidget* header = qobject_cast<QWidget*>(sender());
    if (!header) {
        return;
    }
    showSectionMenu(header->property("sectionId").toString(), header->mapToGlobal(pos));
}

void MainWindow::showListContextMenu(const QPoint& pos) {
    Q_UNUSED(pos);
    QListWidget* list = ancestorListWidget(qobject_cast<QWidget*>(sender()));
    if (!list) {
        return;
    }
    showListMenu(list->property("sectionId").toString(), list, list->viewport()->mapFromGlobal(QCursor::pos()));
}

void MainWindow::runClickedRule(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    runRule(item->data(RuleIdRole).toString());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    hide();
    event->ignore();
}

void MainWindow::enablePointerTracking(QWidget* widget) {
    if (!widget) {
        return;
    }
    widget->setMouseTracking(true);
    for (QObject* child : widget->children()) {
        if (auto* childWidget = qobject_cast<QWidget*>(child)) {
            enablePointerTracking(childWidget);
        }
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove || event->type() == QEvent::Drop) {
        auto* widget = qobject_cast<QWidget*>(watched);
        auto* dropBaseEvent = static_cast<QDropEvent*>(event);
        QString sectionId = widget ? widget->property("sectionId").toString() : QString();
        if (widget && (sectionId.isEmpty() || sectionIndexById(sectionId) < 0)) {
            sectionId = sectionIdAtGlobalPosition(widget->mapToGlobal(eventPosition(dropBaseEvent)));
        }
        if (sectionId.isEmpty()) {
            sectionId = fallbackDropSectionId();
        }
        const int sectionIndex = sectionIndexById(sectionId);
        if (sectionIndex >= 0 && m_document.sections[sectionIndex].category != LauncherCategory::Website) {
            if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
                auto* dragEvent = static_cast<QDragMoveEvent*>(event);
                if (dragEvent->mimeData() && dragEvent->mimeData()->hasUrls()) {
                    dragEvent->setDropAction(Qt::CopyAction);
                    dragEvent->accept();
                    return true;
                }
            } else {
                auto* dropEvent = static_cast<QDropEvent*>(event);
                if (dropEvent->mimeData() && dropEvent->mimeData()->hasUrls()) {
                    addDroppedPathsToSection(sectionId, dropEvent->mimeData()->urls());
                    dropEvent->setDropAction(Qt::CopyAction);
                    dropEvent->accept();
                    return true;
                }
            }
        }
    }

    if (event->type() == QEvent::ContextMenu) {
        if (m_ignoreNextContextMenuEvent) {
            m_ignoreNextContextMenuEvent = false;
            resetPendingContextMenu();
            event->accept();
            return true;
        }
        QWidget* widget = qobject_cast<QWidget*>(watched);
        if (widget && widget->window() == this) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            event->accept();
            return true;
#else
            auto* contextEvent = static_cast<QContextMenuEvent*>(event);
            const QPoint globalPos = contextEvent->globalPos();

            QWidget* sectionHeader = widget;
            while (sectionHeader && sectionHeader->window() == this && sectionHeader->objectName() != "sectionHeader") {
                sectionHeader = sectionHeader->parentWidget();
            }
            if (sectionHeader && sectionHeader->objectName() == "sectionHeader") {
                showSectionMenu(sectionHeader->property("sectionId").toString(), globalPos);
                contextEvent->accept();
                return true;
            }

            if (QListWidget* list = ancestorListWidget(widget)) {
                const QPoint viewportPos = list->viewport()->mapFromGlobal(globalPos);
                showListMenu(list->property("sectionId").toString(), list, viewportPos);
                contextEvent->accept();
                return true;
            }

            QString sectionId = widget->property("sectionId").toString();
            if (sectionId.isEmpty()) {
                sectionId = sectionIdAtGlobalPosition(globalPos);
            }
            if (!sectionId.isEmpty()) {
                if (QListWidget* sectionList = m_sectionLists.value(sectionId, nullptr)) {
                    showListMenu(sectionId, sectionList, sectionList->viewport()->mapFromGlobal(globalPos));
                } else {
                    showSectionMenu(sectionId, globalPos);
                }
                contextEvent->accept();
                return true;
            }
#endif
        }
    }

    if (!isVisible()) {
        return QMainWindow::eventFilter(watched, event);
    }
    if (m_topAutoHidden) {
        return QMainWindow::eventFilter(watched, event);
    }
    if (event->type() != QEvent::MouseMove && event->type() != QEvent::MouseButtonPress &&
        event->type() != QEvent::MouseButtonDblClick && event->type() != QEvent::MouseButtonRelease) {
        return QMainWindow::eventFilter(watched, event);
    }

    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget || widget->window() != this) {
        return QMainWindow::eventFilter(watched, event);
    }

    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    const QPoint localPos = mapFromGlobal(eventGlobalPosition(mouseEvent));

    if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::RightButton) {
        m_ignoreNextContextMenuEvent = false;
        m_pendingContextMenuKind = PendingContextMenuKind::None;
        m_pendingContextList = nullptr;
        m_pendingContextSectionId.clear();
        m_pendingContextViewportPos = QPoint();
        m_pendingContextGlobalPos = eventGlobalPosition(mouseEvent);

        QWidget* sectionHeader = widget;
        while (sectionHeader && sectionHeader->window() == this && sectionHeader->objectName() != "sectionHeader") {
            sectionHeader = sectionHeader->parentWidget();
        }
        if (sectionHeader && sectionHeader->objectName() == "sectionHeader") {
            m_pendingContextMenuKind = PendingContextMenuKind::Section;
            m_pendingContextSectionId = sectionHeader->property("sectionId").toString();
            return true;
        }

        if (QListWidget* list = ancestorListWidget(widget)) {
            m_pendingContextMenuKind = PendingContextMenuKind::List;
            m_pendingContextList = list;
            m_pendingContextSectionId = list->property("sectionId").toString();
            m_pendingContextViewportPos = list->viewport()->mapFromGlobal(m_pendingContextGlobalPos);
            return true;
        }

        QString sectionId = widget->property("sectionId").toString();
        if (sectionId.isEmpty()) {
            sectionId = sectionIdAtGlobalPosition(m_pendingContextGlobalPos);
        }
        if (!sectionId.isEmpty()) {
            if (QListWidget* sectionList = m_sectionLists.value(sectionId, nullptr)) {
                m_pendingContextMenuKind = PendingContextMenuKind::List;
                m_pendingContextList = sectionList;
                m_pendingContextSectionId = sectionId;
                m_pendingContextViewportPos = sectionList->viewport()->mapFromGlobal(m_pendingContextGlobalPos);
            } else {
                m_pendingContextMenuKind = PendingContextMenuKind::Section;
                m_pendingContextSectionId = sectionId;
            }
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease && m_pendingContextMenuKind != PendingContextMenuKind::None) {
        if (m_pendingContextMenuKind == PendingContextMenuKind::Section) {
            const QString sectionId = m_pendingContextSectionId;
            const QPoint globalPos = m_pendingContextGlobalPos;
            resetPendingContextMenu();
            m_ignoreNextContextMenuEvent = true;
            showSectionMenu(sectionId, globalPos);
            return true;
        }
        if (m_pendingContextMenuKind == PendingContextMenuKind::List && m_pendingContextList) {
            QListWidget* list = m_pendingContextList.data();
            const QString sectionId = m_pendingContextSectionId;
            const QPoint viewportPos = m_pendingContextViewportPos;
            resetPendingContextMenu();
            m_ignoreNextContextMenuEvent = true;
            showListMenu(sectionId, list, viewportPos);
            return true;
        }
        resetPendingContextMenu();
    }

    if (event->type() == QEvent::MouseButtonRelease && eventButton(mouseEvent) == Qt::RightButton) {
        resetPendingContextMenu();
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease && mouseEvent->button() == Qt::LeftButton) {
        if (QListWidget* list = ancestorListWidget(widget)) {
            const QPoint viewportPos = list->viewport()->mapFromGlobal(eventGlobalPosition(mouseEvent));
            if (QListWidgetItem* item = list->itemAt(viewportPos)) {
                runRule(item->data(RuleIdRole).toString());
                return true;
            }
        }
    }

    if (event->type() == QEvent::MouseMove) {
        if (m_resizing) {
            performResize(eventGlobalPosition(mouseEvent));
            return true;
        }
        if ((mouseEvent->buttons() & Qt::LeftButton) && !m_dragPosition.isNull()) {
            if (m_topAutoHidden) {
                revealFromTopAutoHide();
                m_dragPosition = eventGlobalPosition(mouseEvent) - frameGeometry().topLeft();
            }
            move(eventGlobalPosition(mouseEvent) - m_dragPosition);
            return true;
        }
        updateResizeCursor(localPos);
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
        m_resizeRegion = resizeRegionAt(localPos);
        if (m_resizeRegion != ResizeRegion::None) {
            m_resizing = true;
            m_resizeStartGlobal = eventGlobalPosition(mouseEvent);
            m_resizeStartGeometry = geometry();
            return true;
        }
        const bool draggableWidget = !qobject_cast<QAbstractButton*>(widget) && !qobject_cast<QLineEdit*>(widget);
        if (localPos.y() <= QtCompat::scaleInt(HeaderHeight) && draggableWidget) {
            m_dragPosition = eventGlobalPosition(mouseEvent) - frameGeometry().topLeft();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonDblClick && mouseEvent->button() == Qt::LeftButton) {
        QWidget* sectionHeader = widget;
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
        QWidget* sectionHeader = widget;
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

#ifdef Q_OS_WIN
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
bool MainWindow::winEvent(MSG* msg, long* result)
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result)
#else
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
#endif
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_UNUSED(eventType);
    auto* msg = static_cast<MSG*>(message);
#endif
    if (!msg || msg->message != WM_DROPFILES) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        return QMainWindow::winEvent(msg, result);
#else
        return QMainWindow::nativeEvent(eventType, message, result);
#endif
    }

    HDROP drop = reinterpret_cast<HDROP>(msg->wParam);
    if (!drop) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        return QMainWindow::winEvent(msg, result);
#else
        return QMainWindow::nativeEvent(eventType, message, result);
#endif
    }

    POINT clientPoint = {};
    DragQueryPoint(drop, &clientPoint);
    POINT screenPoint = clientPoint;
    ClientToScreen(msg->hwnd, &screenPoint);
    QString sectionId = sectionIdAtGlobalPosition(QPoint(screenPoint.x, screenPoint.y));
    if (sectionId.isEmpty()) {
        sectionId = fallbackDropSectionId();
    }

    QList<QUrl> urls;
    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        const UINT length = DragQueryFileW(drop, i, nullptr, 0);
        if (length == 0) {
            continue;
        }
        std::wstring buffer(length + 1, L'\0');
        if (DragQueryFileW(drop, i, &buffer[0], static_cast<UINT>(buffer.size())) > 0) {
            urls.push_back(QUrl::fromLocalFile(fromWideString(buffer.c_str(), static_cast<int>(length))));
        }
    }
    DragFinish(drop);

    const int sectionIndex = sectionIndexById(sectionId);
    if (sectionIndex >= 0 && m_document.sections[sectionIndex].category != LauncherCategory::Website &&
        !urls.isEmpty()) {
        addDroppedPathsToSection(sectionId, urls);
        if (result) {
            *result = 0;
        }
        return true;
    }

    if (result) {
        *result = 0;
    }
    return true;
}
#endif

void MainWindow::mousePressEvent(QMouseEvent* event) {
    if (m_topAutoHidden) {
        revealFromTopAutoHide();
    }
    if (event->button() == Qt::LeftButton) {
        m_resizeRegion = resizeRegionAt(eventPosition(event));
        if (m_resizeRegion != ResizeRegion::None) {
            m_resizing = true;
            m_resizeStartGlobal = eventGlobalPosition(event);
            m_resizeStartGeometry = geometry();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton && eventPosition(event).y() <= QtCompat::scaleInt(HeaderHeight)) {
        m_dragPosition = eventGlobalPosition(event) - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent* event) {
    if (m_topAutoHidden) {
        QMainWindow::mouseMoveEvent(event);
        return;
    }
    if (m_resizing) {
        performResize(eventGlobalPosition(event));
        event->accept();
        return;
    }
    if ((event->buttons() & Qt::LeftButton) && !m_dragPosition.isNull()) {
        if (m_topAutoHidden) {
            revealFromTopAutoHide();
            m_dragPosition = eventGlobalPosition(event) - frameGeometry().topLeft();
        }
        move(eventGlobalPosition(event) - m_dragPosition);
        event->accept();
        return;
    }
    updateResizeCursor(eventPosition(event));
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event) {
    const bool wasDraggingWindow = !m_dragPosition.isNull();
    m_resizing = false;
    m_resizeRegion = ResizeRegion::None;
    m_dragPosition = {};
    updateResizeCursor(eventPosition(event));
    if (wasDraggingWindow) {
        finishInteractiveMove();
        event->accept();
        return;
    }
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    rebuildNavItems();
    updateLauncherGrids();
}

void MainWindow::hideEvent(QHideEvent* event) {
    if (m_topAutoHidden) {
        m_topAutoHidden = false;
        setMinimumHeight(QtCompat::scaleInt(WindowMinimumHeight));
        setMaximumHeight(QWIDGETSIZE_MAX);
        if (m_autoHideShownGeometry.isValid()) {
            setGeometry(m_autoHideShownGeometry);
        }
    }
    m_autoHideTimer.stop();
    QMainWindow::hideEvent(event);
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    setAlwaysOnTop(true);
}

void MainWindow::leaveEvent(QEvent* event) {
    if (!m_resizing) {
        unsetCursor();
    }
    QMainWindow::leaveEvent(event);
}

void MainWindow::onHotkeyTriggered(const HotkeyRule& rule) {
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

void MainWindow::loadDocument() {
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

void MainWindow::saveDocument() {
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

void MainWindow::saveDocumentSilently() {
    QString error;
    if (!m_store.saveDocument(m_document, &error)) {
        showWarning(this, language(), uiText(UiText::Key::SaveFailed), error);
        setStatus(error);
        return;
    }
    refreshHooks();
}

void MainWindow::refreshHooks() {
    m_hookService.setRules(m_document.rules);
    m_hookService.setPaused(!m_document.settings.hotkeysEnabled);
}

void MainWindow::refreshLauncher() {
    rebuildSections();

    const int enabledCount = std::count_if(m_document.rules.begin(), m_document.rules.end(),
                                           [](const HotkeyRule& rule) { return rule.enabled; });
    if (m_ruleCountLabel) {
        m_ruleCountLabel->setText(uiText(UiText::Key::EnabledCount).arg(enabledCount).arg(m_document.rules.size()));
    }
}

void MainWindow::rebuildSections() {
    if (!m_sectionsLayout) {
        return;
    }

    while (QLayoutItem* item = m_sectionsLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_sectionLists.clear();

    QVector<LauncherSection> sections;
    for (const LauncherSection& section : m_document.sections) {
        if (section.category == m_currentCategory) {
            sections.push_back(section);
        }
    }
    std::sort(sections.begin(), sections.end(), [](const LauncherSection& left, const LauncherSection& right) {
        if (left.sortOrder == right.sortOrder) {
            return QString::localeAwareCompare(left.name, right.name) < 0;
        }
        return left.sortOrder < right.sortOrder;
    });

    QString openSectionId;
    for (const LauncherSection& section : sections) {
        if (!section.collapsed) {
            openSectionId = section.id;
            break;
        }
    }
    if (openSectionId.isEmpty() && !sections.isEmpty()) {
        openSectionId = sections.first().id;
    }

    bool normalizedCollapsedState = false;
    for (LauncherSection& section : m_document.sections) {
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
        for (LauncherSection& section : sections) {
            section.collapsed = section.id != openSectionId;
        }
    }

    for (const LauncherSection& section : sections) {
        auto* sectionFrame = new QFrame(m_sectionsContainer);
        sectionFrame->setObjectName("sectionFrame");
        sectionFrame->setAcceptDrops(true);
        sectionFrame->setProperty("sectionId", section.id);
        auto* sectionLayout = new QVBoxLayout(sectionFrame);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(0);

        auto* header = new QFrame(sectionFrame);
        header->setObjectName("sectionHeader");
        header->setAcceptDrops(true);
        header->setContextMenuPolicy(Qt::CustomContextMenu);
        header->setProperty("sectionId", section.id);
        header->setProperty("collapsed", section.collapsed);
        const LauncherSectionAppearance sectionAppearance = scaledAppearance(m_document.settings.sectionAppearance);
        header->setFixedHeight(sectionAppearance.headerHeight);
        auto* headerLayout = new QHBoxLayout(header);
        const int headerVerticalMargin =
            qMax(2, (sectionAppearance.headerHeight -
                     qMax(sectionAppearance.iconHeight, sectionAppearance.fontPointSize + 8)) /
                        2);
        headerLayout->setContentsMargins(8, headerVerticalMargin, 8, headerVerticalMargin);

        auto* iconLabel = new QLabel(header);
        iconLabel->setPixmap(iconForSection(section).pixmap(sectionAppearance.iconWidth, sectionAppearance.iconHeight));
        iconLabel->setFixedSize(sectionAppearance.iconWidth, sectionAppearance.iconHeight);
        iconLabel->setScaledContents(false);
        iconLabel->setAlignment(Qt::AlignCenter);
        auto* title = new QLabel(sectionDisplayName(language(), section), header);
        title->setObjectName("sectionTitle");
        applyTextAppearanceFont(title, sectionAppearance.fontFamily, sectionAppearance.fontPointSize, QFont::DemiBold);
        title->setStyleSheet(textAppearanceStyleSheet(sectionAppearance.fontFamily, sectionAppearance.fontPointSize,
                                                      sectionAppearance.textColor, 700));
        const int itemCount =
            std::count_if(m_document.rules.begin(), m_document.rules.end(),
                          [&section](const HotkeyRule& rule) { return rule.sectionId == section.id; });
        auto* meta = new QLabel(section.encrypted ? uiText(UiText::Key::EncryptedItemCount).arg(itemCount)
                                                  : uiText(UiText::Key::ItemCount).arg(itemCount),
                                header);
        meta->setObjectName("sectionMeta");
        auto* toggle = new QLabel(section.collapsed ? QString("+") : QString("-"), header);
        toggle->setObjectName("sectionToggle");
        toggle->setFixedWidth(16);
        toggle->setAlignment(Qt::AlignCenter);

        headerLayout->addWidget(iconLabel);
        headerLayout->addWidget(title, 1);
        headerLayout->addWidget(meta);
        headerLayout->addWidget(toggle);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        connect(header, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showSectionContextMenu(QPoint)));
#endif

        sectionLayout->addWidget(header);

        auto* body = new QWidget(sectionFrame);
        body->setAcceptDrops(true);
        body->setProperty("sectionId", section.id);
        auto* bodyLayout = new QVBoxLayout(body);
        bodyLayout->setContentsMargins(0, 0, 0, 0);
        bodyLayout->setSpacing(0);
        body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        if (!section.collapsed) {
            if (!isSectionUnlocked(section)) {
                auto* lockedHint = new QLabel(uiText(UiText::Key::LockedHint), body);
                lockedHint->setObjectName("lockedHint");
                lockedHint->setAlignment(Qt::AlignCenter);
                lockedHint->setMinimumHeight(72);
                bodyLayout->addWidget(lockedHint);
            } else {
                auto* list = new QListWidget(body);
                list->setObjectName("ruleGrid");
                list->setMovement(QListView::Static);
                list->setResizeMode(QListView::Adjust);
                list->setSelectionMode(QAbstractItemView::SingleSelection);
                list->setSpacing(0);
                list->setWordWrap(m_document.settings.itemAppearance.multilineText);
                list->setUniformItemSizes(true);
                list->setAcceptDrops(true);
                list->setDragDropMode(QAbstractItemView::DropOnly);
                list->setDefaultDropAction(Qt::CopyAction);
                list->setDropIndicatorShown(true);
                list->installEventFilter(this);
                list->viewport()->setAcceptDrops(true);
                list->viewport()->installEventFilter(this);
                list->setContextMenuPolicy(Qt::NoContextMenu);
                list->viewport()->setContextMenuPolicy(Qt::NoContextMenu);
                list->setProperty("sectionId", section.id);
                list->viewport()->setProperty("sectionId", section.id);
                list->setTextElideMode(m_document.settings.itemAppearance.showEllipsis ? Qt::ElideRight
                                                                                       : Qt::ElideNone);
                list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                list->setItemDelegate(new IconGridDelegate(scaledAppearance(m_document.settings.itemAppearance), list));
                QFont itemFont = list->font();
                if (!m_document.settings.itemAppearance.fontFamily.isEmpty()) {
                    itemFont.setFamily(m_document.settings.itemAppearance.fontFamily);
                }
                itemFont.setPointSize(scaledAppearance(m_document.settings.itemAppearance).fontPointSize);
                list->setFont(itemFont);

                for (const HotkeyRule& rule : m_document.rules) {
                    if (rule.sectionId != section.id || !rulePassesFilters(rule)) {
                        continue;
                    }
                    QString text = ruleTitle(rule);
                    auto* item = new QListWidgetItem(iconForRule(rule), text);
                    const LauncherItemAppearance itemAppearance = scaledAppearance(m_document.settings.itemAppearance);
                    item->setSizeHint(QSize(itemAppearance.itemWidth, itemAppearance.itemHeight));
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
                    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
#endif
                    const QString hotkeyTip =
                        rule.hotkey.isValid() ? rule.hotkey.displayText() : uiText(UiText::Key::UnboundHotkey);
                    item->setToolTip(QString("%1\n%2\n%3").arg(ruleTitle(rule), rule.action.target, hotkeyTip));
                    item->setData(RuleIdRole, rule.id);
                    if (!rule.enabled) {
                        item->setForeground(QColor("#8a94a3"));
                    }
                    list->addItem(item);
                }

                bodyLayout->addWidget(list, 1);
                m_sectionLists.insert(section.id, list);
            }
        }

        body->setVisible(!section.collapsed);
        sectionLayout->addWidget(body, section.collapsed ? 0 : 1);

        sectionFrame->setSizePolicy(QSizePolicy::Expanding,
                                    section.collapsed ? QSizePolicy::Fixed : QSizePolicy::Expanding);
        m_sectionsLayout->addWidget(sectionFrame, section.collapsed ? 0 : 1);
    }

    enablePointerTracking(m_sectionsContainer);
    updateLauncherGrids();
    QTimer::singleShot(0, this, SLOT(updateLauncherGrids()));
}

void MainWindow::updateLauncherGrids() {
    const LauncherItemAppearance appearance = scaledAppearance(m_document.settings.itemAppearance);
    const QSize iconSize(appearance.iconWidth, appearance.iconHeight);
    const QSize gridSize(appearance.itemWidth + appearance.horizontalSpacing,
                         appearance.itemHeight + appearance.verticalSpacing);

    for (QListWidget* list : m_sectionLists) {
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
            if (QListWidgetItem* item = list->item(row)) {
                item->setSizeHint(QSize(appearance.itemWidth, appearance.itemHeight));
            }
        }
        QFont itemFont = list->font();
        if (!appearance.fontFamily.isEmpty()) {
            itemFont.setFamily(appearance.fontFamily);
        }
        itemFont.setPointSize(appearance.fontPointSize);
        list->setFont(itemFont);
        auto* oldDelegate = dynamic_cast<IconGridDelegate*>(list->itemDelegate());
        list->setItemDelegate(new IconGridDelegate(appearance, list));
        if (oldDelegate) {
            oldDelegate->deleteLater();
        }
        list->setMinimumHeight(gridSize.height() + 4);
        list->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}

void MainWindow::setCurrentCategory(LauncherCategory category) {
    if (m_currentCategory == category) {
        return;
    }
    m_currentCategory = category;
    rebuildNavItems();
    refreshLauncher();
}

void MainWindow::setLanguage(const QString& language) {
    const QString normalized = UiText::normalizeLanguage(language);
    if (m_document.settings.language == normalized) {
        return;
    }
    m_document.settings.language = normalized;
    saveDocumentSilently();
    retranslateUi();
    emit languageChanged(normalized);
}

void MainWindow::setThemeMode(const QString& themeMode) {
    const QString normalized = themeMode.compare("light", Qt::CaseInsensitive) == 0  ? "light"
                               : themeMode.compare("dark", Qt::CaseInsensitive) == 0 ? "dark"
                                                                                     : "system";
    if (m_document.settings.themeMode == normalized) {
        return;
    }
    m_document.settings.themeMode = normalized;
    saveDocumentSilently();
    applyTheme();
}

void MainWindow::showHotkeyListDialog() {
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(uiText(UiText::Key::HotkeyList));
    dialog->setModal(false);

    auto* layout = new QVBoxLayout(dialog);
    auto* table = new QTableWidget(dialog);
    table->setColumnCount(4);
    QStringList headerLabels;
    headerLabels << uiText(UiText::Key::HotkeyListCategory) << uiText(UiText::Key::HotkeyListItem)
                 << uiText(UiText::Key::HotkeyListHotkey) << uiText(UiText::Key::HotkeyListTarget);
    table->setHorizontalHeaderLabels(headerLabels);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);

    for (const HotkeyRule& rule : m_document.rules) {
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
        auto* emptyLabel = new QLabel(uiText(UiText::Key::HotkeyListEmpty), dialog);
        emptyLabel->setObjectName("emptyHint");
        emptyLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(emptyLabel);
    }
    auto* buttons = makeDialogButtons(QDialogButtonBox::Close, dialog);
    connectDialogClose(buttons, dialog);
    layout->addWidget(buttons);

    dialog->resize(620, 420);
    dialog->show();
}

void MainWindow::showItemAppearanceDialog() {
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(uiText(UiText::Key::ItemAppearance));
    dialog->setModal(false);

    auto* layout = new QVBoxLayout(dialog);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(form);

    auto makeSpin = [dialog](int minimum, int maximum, int value, bool pixelSuffix = true) {
        auto* spin = new QSpinBox(dialog);
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        if (pixelSuffix) {
            spin->setSuffix(" px");
        }
        return spin;
    };

    const LauncherItemAppearance appearance = m_document.settings.itemAppearance;
    auto* iconWidth = makeSpin(16, 128, appearance.iconWidth);
    auto* iconHeight = makeSpin(16, 128, appearance.iconHeight);
    auto* itemWidth = makeSpin(40, 180, appearance.itemWidth);
    auto* itemHeight = makeSpin(44, 220, appearance.itemHeight);
    auto* fontFamily = new QFontComboBox(dialog);
    if (!appearance.fontFamily.isEmpty()) {
        fontFamily->setCurrentFont(QFont(appearance.fontFamily));
    }
    auto* fontPointSize = makeSpin(6, 18, appearance.fontPointSize, false);
    auto* horizontalSpacing = makeSpin(0, 40, appearance.horizontalSpacing);
    auto* verticalSpacing = makeSpin(0, 40, appearance.verticalSpacing);
    auto* multilineText = new QCheckBox(dialog);
    multilineText->setChecked(appearance.multilineText);
    auto* showEllipsis = new QCheckBox(dialog);
    showEllipsis->setChecked(appearance.showEllipsis);

    m_itemIconWidthSpin = iconWidth;
    m_itemIconHeightSpin = iconHeight;
    m_itemWidthSpin = itemWidth;
    m_itemHeightSpin = itemHeight;
    m_itemFontFamilyCombo = fontFamily;
    m_itemFontPointSizeSpin = fontPointSize;
    m_itemHorizontalSpacingSpin = horizontalSpacing;
    m_itemVerticalSpacingSpin = verticalSpacing;
    m_itemMultilineCheck = multilineText;
    m_itemEllipsisCheck = showEllipsis;

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

    auto* buttons = makeDialogButtons(QDialogButtonBox::Close, dialog);
    layout->addWidget(buttons);
    connectDialogClose(buttons, dialog);

    QList<QSpinBox*> itemSpins;
    itemSpins << iconWidth << iconHeight << itemWidth << itemHeight << fontPointSize << horizontalSpacing
              << verticalSpacing;
    for (QSpinBox* spin : itemSpins) {
        connect(spin, SIGNAL(valueChanged(int)), this, SLOT(applyItemAppearanceDialogChange()));
    }
    connect(fontFamily, SIGNAL(currentFontChanged(QFont)), this, SLOT(applyItemAppearanceDialogChange()));
    connect(multilineText, SIGNAL(toggled(bool)), this, SLOT(applyItemAppearanceDialogChange()));
    connect(showEllipsis, SIGNAL(toggled(bool)), this, SLOT(applyItemAppearanceDialogChange()));

    dialog->resize(320, dialog->sizeHint().height());
    dialog->show();
}

void MainWindow::showSectionAppearanceDialog() {
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(uiText(UiText::Key::SectionAppearance));
    dialog->setModal(false);

    auto* layout = new QVBoxLayout(dialog);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(form);

    auto makeSpin = [dialog](int minimum, int maximum, int value, bool pixelSuffix = true) {
        auto* spin = new QSpinBox(dialog);
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        if (pixelSuffix) {
            spin->setSuffix(" px");
        }
        return spin;
    };

    const LauncherSectionAppearance appearance = m_document.settings.sectionAppearance;
    auto* iconWidth = makeSpin(12, 96, appearance.iconWidth);
    auto* iconHeight = makeSpin(12, 96, appearance.iconHeight);
    auto* headerHeight = makeSpin(24, 96, appearance.headerHeight);
    auto* fontFamily = new QFontComboBox(dialog);
    if (!appearance.fontFamily.isEmpty()) {
        fontFamily->setCurrentFont(QFont(appearance.fontFamily));
    }
    auto* fontPointSize = makeSpin(6, 18, appearance.fontPointSize, false);
    auto* colorButton = new QPushButton(dialog);
    auto* defaultColorButton = new QPushButton(uiText(UiText::Key::DefaultColor), dialog);
    updateColorButton(colorButton, language(), appearance.textColor);
    colorButton->setProperty("selectedColor", appearance.textColor);

    m_sectionIconWidthSpin = iconWidth;
    m_sectionIconHeightSpin = iconHeight;
    m_sectionHeaderHeightSpin = headerHeight;
    m_sectionFontFamilyCombo = fontFamily;
    m_sectionFontPointSizeSpin = fontPointSize;
    m_sectionColorButton = colorButton;

    auto* colorLayout = new QHBoxLayout;
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->addWidget(colorButton, 1);
    colorLayout->addWidget(defaultColorButton);
    auto* colorWidget = new QWidget(dialog);
    colorWidget->setLayout(colorLayout);

    form->addRow(uiText(UiText::Key::IconWidth), iconWidth);
    form->addRow(uiText(UiText::Key::IconHeight), iconHeight);
    form->addRow(uiText(UiText::Key::SectionHeight), headerHeight);
    form->addRow(uiText(UiText::Key::FontFamily), fontFamily);
    form->addRow(uiText(UiText::Key::FontPointSize), fontPointSize);
    form->addRow(uiText(UiText::Key::TextColor), colorWidget);

    auto* buttons = makeDialogButtons(QDialogButtonBox::Close, dialog);
    layout->addWidget(buttons);
    connectDialogClose(buttons, dialog);

    QList<QSpinBox*> sectionSpins;
    sectionSpins << iconWidth << iconHeight << headerHeight << fontPointSize;
    for (QSpinBox* spin : sectionSpins) {
        connect(spin, SIGNAL(valueChanged(int)), this, SLOT(applySectionAppearanceDialogChange()));
    }
    connect(fontFamily, SIGNAL(currentFontChanged(QFont)), this, SLOT(applySectionAppearanceDialogChange()));
    connect(colorButton, SIGNAL(clicked()), this, SLOT(chooseSectionTextColor()));
    connect(defaultColorButton, SIGNAL(clicked()), this, SLOT(resetSectionTextColor()));

    dialog->resize(320, dialog->sizeHint().height());
    dialog->show();
}

void MainWindow::showCategoryAppearanceDialog() {
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(uiText(UiText::Key::CategoryAppearance));
    dialog->setModal(false);

    auto* layout = new QVBoxLayout(dialog);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(form);

    auto makeSpin = [dialog](int minimum, int maximum, int value, bool pixelSuffix = true) {
        auto* spin = new QSpinBox(dialog);
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        if (pixelSuffix) {
            spin->setSuffix(" px");
        }
        return spin;
    };

    const LauncherCategoryAppearance appearance = m_document.settings.categoryAppearance;
    auto* iconWidth = makeSpin(12, 96, appearance.iconWidth);
    auto* iconHeight = makeSpin(12, 96, appearance.iconHeight);
    auto* buttonHeight = makeSpin(24, 96, appearance.buttonHeight);
    auto* fontFamily = new QFontComboBox(dialog);
    if (!appearance.fontFamily.isEmpty()) {
        fontFamily->setCurrentFont(QFont(appearance.fontFamily));
    }
    auto* fontPointSize = makeSpin(6, 18, appearance.fontPointSize, false);
    auto* colorButton = new QPushButton(dialog);
    auto* defaultColorButton = new QPushButton(uiText(UiText::Key::DefaultColor), dialog);
    updateColorButton(colorButton, language(), appearance.textColor);
    colorButton->setProperty("selectedColor", appearance.textColor);

    m_categoryIconWidthSpin = iconWidth;
    m_categoryIconHeightSpin = iconHeight;
    m_categoryButtonHeightSpin = buttonHeight;
    m_categoryFontFamilyCombo = fontFamily;
    m_categoryFontPointSizeSpin = fontPointSize;
    m_categoryColorButton = colorButton;

    auto* colorLayout = new QHBoxLayout;
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->addWidget(colorButton, 1);
    colorLayout->addWidget(defaultColorButton);
    auto* colorWidget = new QWidget(dialog);
    colorWidget->setLayout(colorLayout);

    form->addRow(uiText(UiText::Key::IconWidth), iconWidth);
    form->addRow(uiText(UiText::Key::IconHeight), iconHeight);
    form->addRow(uiText(UiText::Key::CategoryHeight), buttonHeight);
    form->addRow(uiText(UiText::Key::FontFamily), fontFamily);
    form->addRow(uiText(UiText::Key::FontPointSize), fontPointSize);
    form->addRow(uiText(UiText::Key::TextColor), colorWidget);

    auto* buttons = makeDialogButtons(QDialogButtonBox::Close, dialog);
    layout->addWidget(buttons);
    connectDialogClose(buttons, dialog);

    QList<QSpinBox*> categorySpins;
    categorySpins << iconWidth << iconHeight << buttonHeight << fontPointSize;
    for (QSpinBox* spin : categorySpins) {
        connect(spin, SIGNAL(valueChanged(int)), this, SLOT(applyCategoryAppearanceDialogChange()));
    }
    connect(fontFamily, SIGNAL(currentFontChanged(QFont)), this, SLOT(applyCategoryAppearanceDialogChange()));
    connect(colorButton, SIGNAL(clicked()), this, SLOT(chooseCategoryTextColor()));
    connect(defaultColorButton, SIGNAL(clicked()), this, SLOT(resetCategoryTextColor()));

    dialog->resize(320, dialog->sizeHint().height());
    dialog->show();
}

void MainWindow::applyItemAppearanceChange() {
    applyFixedLauncherWidth();
    saveDocumentSilently();
    refreshLauncher();
}

void MainWindow::applySectionAppearanceChange() {
    saveDocumentSilently();
    refreshLauncher();
}

void MainWindow::applyCategoryAppearanceChange() {
    saveDocumentSilently();
    applyCategoryAppearance();
    rebuildNavItems();
}

void MainWindow::applyItemAppearanceDialogChange() {
    if (!m_itemIconWidthSpin || !m_itemIconHeightSpin || !m_itemWidthSpin || !m_itemHeightSpin ||
        !m_itemFontFamilyCombo || !m_itemFontPointSizeSpin || !m_itemHorizontalSpacingSpin ||
        !m_itemVerticalSpacingSpin || !m_itemMultilineCheck || !m_itemEllipsisCheck) {
        return;
    }

    LauncherItemAppearance next;
    next.iconWidth = m_itemIconWidthSpin->value();
    next.iconHeight = m_itemIconHeightSpin->value();
    next.itemWidth = m_itemWidthSpin->value();
    next.itemHeight = m_itemHeightSpin->value();
    next.fontFamily = m_itemFontFamilyCombo->currentFont().family();
    next.fontPointSize = m_itemFontPointSizeSpin->value();
    next.horizontalSpacing = m_itemHorizontalSpacingSpin->value();
    next.verticalSpacing = m_itemVerticalSpacingSpin->value();
    next.multilineText = m_itemMultilineCheck->isChecked();
    next.showEllipsis = m_itemEllipsisCheck->isChecked();
    m_document.settings.itemAppearance = LauncherItemAppearance::fromJson(next.toJson());
    applyItemAppearanceChange();
}

void MainWindow::applySectionAppearanceDialogChange() {
    if (!m_sectionIconWidthSpin || !m_sectionIconHeightSpin || !m_sectionHeaderHeightSpin ||
        !m_sectionFontFamilyCombo || !m_sectionFontPointSizeSpin || !m_sectionColorButton) {
        return;
    }

    LauncherSectionAppearance next;
    next.iconWidth = m_sectionIconWidthSpin->value();
    next.iconHeight = m_sectionIconHeightSpin->value();
    next.headerHeight = m_sectionHeaderHeightSpin->value();
    next.fontFamily = m_sectionFontFamilyCombo->currentFont().family();
    next.fontPointSize = m_sectionFontPointSizeSpin->value();
    next.textColor = m_sectionColorButton->property("selectedColor").toString();
    m_document.settings.sectionAppearance = LauncherSectionAppearance::fromJson(next.toJson());
    applySectionAppearanceChange();
}

void MainWindow::applyCategoryAppearanceDialogChange() {
    if (!m_categoryIconWidthSpin || !m_categoryIconHeightSpin || !m_categoryButtonHeightSpin ||
        !m_categoryFontFamilyCombo || !m_categoryFontPointSizeSpin || !m_categoryColorButton) {
        return;
    }

    LauncherCategoryAppearance next;
    next.iconWidth = m_categoryIconWidthSpin->value();
    next.iconHeight = m_categoryIconHeightSpin->value();
    next.buttonHeight = m_categoryButtonHeightSpin->value();
    next.fontFamily = m_categoryFontFamilyCombo->currentFont().family();
    next.fontPointSize = m_categoryFontPointSizeSpin->value();
    next.textColor = m_categoryColorButton->property("selectedColor").toString();
    m_document.settings.categoryAppearance = LauncherCategoryAppearance::fromJson(next.toJson());
    applyCategoryAppearanceChange();
}

void MainWindow::chooseSectionTextColor() {
    if (!m_sectionColorButton) {
        return;
    }
    const QString currentColor = m_sectionColorButton->property("selectedColor").toString();
    const QColor initial = currentColor.isEmpty() ? palette().color(QPalette::Text) : QColor(currentColor);
    const QColor selected = QColorDialog::getColor(initial, this, uiText(UiText::Key::ChooseColor));
    if (!selected.isValid()) {
        return;
    }
    const QString color = selected.name();
    m_sectionColorButton->setProperty("selectedColor", color);
    updateColorButton(m_sectionColorButton, language(), color);
    applySectionAppearanceDialogChange();
}

void MainWindow::resetSectionTextColor() {
    if (!m_sectionColorButton) {
        return;
    }
    m_sectionColorButton->setProperty("selectedColor", QString());
    updateColorButton(m_sectionColorButton, language(), QString());
    applySectionAppearanceDialogChange();
}

void MainWindow::chooseCategoryTextColor() {
    if (!m_categoryColorButton) {
        return;
    }
    const QString currentColor = m_categoryColorButton->property("selectedColor").toString();
    const QColor initial = currentColor.isEmpty() ? palette().color(QPalette::Text) : QColor(currentColor);
    const QColor selected = QColorDialog::getColor(initial, this, uiText(UiText::Key::ChooseColor));
    if (!selected.isValid()) {
        return;
    }
    const QString color = selected.name();
    m_categoryColorButton->setProperty("selectedColor", color);
    updateColorButton(m_categoryColorButton, language(), color);
    applyCategoryAppearanceDialogChange();
}

void MainWindow::resetCategoryTextColor() {
    if (!m_categoryColorButton) {
        return;
    }
    m_categoryColorButton->setProperty("selectedColor", QString());
    updateColorButton(m_categoryColorButton, language(), QString());
    applyCategoryAppearanceDialogChange();
}

void MainWindow::applyCategoryAppearance() {
    const LauncherCategoryAppearance appearance = scaledAppearance(m_document.settings.categoryAppearance);
    for (QToolButton* button : m_navButtons) {
        if (!button) {
            continue;
        }
        button->setFixedHeight(appearance.buttonHeight);
        button->setIconSize(QSize(appearance.iconWidth, appearance.iconHeight));
        applyTextAppearanceFont(button, appearance.fontFamily, appearance.fontPointSize, QFont::DemiBold);
        button->setStyleSheet(
            textAppearanceStyleSheet(appearance.fontFamily, appearance.fontPointSize, appearance.textColor, 700));
    }
}

int MainWindow::fixedLauncherWidth() const {
    const LauncherItemAppearance appearance = scaledAppearance(m_document.settings.itemAppearance);
    const int cellWidth = appearance.itemWidth + appearance.horizontalSpacing;
    return FixedIconColumns * cellWidth + SectionHorizontalMargin * 2 + ScrollBarReserveWidth;
}

void MainWindow::applyFixedLauncherWidth() {
    const int width = fixedLauncherWidth();
    setMinimumSize(width, QtCompat::scaleInt(WindowMinimumHeight));
    setMaximumSize(width, QWIDGETSIZE_MAX);
    setMaximumWidth(width);
    if (this->width() != width) {
        resize(width, height());
    }
}

bool MainWindow::effectiveDarkTheme() const {
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

void MainWindow::applyTheme() {
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

void MainWindow::upsertRule(const HotkeyRule& rule) {
    const int index = ruleIndexById(rule.id);
    if (index >= 0) {
        m_document.rules[index] = rule;
    } else {
        m_document.rules.push_back(rule);
    }
    saveDocument();
}

void MainWindow::setStatus(const QString& message) {
    if (m_statusLabel) {
        m_statusLabel->setText(message);
    }
}

void MainWindow::showSectionMenu(const QString& sectionId, const QPoint& globalPos) {
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }

    QMenu menu(this);
    QAction* newAction = menu.addAction(uiText(UiText::Key::NewSection));
    QAction* editAction = menu.addAction(uiText(UiText::Key::EditSection));
    QAction* deleteAction = menu.addAction(uiText(UiText::Key::DeleteSection));
    QAction* encryptAction = menu.addAction(uiText(UiText::Key::EncryptSection));
    QAction* unlockAction = nullptr;
    if (m_document.sections[index].encrypted && !m_unlockedSectionIds.contains(sectionId)) {
        menu.addSeparator();
        unlockAction = menu.addAction(uiText(UiText::Key::UnlockSection));
    }
    QAction* selected = menu.exec(globalPos);
    if (selected == newAction) {
        addSection(m_currentCategory);
    } else if (selected == editAction) {
        editSection(sectionId);
    } else if (selected == deleteAction) {
        deleteSection(sectionId);
    } else if (selected == encryptAction) {
        encryptSection(sectionId);
    } else if (selected == unlockAction && unlockAction) {
        if (ensureSectionUnlocked(sectionId)) {
            refreshLauncher();
        }
    }
}

void MainWindow::showListMenu(const QString& sectionId, QListWidget* list, const QPoint& viewportPos) {
    if (sectionIndexById(sectionId) < 0) {
        return;
    }
    QListWidgetItem* item = list ? list->itemAt(viewportPos) : nullptr;
    QMenu menu(this);
    QAction* runAction = nullptr;
    QAction* runAsAdminAction = nullptr;
    QAction* explorerMenuAction = nullptr;
    QAction* browseAction = nullptr;
    QAction* desktopShortcutAction = nullptr;
    QAction* startupAction = nullptr;
    QAction* editAction = nullptr;
    QAction* deleteAction = nullptr;
    QAction* addAction = nullptr;
    QString ruleId;
    if (item) {
        ruleId = item->data(RuleIdRole).toString();
        runAction = menu.addAction(uiText(UiText::Key::Run));
        runAsAdminAction = menu.addAction(uiText(UiText::Key::RunAsAdmin));
        explorerMenuAction = menu.addAction(uiText(UiText::Key::ExplorerContextMenu));
        browseAction = menu.addAction(uiText(UiText::Key::BrowseTarget));
        desktopShortcutAction = menu.addAction(uiText(UiText::Key::CreateDesktopShortcut));
        startupAction = menu.addAction(uiText(UiText::Key::SetStartup));
        menu.addSeparator();
        editAction = menu.addAction(uiText(UiText::Key::Edit));
        deleteAction = menu.addAction(uiText(UiText::Key::Delete));
    } else {
        addAction = menu.addAction(uiText(UiText::Key::AddItem));
    }
    const QPoint globalPos = list ? list->viewport()->mapToGlobal(viewportPos) : QCursor::pos();
    QAction* selected = menu.exec(globalPos);
    if (!selected) {
        return;
    }
    if (selected == runAction) {
        runRule(ruleId);
    } else if (selected == runAsAdminAction) {
        runRuleAsAdmin(ruleId);
    } else if (selected == explorerMenuAction) {
        showExplorerContextMenuForRule(ruleId, globalPos);
    } else if (selected == browseAction) {
        browseRuleTarget(ruleId);
    } else if (selected == desktopShortcutAction) {
        createDesktopShortcutForRule(ruleId);
    } else if (selected == startupAction) {
        setRuleStartupShortcut(ruleId);
    } else if (selected == editAction) {
        editRule(ruleId);
    } else if (selected == deleteAction) {
        deleteRule(ruleId);
    } else if (addAction && selected == addAction) {
        addRuleToSection(sectionId);
    }
}

void MainWindow::addSection(LauncherCategory category) {
    int nextOrder = 0;
    for (const LauncherSection& section : m_document.sections) {
        if (section.category == category) {
            nextOrder = qMax(nextOrder, section.sortOrder + 1);
        }
    }

    QDialog dialog(this);
    dialog.setWindowTitle(uiText(UiText::Key::NewSectionTitle));
    auto* form = new QFormLayout;
    auto* nameEdit = new QLineEdit(&dialog);
    auto* iconEdit = new QLineEdit(LauncherSection::categoryName(category).toLower(), &dialog);
    auto* orderEdit = new QLineEdit(QString::number(nextOrder), &dialog);
    form->addRow(uiText(UiText::Key::Name), nameEdit);
    form->addRow(uiText(UiText::Key::IconKey), iconEdit);
    form->addRow(uiText(UiText::Key::SortOrder), orderEdit);

    auto* buttons = makeDialogButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    localizeDialogButtons(buttons, language());
    connectDialogAcceptReject(buttons, &dialog);

    auto* layout = new QVBoxLayout(&dialog);
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
    section.id = QtCompat::uuidWithoutBraces();
    section.category = category;
    section.name = name;
    section.iconKey = iconEdit->text().trimmed();
    section.sortOrder = ok ? order : nextOrder;
    m_document.sections.push_back(section);
    saveDocument();
}

void MainWindow::editSection(const QString& sectionId) {
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }

    LauncherSection& section = m_document.sections[index];
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(uiText(UiText::Key::EditSectionTitle));
    auto* form = new QFormLayout;
    auto* nameEdit = new QLineEdit(section.name, dialog);
    auto* iconEdit = new QLineEdit(section.iconKey, dialog);
    auto* orderEdit = new QLineEdit(QString::number(section.sortOrder), dialog);
    form->addRow(uiText(UiText::Key::Name), nameEdit);
    form->addRow(uiText(UiText::Key::IconKey), iconEdit);
    form->addRow(uiText(UiText::Key::SortOrder), orderEdit);

    auto* buttons = makeDialogButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    localizeDialogButtons(buttons, language());
    connectDialogAcceptReject(buttons, dialog);

    auto* layout = new QVBoxLayout(dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    if (dialog->exec() == QDialog::Accepted) {
        const QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            showWarning(this, language(), uiText(UiText::Key::InvalidSection),
                        uiText(UiText::Key::SectionNameRequired));
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

void MainWindow::deleteSection(const QString& sectionId) {
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }

    const LauncherSection section = m_document.sections[index];
    const int itemCount = std::count_if(m_document.rules.begin(), m_document.rules.end(),
                                        [&sectionId](const HotkeyRule& rule) { return rule.sectionId == sectionId; });
    const QString message =
        itemCount > 0
            ? uiText(UiText::Key::DeleteSectionWithItems).arg(sectionDisplayName(language(), section)).arg(itemCount)
            : uiText(UiText::Key::DeleteSectionConfirm).arg(sectionDisplayName(language(), section));
    if (!confirm(this, language(), uiText(UiText::Key::DeleteSectionTitle), message)) {
        return;
    }

    m_document.sections.remove(index);
    m_document.rules.erase(std::remove_if(m_document.rules.begin(), m_document.rules.end(),
                                          [&sectionId](const HotkeyRule& rule) { return rule.sectionId == sectionId; }),
                           m_document.rules.end());
    m_unlockedSectionIds.remove(sectionId);
    saveDocument();
}

void MainWindow::encryptSection(const QString& sectionId) {
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }

    LauncherSection& section = m_document.sections[index];
    if (section.encrypted && !ensureSectionUnlocked(sectionId)) {
        return;
    }

    bool ok = false;
    const QString password = passwordInput(this, language(), uiText(UiText::Key::EncryptSectionTitle),
                                           uiText(UiText::Key::EncryptSectionPrompt), &ok);
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

void MainWindow::expandSectionOnly(const QString& sectionId) {
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }

    const LauncherCategory category = m_document.sections[index].category;
    bool changed = false;
    for (LauncherSection& section : m_document.sections) {
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

void MainWindow::toggleSectionCollapsed(const QString& sectionId) {
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return;
    }
    m_document.sections[index].collapsed = !m_document.sections[index].collapsed;
    saveDocument();
    rebuildSections();
}

void MainWindow::addRuleToSection(const QString& sectionId) {
    const int sectionIndex = sectionIndexById(sectionId);
    if (sectionIndex < 0) {
        return;
    }
    const LauncherSection& section = m_document.sections[sectionIndex];
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

void MainWindow::addDroppedPathsToSection(const QString& sectionId, const QList<QUrl>& urls) {
    const int sectionIndex = sectionIndexById(sectionId);
    if (sectionIndex < 0 || urls.isEmpty()) {
        return;
    }
    const LauncherSection section = m_document.sections[sectionIndex];
    if (!ensureSectionUnlocked(sectionId)) {
        return;
    }

    int addedCount = 0;
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QFileInfo info(url.toLocalFile());
        if (!info.exists()) {
            continue;
        }

        HotkeyRule rule;
        rule.id = QtCompat::uuidWithoutBraces();
        rule.enabled = true;
        rule.category = section.category;
        rule.sectionId = section.id;
        rule.action.target = info.absoluteFilePath();
        rule.description = info.isDir() ? info.fileName() : info.completeBaseName();

        if (section.category == LauncherCategory::Website ||
            (section.category == LauncherCategory::Program && info.isDir())) {
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

QString MainWindow::sectionIdAtGlobalPosition(const QPoint& globalPos) const {
    QWidget* widget = QApplication::widgetAt(globalPos);
    while (widget) {
        const QString sectionId = widget->property("sectionId").toString();
        if (!sectionId.isEmpty() && sectionIndexById(sectionId) >= 0) {
            return sectionId;
        }
        widget = widget->parentWidget();
    }

    for (auto it = m_sectionLists.constBegin(); it != m_sectionLists.constEnd(); ++it) {
        QListWidget* list = it.value();
        if (!list || !list->isVisible()) {
            continue;
        }
        const QRect listRect(list->mapToGlobal(QPoint(0, 0)), list->size());
        const QRect viewportRect(list->viewport()->mapToGlobal(QPoint(0, 0)), list->viewport()->size());
        if (listRect.contains(globalPos) || viewportRect.contains(globalPos)) {
            return it.key();
        }
    }

    QWidget* container = m_sectionsContainer;
    while (container) {
        const QList<QWidget*> allWidgets = container->findChildren<QWidget*>();
        QList<QWidget*> sectionFrames;
        for (QWidget* child : allWidgets) {
            if (child && child->parentWidget() == container) {
                sectionFrames << child;
            }
        }
        for (QWidget* sectionFrame : sectionFrames) {
            const QString sectionId = sectionFrame->property("sectionId").toString();
            if (sectionId.isEmpty() || sectionIndexById(sectionId) < 0 || !sectionFrame->isVisible()) {
                continue;
            }
            const QRect frameRect(sectionFrame->mapToGlobal(QPoint(0, 0)), sectionFrame->size());
            if (frameRect.contains(globalPos)) {
                return sectionId;
            }
        }
        break;
    }

    return {};
}

QString MainWindow::fallbackDropSectionId() const {
    QString firstSectionId;
    for (const LauncherSection& section : m_document.sections) {
        if (section.category != m_currentCategory) {
            continue;
        }
        if (firstSectionId.isEmpty()) {
            firstSectionId = section.id;
        }
        if (!section.collapsed) {
            return section.id;
        }
    }
    return firstSectionId;
}

void MainWindow::editRule(const QString& ruleId) {
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

void MainWindow::deleteRule(const QString& ruleId) {
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    if (!ensureSectionUnlocked(m_document.rules[index].sectionId)) {
        return;
    }

    if (!confirm(this, language(), uiText(UiText::Key::DeleteItemTitle),
                 uiText(UiText::Key::DeleteItemConfirm).arg(ruleTitle(m_document.rules[index])))) {
        return;
    }
    m_document.rules.remove(index);
    saveDocument();
}

void MainWindow::runRule(const QString& ruleId) {
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    if (!ensureSectionUnlocked(m_document.rules[index].sectionId)) {
        return;
    }
    onHotkeyTriggered(m_document.rules[index]);
}

void MainWindow::runRuleAsAdmin(const QString& ruleId) {
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    if (!ensureSectionUnlocked(m_document.rules[index].sectionId)) {
        return;
    }

#ifdef Q_OS_WIN
    const HotkeyRule& rule = m_document.rules[index];
    if (!shellExecutePath(rule.action.target, L"runas", rule.action.arguments, rule.action.workingDirectory)) {
        setStatus(uiText(UiText::Key::LaunchFailed).arg(QString::number(GetLastError())));
    }
#else
    runRule(ruleId);
#endif
}

void MainWindow::showExplorerContextMenuForRule(const QString& ruleId, const QPoint& globalPos) {
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    if (!ensureSectionUnlocked(m_document.rules[index].sectionId)) {
        return;
    }

#ifdef Q_OS_WIN
    showShellContextMenu(this, m_document.rules[index].action.target, globalPos);
#else
    Q_UNUSED(globalPos)
#endif
}

void MainWindow::browseRuleTarget(const QString& ruleId) {
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    if (!ensureSectionUnlocked(m_document.rules[index].sectionId)) {
        return;
    }

#ifdef Q_OS_WIN
    revealInExplorer(m_document.rules[index].action.target);
#endif
}

void MainWindow::createDesktopShortcutForRule(const QString& ruleId) {
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    if (!ensureSectionUnlocked(m_document.rules[index].sectionId)) {
        return;
    }

#ifdef Q_OS_WIN
    const HotkeyRule& rule = m_document.rules[index];
    const QString shortcutName = QtCompat::sanitizeFileName(QString("%1.lnk").arg(ruleTitle(rule)));
    createShortcutFile(QDir(desktopDirectoryPath()).filePath(shortcutName), rule, ruleTitle(rule));
#endif
}

void MainWindow::setRuleStartupShortcut(const QString& ruleId) {
    const int index = ruleIndexById(ruleId);
    if (index < 0) {
        return;
    }
    if (!ensureSectionUnlocked(m_document.rules[index].sectionId)) {
        return;
    }

#ifdef Q_OS_WIN
    const HotkeyRule& rule = m_document.rules[index];
    const QString shortcutName = QtCompat::sanitizeFileName(QString("HStart-%1.lnk").arg(ruleTitle(rule)));
    createShortcutFile(QDir(startupDirectoryPath()).filePath(shortcutName), rule, ruleTitle(rule));
#endif
}

void MainWindow::resetPendingContextMenu() {
    m_pendingContextMenuKind = PendingContextMenuKind::None;
    m_pendingContextList = nullptr;
    m_pendingContextSectionId.clear();
    m_pendingContextViewportPos = QPoint();
    m_pendingContextGlobalPos = QPoint();
}

bool MainWindow::ensureSectionUnlocked(const QString& sectionId) {
    const int index = sectionIndexById(sectionId);
    if (index < 0) {
        return false;
    }
    const LauncherSection& section = m_document.sections[index];
    if (isSectionUnlocked(section)) {
        return true;
    }

    bool ok = false;
    const QString password = passwordInput(this, language(), uiText(UiText::Key::UnlockSectionTitle),
                                           uiText(UiText::Key::UnlockSectionPrompt), &ok);
    if (!ok) {
        return false;
    }
    if (passwordHash(password) != section.passwordHash) {
        showWarning(this, language(), uiText(UiText::Key::WrongPasswordTitle),
                    uiText(UiText::Key::WrongPasswordMessage));
        return false;
    }
    m_unlockedSectionIds.insert(sectionId);
    return true;
}

bool MainWindow::isSectionUnlocked(const LauncherSection& section) const {
    return !section.encrypted || m_unlockedSectionIds.contains(section.id);
}

int MainWindow::sectionIndexById(const QString& sectionId) const {
    for (int i = 0; i < m_document.sections.size(); ++i) {
        if (m_document.sections[i].id == sectionId) {
            return i;
        }
    }
    return -1;
}

int MainWindow::ruleIndexById(const QString& ruleId) const {
    for (int i = 0; i < m_document.rules.size(); ++i) {
        if (m_document.rules[i].id == ruleId) {
            return i;
        }
    }
    return -1;
}

QIcon MainWindow::iconForRule(const HotkeyRule& rule) const {
    switch (rule.category) {
    case LauncherCategory::Program: {
        const QString target = QFileInfo(rule.action.target).fileName().toLower();
        const QString title = rule.description.toLower();
        const auto nativeOrFallback = [this](const QString& path, QStyle::StandardPixmap fallback) {
#ifdef Q_OS_WIN
            const QIcon native = nativeFileIcon(path);
            if (!native.isNull()) {
                return native;
            }
#else
            Q_UNUSED(path)
#endif
            return style()->standardIcon(fallback);
        };
        if (target == "control.exe" || title.contains(QString::fromUtf8("控制面板")) || title.contains("control")) {
            return nativeOrFallback("control.exe", QStyle::SP_ComputerIcon);
        }
        if (target == "taskmgr.exe" || title.contains(QString::fromUtf8("任务管理器")) || title.contains("task")) {
            return nativeOrFallback("taskmgr.exe", QStyle::SP_FileDialogDetailedView);
        }
        if (target == "cmd.exe" || target == "powershell.exe" || title.contains(QString::fromUtf8("命令")) ||
            title.contains("terminal")) {
            return nativeOrFallback("cmd.exe", QStyle::SP_ComputerIcon);
        }
        if (target == "regedit.exe" || title.contains(QString::fromUtf8("注册表")) || title.contains("registry")) {
            return nativeOrFallback("regedit.exe", QStyle::SP_FileIcon);
        }
        if (target == "services.msc" || title.contains(QString::fromUtf8("服务")) || title.contains("services")) {
            return nativeOrFallback("services.msc", QStyle::SP_FileDialogInfoView);
        }
        if (target == "devmgmt.msc" || title.contains(QString::fromUtf8("设备")) || title.contains("device")) {
            return nativeOrFallback("devmgmt.msc", QStyle::SP_ComputerIcon);
        }
        if (target == "calc.exe" || title.contains(QString::fromUtf8("计算器")) || title.contains("calculator")) {
            return nativeOrFallback("calc.exe", QStyle::SP_FileIcon);
        }
        if (target == "msinfo32.exe" || title.contains(QString::fromUtf8("系统信息")) ||
            title.contains("system info")) {
            return nativeOrFallback("msinfo32.exe", QStyle::SP_FileDialogInfoView);
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

QIcon MainWindow::iconForCategory(LauncherCategory category) const {
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

QIcon MainWindow::iconForSection(const LauncherSection& section) const {
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

QIcon MainWindow::themedIcon(const QString& key) const {
    const QString normalized = key.toLower();
    const QString path = QString(":/icon-%1.svg").arg(normalized);
    QIcon icon(path);
    if (!icon.isNull()) {
        return icon;
    }
    if (normalized == "folder" || normalized == "folder-system" || normalized == "documents" ||
        normalized == "downloads") {
        return style()->standardIcon(QStyle::SP_DirIcon);
    }
    if (normalized == "desktop") {
        return style()->standardIcon(QStyle::SP_DesktopIcon);
    }
    if (normalized == "settings") {
        return style()->standardIcon(QStyle::SP_FileDialogDetailedView);
    }
    if (normalized == "globe") {
        return style()->standardIcon(QStyle::SP_DriveNetIcon);
    }
    if (normalized == "control-panel" || normalized == "program" || normalized == "system") {
        return style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    if (normalized == "task-manager" || normalized == "services" || normalized == "system-info") {
        return style()->standardIcon(QStyle::SP_FileDialogInfoView);
    }
    if (normalized == "terminal" || normalized == "registry" || normalized == "calculator" ||
        normalized == "device-manager") {
        return style()->standardIcon(QStyle::SP_FileIcon);
    }
    return AppIcon::launcherIcon();
}

bool MainWindow::rulePassesFilters(const HotkeyRule& rule) const {
    if (rule.category != m_currentCategory) {
        return false;
    }

    const QString search = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    if (!search.isEmpty()) {
        const QString haystack =
            QString("%1 %2 %3 %4")
                .arg(ruleTitle(rule), rule.action.target, rule.description, rule.hotkey.displayText());
        if (!haystack.contains(search, Qt::CaseInsensitive)) {
            return false;
        }
    }
    return true;
}

QString MainWindow::ruleTitle(const HotkeyRule& rule) const {
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

QString MainWindow::categoryDisplayName(LauncherCategory category) const {
    return UiText::categoryName(language(), category);
}

QString MainWindow::passwordHash(const QString& password) const {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0) && defined(Q_OS_WIN)
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    QByteArray digest;
    const QByteArray input = password.toUtf8();
    if (CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) &&
        CryptHashData(hash, reinterpret_cast<const BYTE*>(input.constData()), static_cast<DWORD>(input.size()), 0)) {
        DWORD hashSize = 0;
        DWORD hashSizeLength = sizeof(hashSize);
        if (CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&hashSize), &hashSizeLength, 0) &&
            hashSize > 0) {
            digest.resize(static_cast<int>(hashSize));
            DWORD digestLength = hashSize;
            if (!CryptGetHashParam(hash, HP_HASHVAL, reinterpret_cast<BYTE*>(digest.data()), &digestLength, 0)) {
                digest.clear();
            }
        }
    }
    if (hash) {
        CryptDestroyHash(hash);
    }
    if (provider) {
        CryptReleaseContext(provider, 0);
    }
    if (!digest.isEmpty()) {
        return QString::fromLatin1(digest.toHex());
    }
    return QString::fromLatin1(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha1).toHex());
#else
    return QString::fromLatin1(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
#endif
}

QString MainWindow::uiText(UiText::Key key) const {
    return UiText::text(language(), key);
}

MainWindow::ResizeRegion MainWindow::resizeRegionAt(const QPoint& position) const {
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

void MainWindow::updateResizeCursor(const QPoint& position) {
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

void MainWindow::performResize(const QPoint& globalPosition) {
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

void MainWindow::finishInteractiveMove() {
    if (!m_resizing) {
        snapToTopIfNeeded();
    }
}

void MainWindow::snapToTopIfNeeded() {
    if (m_topAutoHidden || !isVisible() || isMinimized()) {
        return;
    }

    const QPoint cursorPosition = QCursor::pos();
    QScreen* targetScreen = QtCompat::screenAtPoint(cursorPosition);
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

    // Snap immediately, but defer hiding until the pointer leaves the restored window.
    // This prevents the launcher from disappearing while the user is still placing it.
    QRect next = geometry();
    next.moveTop(screenGeometry.top());
    next.moveLeft(qBound(screenGeometry.left(), next.left(), screenGeometry.right() - next.width() + 1));
    setGeometry(next);
    m_autoHideShownGeometry = geometry();
    setAlwaysOnTop(true);
    m_autoHideTimer.start();
}

void MainWindow::setTopAutoHidden(bool hidden) {
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
            hiddenGeometry.moveLeft(qBound(screenGeometry.left(), hiddenGeometry.left(),
                                           screenGeometry.right() - hiddenGeometry.width() + 1));
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
    setMinimumHeight(QtCompat::scaleInt(WindowMinimumHeight));
    setMaximumHeight(QWIDGETSIZE_MAX);
    if (m_autoHideShownGeometry.isValid()) {
        const QRect screenGeometry = currentScreenAvailableGeometry();
        QRect shownGeometry = m_autoHideShownGeometry;
        if (!screenGeometry.isNull()) {
            shownGeometry.moveLeft(qBound(screenGeometry.left(), shownGeometry.left(),
                                          screenGeometry.right() - shownGeometry.width() + 1));
            shownGeometry.moveTop(screenGeometry.top());
        }
        setGeometry(shownGeometry);
    }
}

void MainWindow::revealFromTopAutoHide() {
    if (m_topAutoHidden) {
        setTopAutoHidden(false);
    }
}

void MainWindow::updateTopAutoHide() {
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

void MainWindow::setAlwaysOnTop(bool enabled) {
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return;
    }
    const HWND insertAfter = enabled ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(hwnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
#else
    Q_UNUSED(enabled);
#endif
}

QScreen* MainWindow::currentScreen() const {
    return QtCompat::screenForWidget(this);
}

QRect MainWindow::currentScreenAvailableGeometry() const {
    if (QScreen* targetScreen = currentScreen()) {
        return targetScreen->availableGeometry();
    }
    if (QScreen* primary = QGuiApplication::primaryScreen()) {
        return primary->availableGeometry();
    }
    return {};
}
