#include "SystemIconProvider.h"

#include "PathUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPixmap>

#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>
#endif

namespace {

#ifdef Q_OS_WIN
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

QString knownWindowsFilePath(const QString& fileName) {
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

QString expandedWindowsPath(const QString& path) {
    const std::wstring source = toWideString(path);
    const DWORD required = ExpandEnvironmentStringsW(source.c_str(), nullptr, 0);
    if (required <= 1) {
        return knownWindowsFilePath(path);
    }
    std::vector<wchar_t> buffer(static_cast<size_t>(required));
    if (!ExpandEnvironmentStringsW(source.c_str(), buffer.data(), required)) {
        return knownWindowsFilePath(path);
    }
    return knownWindowsFilePath(fromWideString(buffer.data()));
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
    const QPixmap pixmap = QPixmap::fromImage(image.copy());

    SelectObject(dc, previousBitmap);
    DeleteDC(dc);
    DeleteObject(bitmap);
    return pixmap;
}

QIcon iconFromNativeHandles(HICON largeIcon, HICON smallIcon) {
    QIcon icon;
    if (largeIcon) {
        const QPixmap pixmap =
            pixmapFromNativeIcon(largeIcon, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
        if (!pixmap.isNull()) {
            icon.addPixmap(pixmap);
        }
    }
    if (smallIcon) {
        const QPixmap pixmap =
            pixmapFromNativeIcon(smallIcon, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
        if (!pixmap.isNull()) {
            icon.addPixmap(pixmap);
        }
    }
    return icon;
}

QIcon loadFileIcon(const QString& path) {
    const QString resolvedPath = knownWindowsFilePath(PathUtils::toAbsolutePath(path));
    const bool exists = QFileInfo(resolvedPath).exists();
    const std::wstring nativePath = toWideString(QDir::toNativeSeparators(resolvedPath));
    SHFILEINFOW largeInfo = {};
    SHFILEINFOW smallInfo = {};
    const UINT commonFlags = SHGFI_ICON | (exists ? 0 : SHGFI_USEFILEATTRIBUTES);
    const DWORD_PTR largeResult = SHGetFileInfoW(nativePath.c_str(), exists ? 0 : FILE_ATTRIBUTE_NORMAL, &largeInfo,
                                                 sizeof(largeInfo), commonFlags | SHGFI_LARGEICON);
    const DWORD_PTR smallResult = SHGetFileInfoW(nativePath.c_str(), exists ? 0 : FILE_ATTRIBUTE_NORMAL, &smallInfo,
                                                 sizeof(smallInfo), commonFlags | SHGFI_SMALLICON);
    if (!largeResult && !smallResult) {
        return QIcon();
    }

    const QIcon icon = iconFromNativeHandles(largeInfo.hIcon, smallInfo.hIcon);
    if (largeInfo.hIcon) {
        DestroyIcon(largeInfo.hIcon);
    }
    if (smallInfo.hIcon) {
        DestroyIcon(smallInfo.hIcon);
    }
    return icon;
}

QIcon resourceIcon(const QString& module, int iconIndex) {
    const QString resolvedPath = expandedWindowsPath(module);
    if (!QFileInfo(resolvedPath).exists()) {
        return QIcon();
    }

    HICON largeIcon = nullptr;
    HICON smallIcon = nullptr;
    const std::wstring nativePath = toWideString(QDir::toNativeSeparators(resolvedPath));
    if (ExtractIconExW(nativePath.c_str(), iconIndex, &largeIcon, &smallIcon, 1) == 0) {
        return QIcon();
    }
    const QIcon icon = iconFromNativeHandles(largeIcon, smallIcon);
    if (largeIcon) {
        DestroyIcon(largeIcon);
    }
    if (smallIcon) {
        DestroyIcon(smallIcon);
    }
    return icon;
}

QIcon shellItemIcon(const QString& parsingName) {
    typedef HRESULT(WINAPI * SHParseDisplayNameFunction)(LPCWSTR, IBindCtx*, LPITEMIDLIST*, SFGAOF, SFGAOF*);
    const HMODULE shellModule = GetModuleHandleW(L"shell32.dll");
    const SHParseDisplayNameFunction parseDisplayName =
        shellModule ? reinterpret_cast<SHParseDisplayNameFunction>(GetProcAddress(shellModule, "SHParseDisplayName"))
                    : nullptr;
    if (!parseDisplayName) {
        return QIcon();
    }

    const std::wstring nativeName = toWideString(parsingName);
    LPITEMIDLIST itemIdList = nullptr;
    SFGAOF attributes = 0;
    if (FAILED(parseDisplayName(nativeName.c_str(), nullptr, &itemIdList, 0, &attributes)) || !itemIdList) {
        return QIcon();
    }

    SHFILEINFOW largeInfo = {};
    SHFILEINFOW smallInfo = {};
    const DWORD_PTR largeResult = SHGetFileInfoW(reinterpret_cast<LPCWSTR>(itemIdList), 0, &largeInfo,
                                                 sizeof(largeInfo), SHGFI_PIDL | SHGFI_ICON | SHGFI_LARGEICON);
    const DWORD_PTR smallResult = SHGetFileInfoW(reinterpret_cast<LPCWSTR>(itemIdList), 0, &smallInfo,
                                                 sizeof(smallInfo), SHGFI_PIDL | SHGFI_ICON | SHGFI_SMALLICON);
    CoTaskMemFree(itemIdList);
    if (!largeResult && !smallResult) {
        return QIcon();
    }

    const QIcon icon = iconFromNativeHandles(largeInfo.hIcon, smallInfo.hIcon);
    if (largeInfo.hIcon) {
        DestroyIcon(largeInfo.hIcon);
    }
    if (smallInfo.hIcon) {
        DestroyIcon(smallInfo.hIcon);
    }
    return icon;
}

QIcon firstAvailable(std::initializer_list<QIcon> icons) {
    for (const QIcon& icon : icons) {
        if (!icon.isNull()) {
            return icon;
        }
    }
    return QIcon();
}

QIcon loadSystemToolIcon(const HotkeyRule& rule) {
    if (rule.sectionId != QString::fromLatin1("program-system")) {
        return QIcon();
    }

    const LaunchAction action = PathUtils::toAbsoluteAction(rule.action);
    const QString target = QFileInfo(action.target).fileName().toLower();
    const QString arguments = action.arguments.toLower();
    const QString title = rule.description.toLower();
    const auto hasTitle = [&title](const char* text) { return title.contains(QString::fromUtf8(text)); };
    const auto hasArgument = [&arguments](const char* text) { return arguments.contains(QString::fromLatin1(text)); };

    // Shell namespace objects preserve the icon selected by the installed Windows version.
    if (hasArgument("shell:personal")) {
        return shellItemIcon(QString::fromLatin1("shell:Personal"));
    }
    if (hasArgument("shell:mycomputerfolder")) {
        return shellItemIcon(QString::fromLatin1("shell:MyComputerFolder"));
    }
    if (hasArgument("shell:recyclebinfolder")) {
        return shellItemIcon(QString::fromLatin1("shell:RecycleBinFolder"));
    }
    if (hasArgument("ncpa.cpl")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{7007ACC7-3202-11D1-AAD2-00805FC1270E}")),
            resourceIcon(QString::fromLatin1("ncpa.cpl"), 0),
        });
    }
    if (target == QString::fromLatin1("control.exe") && arguments.trimmed().isEmpty()) {
        return shellItemIcon(QString::fromLatin1("shell:ControlPanelFolder"));
    }
    if (hasArgument("printers")) {
        return shellItemIcon(QString::fromLatin1("shell:PrintersFolder"));
    }
    if (hasArgument("desk.cpl")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{ED834ED6-4B5A-4BFE-8F11-A626DCB6A921}")),
            resourceIcon(QString::fromLatin1("desk.cpl"), 0),
        });
    }
    if (hasArgument("timedate.cpl")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{E2E7934B-DCE5-43C4-9576-7FE4F75E7480}")),
            resourceIcon(QString::fromLatin1("timedate.cpl"), 0),
        });
    }
    if (hasArgument("inetcpl.cpl")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{A3DD4F92-658A-410F-84FD-6FBBBEF2FFFE}")),
            resourceIcon(QString::fromLatin1("inetcpl.cpl"), 0),
        });
    }
    if (hasArgument("sysdm.cpl") && target != QString::fromLatin1("rundll32.exe")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{BB06C0E4-D293-4F75-8A90-CB05B6477EEE}")),
            resourceIcon(QString::fromLatin1("sysdm.cpl"), 0),
        });
    }
    if (hasTitle("环境变量") || (target == QString::fromLatin1("rundll32.exe") && hasArgument("sysdm.cpl"))) {
        return resourceIcon(QString::fromLatin1("sysdm.cpl"), 0);
    }
    if (hasArgument("folders")) {
        return shellItemIcon(QString::fromLatin1("shell:::{6DFD7C5C-2451-11D3-A299-00C04F8EF6AF}"));
    }
    if (target == QString::fromLatin1("devmgmt.msc")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{74246BFC-4C96-11D0-ABEF-0020AF6B0B7A}")),
            resourceIcon(QString::fromLatin1("devmgr.dll"), 5),
        });
    }
    if (hasArgument("appwiz.cpl")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{7B81BE6A-CE2B-4676-A29E-EB907A5126C5}")),
            resourceIcon(QString::fromLatin1("appwiz.cpl"), 0),
        });
    }
    if (target == QString::fromLatin1("diskmgmt.msc")) {
        return resourceIcon(QString::fromLatin1("dmdskres.dll"), 0);
    }
    if (target == QString::fromLatin1("compmgmt.msc")) {
        return resourceIcon(QString::fromLatin1("mycomput.dll"), 2);
    }
    if (target == QString::fromLatin1("services.msc")) {
        return resourceIcon(QString::fromLatin1("filemgmt.dll"), 0);
    }
    if (target == QString::fromLatin1("gpedit.msc")) {
        return resourceIcon(QString::fromLatin1("gpedit.dll"), 0);
    }
    if (hasArgument("mouse")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{6C8EEC18-8D75-41B2-A177-8831D59D2D50}")),
            resourceIcon(QString::fromLatin1("main.cpl"), 0),
        });
    }
    if (hasArgument("keyboard")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{725BE8F7-668E-4C7B-8F90-46BDB0936430}")),
            resourceIcon(QString::fromLatin1("main.cpl"), 5),
        });
    }
    if (hasArgument("mmsys.cpl")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{F2DDFC82-8F12-4CDD-B7DC-D4FE1425AA4D}")),
            resourceIcon(QString::fromLatin1("mmsys.cpl"), 0),
        });
    }
    if (hasArgument("powercfg.cpl")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{025A5937-A6BE-4686-A844-36FE4BEC8B6D}")),
            resourceIcon(QString::fromLatin1("powercfg.cpl"), 0),
        });
    }
    if (hasArgument("firewall.cpl")) {
        return firstAvailable({
            shellItemIcon(QString::fromLatin1("shell:::{4026492F-2F69-46B8-B9BF-5654FC07E423}")),
            resourceIcon(QString::fromLatin1("FirewallControlPanel.dll"), 0),
        });
    }

    // These IDs match the icon locations used by Windows' own Start menu shortcuts.
    if (target == QString::fromLatin1("msconfig.exe")) {
        return firstAvailable({resourceIcon(action.target, -3000), loadFileIcon(action.target)});
    }
    if (target == QString::fromLatin1("taskmgr.exe")) {
        return firstAvailable({resourceIcon(action.target, -30651), loadFileIcon(action.target)});
    }
    if (target == QString::fromLatin1("regedit.exe")) {
        return firstAvailable({resourceIcon(action.target, -100), loadFileIcon(action.target)});
    }

    if (action.target.compare(QString::fromLatin1("ms-screenclip:"), Qt::CaseInsensitive) == 0) {
        const QString snippingTool = knownWindowsFilePath(QString::fromLatin1("SnippingTool.exe"));
        return QFileInfo(snippingTool).exists() ? loadFileIcon(snippingTool) : QIcon();
    }

    // Windows provides no stable, dedicated icon for these actions. The UI uses bundled semantic icons instead.
    if (target == QString::fromLatin1("shutdown.exe") || target == QString::fromLatin1("powershell.exe")) {
        return QIcon();
    }

    return loadFileIcon(action.target);
}
#endif

} // namespace

QIcon SystemIconProvider::fileIcon(const QString& path) {
#ifdef Q_OS_WIN
    return loadFileIcon(path);
#else
    Q_UNUSED(path)
    return QIcon();
#endif
}

QIcon SystemIconProvider::systemToolIcon(const HotkeyRule& rule) {
#ifdef Q_OS_WIN
    const QString cacheKey = QString::fromLatin1("%1\n%2\n%3\n%4")
                                 .arg(rule.sectionId, rule.action.target, rule.action.arguments, rule.description);
    static QHash<QString, QIcon> cache;
    const auto cached = cache.constFind(cacheKey);
    if (cached != cache.constEnd()) {
        return cached.value();
    }

    const QIcon icon = loadSystemToolIcon(rule);
    cache.insert(cacheKey, icon);
    return icon;
#else
    Q_UNUSED(rule)
    return QIcon();
#endif
}
