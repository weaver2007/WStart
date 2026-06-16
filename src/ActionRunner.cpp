#include "ActionRunner.h"

#include <QDir>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

ActionRunner::ActionRunner(QObject *parent)
    : QObject(parent)
{
}

bool ActionRunner::run(const LaunchAction &action, QString *error) const
{
    if (!action.isValid()) {
        if (error) {
            *error = "Action target is empty.";
        }
        return false;
    }

#ifdef Q_OS_WIN
    const std::wstring verb = L"open";
    const std::wstring target = action.target.toStdWString();
    const std::wstring arguments = action.arguments.toStdWString();
    const std::wstring workingDirectory = QDir::toNativeSeparators(action.workingDirectory).toStdWString();

    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = verb.c_str();
    info.lpFile = target.c_str();
    info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
    info.lpDirectory = workingDirectory.empty() ? nullptr : workingDirectory.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info)) {
        if (error) {
            *error = QString("ShellExecuteEx failed with error %1.").arg(GetLastError());
        }
        return false;
    }
    if (info.hProcess) {
        CloseHandle(info.hProcess);
    }
    return true;
#else
    Q_UNUSED(action)
    if (error) {
        *error = "ActionRunner is implemented for Windows only.";
    }
    return false;
#endif
}
