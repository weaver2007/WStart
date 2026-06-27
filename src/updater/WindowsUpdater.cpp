#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace {

std::wstring argumentValue(const std::vector<std::wstring>& args, const wchar_t* name) {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == name) {
            return args[i + 1];
        }
    }
    return std::wstring();
}

bool waitForProcess(DWORD pid) {
    if (pid == 0) {
        return true;
    }
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
    if (!process) {
        return true;
    }

    DWORD waitResult = WaitForSingleObject(process, 60000);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(process, 0);
        waitResult = WaitForSingleObject(process, 10000);
    }

    CloseHandle(process);
    return waitResult == WAIT_OBJECT_0;
}

bool pathExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isDirectory(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring joinPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty() || left[left.size() - 1] == L'\\' || left[left.size() - 1] == L'/') {
        return left + right;
    }
    return left + L"\\" + right;
}

void restartApplication(const std::wstring& restartPath) {
    if (!restartPath.empty()) {
        ShellExecuteW(nullptr, L"open", restartPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

bool removeTree(const std::wstring& path) {
    if (!pathExists(path)) {
        return true;
    }
    std::wstring from = path;
    from.push_back(L'\0');
    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationW(&op) == 0;
}

bool ensureDirectory(const std::wstring& path) {
    if (isDirectory(path)) {
        return true;
    }
    if (CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS && isDirectory(path);
}

bool copyFileReplacing(const std::wstring& fromPath, const std::wstring& toPath) {
    for (int attempt = 0; attempt < 120; ++attempt) {
        if (pathExists(toPath)) {
            SetFileAttributesW(toPath.c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        if (CopyFileW(fromPath.c_str(), toPath.c_str(), FALSE)) {
            return true;
        }
        const DWORD error = GetLastError();
        if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION && error != ERROR_ACCESS_DENIED) {
            return false;
        }
        Sleep(500);
    }
    return false;
}

bool copyTree(const std::wstring& fromPath, const std::wstring& toPath) {
    if (!ensureDirectory(toPath)) {
        return false;
    }

    WIN32_FIND_DATAW findData = {};
    const std::wstring searchPath = joinPath(fromPath, L"*");
    HANDLE findHandle = FindFirstFileW(searchPath.c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }

    bool ok = true;
    do {
        const std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }

        const std::wstring source = joinPath(fromPath, name);
        const std::wstring target = joinPath(toPath, name);
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            ok = copyTree(source, target);
        } else {
            ok = copyFileReplacing(source, target);
        }
    } while (ok && FindNextFileW(findHandle, &findData));

    const DWORD lastError = GetLastError();
    FindClose(findHandle);
    return ok && lastError == ERROR_NO_MORE_FILES;
}

std::wstring quotePowerShellString(std::wstring value) {
    std::wstring result = L"'";
    for (wchar_t ch : value) {
        if (ch == L'\'') {
            result += L"''";
        } else {
            result += ch;
        }
    }
    result += L"'";
    return result;
}

bool runPowerShellExpand(const std::wstring& packagePath, const std::wstring& destinationPath) {
    CreateDirectoryW(destinationPath.c_str(), nullptr);
    const std::wstring command = L"-NoProfile -ExecutionPolicy Bypass -Command Expand-Archive -LiteralPath " +
                                 quotePowerShellString(packagePath) + L" -DestinationPath " +
                                 quotePowerShellString(destinationPath) + L" -Force";
    STARTUPINFOW startup = {};
    PROCESS_INFORMATION process = {};
    startup.cb = sizeof(startup);
    std::wstring mutableCommand = L"powershell.exe " + command;
    if (!CreateProcessW(nullptr, &mutableCommand[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup, &process)) {
        return false;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode == 0;
}

std::wstring tempPathFor(const std::wstring& suffix) {
    wchar_t buffer[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, buffer);
    return std::wstring(buffer) + L"WStartUpdate-" + suffix;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    if (argv) {
        LocalFree(argv);
    }

    const DWORD pid = static_cast<DWORD>(_wtoi(argumentValue(args, L"--pid").c_str()));
    const std::wstring packagePath = argumentValue(args, L"--package");
    const std::wstring targetDir = argumentValue(args, L"--target-dir");
    const std::wstring restartPath = argumentValue(args, L"--restart");
    if (packagePath.empty() || targetDir.empty() || restartPath.empty()) {
        return 2;
    }

    if (!waitForProcess(pid)) {
        restartApplication(restartPath);
        return 6;
    }

    const std::wstring extractDir = tempPathFor(L"extract");
    const std::wstring backupDir = tempPathFor(L"backup");
    removeTree(extractDir);
    removeTree(backupDir);

    if (!runPowerShellExpand(packagePath, extractDir)) {
        restartApplication(restartPath);
        return 3;
    }

    std::wstring payloadDir = extractDir;
    if (pathExists(extractDir + L"\\WStart")) {
        payloadDir = extractDir + L"\\WStart";
    }

    // Keep the portable directory in place. The backup is only used to restore overwritten files if copying fails.
    if (!copyTree(targetDir, backupDir)) {
        removeTree(extractDir);
        restartApplication(restartPath);
        return 4;
    }
    if (!copyTree(payloadDir, targetDir)) {
        copyTree(backupDir, targetDir);
        removeTree(backupDir);
        removeTree(extractDir);
        restartApplication(restartPath);
        return 5;
    }

    removeTree(backupDir);
    removeTree(extractDir);
    restartApplication(restartPath);
    return 0;
}
