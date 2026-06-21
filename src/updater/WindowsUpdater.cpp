#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
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
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!process) {
        return true;
    }
    WaitForSingleObject(process, 30000);
    CloseHandle(process);
    return true;
}

bool pathExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
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

bool moveTree(const std::wstring& fromPath, const std::wstring& toPath) {
    std::wstring from = fromPath;
    std::wstring to = toPath;
    from.push_back(L'\0');
    to.push_back(L'\0');
    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_MOVE;
    op.pFrom = from.c_str();
    op.pTo = to.c_str();
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationW(&op) == 0;
}

bool copyTree(const std::wstring& fromPath, const std::wstring& toPath) {
    std::wstring from = fromPath + L"\\*";
    std::wstring to = toPath;
    from.push_back(L'\0');
    to.push_back(L'\0');
    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_COPY;
    op.pFrom = from.c_str();
    op.pTo = to.c_str();
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationW(&op) == 0;
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

    waitForProcess(pid);
    Sleep(500);

    const std::wstring extractDir = tempPathFor(L"extract");
    const std::wstring backupDir = tempPathFor(L"backup");
    removeTree(extractDir);
    removeTree(backupDir);

    if (!runPowerShellExpand(packagePath, extractDir)) {
        return 3;
    }

    std::wstring payloadDir = extractDir;
    if (pathExists(extractDir + L"\\WStart")) {
        payloadDir = extractDir + L"\\WStart";
    }

    if (!moveTree(targetDir, backupDir)) {
        return 4;
    }
    CreateDirectoryW(targetDir.c_str(), nullptr);
    if (!copyTree(payloadDir, targetDir)) {
        removeTree(targetDir);
        moveTree(backupDir, targetDir);
        return 5;
    }

    removeTree(backupDir);
    removeTree(extractDir);
    ShellExecuteW(nullptr, L"open", restartPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}
