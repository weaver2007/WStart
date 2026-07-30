#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace {

const wchar_t kManagedFilesName[] = L".wstart-managed-files.txt";
const LONGLONG kMaxManagedFileSize = 1024 * 1024;

struct CaseInsensitiveLess {
    bool operator()(const std::wstring& left, const std::wstring& right) const {
        return _wcsicmp(left.c_str(), right.c_str()) < 0;
    }
};

struct TransactionFile {
    std::wstring relativePath;
    std::wstring sourcePath;
    std::wstring targetPath;
    std::wstring backupPath;
    bool targetExisted = false;
    bool removeOnly = false;
    bool changed = false;
};

std::wstring argumentValue(const std::vector<std::wstring>& args, const wchar_t* name) {
    for (size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == name) {
            return args[index + 1];
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
    const DWORD waitResult = WaitForSingleObject(process, 60000);
    CloseHandle(process);
    return waitResult == WAIT_OBJECT_0;
}

DWORD pathAttributes(const std::wstring& path) {
    return GetFileAttributesW(path.c_str());
}

bool pathExists(const std::wstring& path) {
    return pathAttributes(path) != INVALID_FILE_ATTRIBUTES;
}

bool isDirectory(const std::wstring& path) {
    const DWORD attributes = pathAttributes(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool hasMultipleHardLinks(const std::wstring& path) {
    HANDLE file =
        CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    BY_HANDLE_FILE_INFORMATION information = {};
    const bool multiple = GetFileInformationByHandle(file, &information) != FALSE && information.nNumberOfLinks > 1;
    CloseHandle(file);
    return multiple;
}

std::wstring joinPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty() || left[left.size() - 1] == L'\\' || left[left.size() - 1] == L'/') {
        return left + right;
    }
    return left + L"\\" + right;
}

std::wstring parentPath(const std::wstring& path) {
    const size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring() : path.substr(0, separator);
}

std::wstring systemExecutablePath(const wchar_t* relativePath) {
    wchar_t systemDirectory[MAX_PATH + 1] = {};
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH + 1);
    if (length == 0 || length > MAX_PATH) {
        return std::wstring();
    }
    return joinPath(systemDirectory, relativePath);
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
    SHFILEOPSTRUCTW operation = {};
    operation.wFunc = FO_DELETE;
    operation.pFrom = from.c_str();
    operation.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationW(&operation) == 0;
}

bool ensureDirectoryTree(const std::wstring& path) {
    if (path.empty() || isDirectory(path)) {
        return true;
    }
    const std::wstring parent = parentPath(path);
    if (!parent.empty() && parent != path && !ensureDirectoryTree(parent)) {
        return false;
    }
    if (CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS && isDirectory(path);
}

bool copyFileWithRetry(const std::wstring& fromPath, const std::wstring& toPath, bool failIfExists) {
    if (!ensureDirectoryTree(parentPath(toPath))) {
        return false;
    }
    for (int attempt = 0; attempt < 120; ++attempt) {
        if (!failIfExists && pathExists(toPath)) {
            SetFileAttributesW(toPath.c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        if (CopyFileW(fromPath.c_str(), toPath.c_str(), failIfExists ? TRUE : FALSE)) {
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

bool collectPayloadFiles(const std::wstring& rootPath, const std::wstring& relativeDirectory,
                         std::vector<TransactionFile>* files) {
    if (!files) {
        return false;
    }
    const std::wstring directory = relativeDirectory.empty() ? rootPath : joinPath(rootPath, relativeDirectory);
    WIN32_FIND_DATAW findData = {};
    HANDLE findHandle = FindFirstFileW(joinPath(directory, L"*").c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }

    bool ok = true;
    do {
        const std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            ok = false;
            break;
        }
        const std::wstring relativePath = relativeDirectory.empty() ? name : joinPath(relativeDirectory, name);
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            ok = collectPayloadFiles(rootPath, relativePath, files);
        } else if (_wcsicmp(relativePath.c_str(), kManagedFilesName) != 0) {
            TransactionFile file;
            file.relativePath = relativePath;
            file.sourcePath = joinPath(rootPath, relativePath);
            files->push_back(file);
        }
    } while (ok && FindNextFileW(findHandle, &findData));

    const DWORD lastError = GetLastError();
    FindClose(findHandle);
    return ok && lastError == ERROR_NO_MORE_FILES;
}

bool isSafeRelativePath(const std::wstring& path) {
    if (path.empty() || path[0] == L'\\' || path[0] == L'/' || path.find(L':') != std::wstring::npos) {
        return false;
    }
    size_t start = 0;
    while (start <= path.size()) {
        const size_t end = path.find_first_of(L"\\/", start);
        const std::wstring part = path.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (part.empty() || part == L"." || part == L"..") {
            return false;
        }
        if (end == std::wstring::npos) {
            break;
        }
        start = end + 1;
    }
    return true;
}

bool hasUnsafeTargetComponent(const std::wstring& rootPath, const std::wstring& relativePath) {
    std::wstring current = rootPath;
    size_t start = 0;
    while (start < relativePath.size()) {
        const size_t end = relativePath.find_first_of(L"\\/", start);
        const std::wstring part =
            relativePath.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        current = joinPath(current, part);
        const DWORD attributes = pathAttributes(current);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const DWORD error = GetLastError();
            return error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND;
        }
        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            return true;
        }
        if (end == std::wstring::npos) {
            return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && hasMultipleHardLinks(current);
        }
        start = end + 1;
    }
    return false;
}

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }
    const int size =
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return std::string();
    }
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }
    const int size =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring();
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), &result[0], size);
    return result;
}

bool readManagedFiles(const std::wstring& path, std::vector<std::wstring>* files) {
    if (!files || !pathExists(path)) {
        return true;
    }
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > kMaxManagedFileSize) {
        CloseHandle(file);
        return false;
    }
    std::string contents(static_cast<size_t>(size.QuadPart), '\0');
    DWORD bytesRead = 0;
    const bool read = contents.empty() ||
                      ReadFile(file, &contents[0], static_cast<DWORD>(contents.size()), &bytesRead, nullptr) != FALSE;
    CloseHandle(file);
    if (!read) {
        return false;
    }
    contents.resize(bytesRead);
    std::set<std::wstring, CaseInsensitiveLess> uniquePaths;
    size_t start = 0;
    while (start < contents.size()) {
        const size_t end = contents.find('\n', start);
        std::string line = contents.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.resize(line.size() - 1);
        }
        if (!line.empty()) {
            const std::wstring relativePath = utf8ToWide(line);
            if (relativePath.empty() || !isSafeRelativePath(relativePath)) {
                return false;
            }
            if (uniquePaths.insert(relativePath).second) {
                files->push_back(relativePath);
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return true;
}

bool writeManagedFiles(const std::wstring& path, const std::vector<TransactionFile>& files) {
    std::string contents;
    for (const TransactionFile& file : files) {
        if (!file.removeOnly && file.relativePath != kManagedFilesName) {
            const std::string encodedPath = wideToUtf8(file.relativePath);
            if (encodedPath.empty() ||
                contents.size() + encodedPath.size() + 1 > static_cast<size_t>(kMaxManagedFileSize)) {
                return false;
            }
            contents += encodedPath;
            contents.push_back('\n');
        }
    }
    HANDLE output = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const bool ok =
        contents.empty() ||
        (WriteFile(output, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) != FALSE &&
         written == static_cast<DWORD>(contents.size()) && FlushFileBuffers(output) != FALSE);
    CloseHandle(output);
    return ok;
}

std::wstring quotePowerShellString(const std::wstring& value) {
    std::wstring result = L"'";
    for (wchar_t character : value) {
        if (character == L'\'') {
            result += L"''";
        } else {
            result.push_back(character);
        }
    }
    result += L"'";
    return result;
}

bool runPowerShellExpand(const std::wstring& packagePath, const std::wstring& destinationPath) {
    if (!ensureDirectoryTree(destinationPath)) {
        return false;
    }
    const std::wstring command = L"-NoProfile -ExecutionPolicy Bypass -Command Expand-Archive -LiteralPath " +
                                 quotePowerShellString(packagePath) + L" -DestinationPath " +
                                 quotePowerShellString(destinationPath) + L" -Force";
    STARTUPINFOW startup = {};
    PROCESS_INFORMATION process = {};
    startup.cb = sizeof(startup);
    const std::wstring powershellPath = systemExecutablePath(L"WindowsPowerShell\\v1.0\\powershell.exe");
    if (powershellPath.empty()) {
        return false;
    }
    std::wstring mutableCommand = L"\"" + powershellPath + L"\" " + command;
    if (!CreateProcessW(powershellPath.c_str(), &mutableCommand[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &startup, &process)) {
        return false;
    }
    const DWORD waitResult = WaitForSingleObject(process.hProcess, 300000);
    DWORD exitCode = 1;
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(process.hProcess, &exitCode);
    } else {
        TerminateProcess(process.hProcess, 1);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return waitResult == WAIT_OBJECT_0 && exitCode == 0;
}

std::wstring tempPathFor(const std::wstring& suffix) {
    wchar_t buffer[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, buffer);
    return std::wstring(buffer) + L"WStartUpdate-" + std::to_wstring(GetCurrentProcessId()) + L"-" + suffix;
}

bool prepareTransaction(const std::wstring& payloadDir, const std::wstring& targetDir, const std::wstring& backupDir,
                        std::vector<TransactionFile>* files) {
    if (!collectPayloadFiles(payloadDir, std::wstring(), files) || files->empty()) {
        return false;
    }
    const std::wstring newExecutable = joinPath(payloadDir, L"WStart.exe");
    if (!pathExists(newExecutable) || isDirectory(newExecutable)) {
        return false;
    }

    std::set<std::wstring, CaseInsensitiveLess> newPaths;
    for (TransactionFile& file : *files) {
        if (!isSafeRelativePath(file.relativePath) || hasUnsafeTargetComponent(targetDir, file.relativePath)) {
            return false;
        }
        newPaths.insert(file.relativePath);
        file.targetPath = joinPath(targetDir, file.relativePath);
        file.backupPath = joinPath(backupDir, file.relativePath);
    }

    std::vector<std::wstring> oldPaths;
    if (!readManagedFiles(joinPath(targetDir, kManagedFilesName), &oldPaths)) {
        return false;
    }
    for (const std::wstring& oldPath : oldPaths) {
        if (hasUnsafeTargetComponent(targetDir, oldPath)) {
            return false;
        }
        if (newPaths.find(oldPath) == newPaths.end()) {
            TransactionFile stale;
            stale.relativePath = oldPath;
            stale.targetPath = joinPath(targetDir, oldPath);
            stale.backupPath = joinPath(backupDir, oldPath);
            stale.removeOnly = true;
            files->push_back(stale);
        }
    }

    const std::wstring generatedManifest = joinPath(backupDir, L"managed-files.new");
    if (!ensureDirectoryTree(backupDir) || !writeManagedFiles(generatedManifest, *files)) {
        return false;
    }
    TransactionFile manifest;
    manifest.relativePath = kManagedFilesName;
    manifest.sourcePath = generatedManifest;
    manifest.targetPath = joinPath(targetDir, kManagedFilesName);
    manifest.backupPath = joinPath(backupDir, L"managed-files.previous");
    if (hasUnsafeTargetComponent(targetDir, manifest.relativePath)) {
        return false;
    }
    files->push_back(manifest);
    return true;
}

bool backupTransactionFiles(std::vector<TransactionFile>* files) {
    for (TransactionFile& file : *files) {
        if (!pathExists(file.targetPath)) {
            continue;
        }
        if (isDirectory(file.targetPath)) {
            return false;
        }
        file.targetExisted = true;
        if (!copyFileWithRetry(file.targetPath, file.backupPath, true)) {
            return false;
        }
    }
    return true;
}

bool applyTransaction(std::vector<TransactionFile>* files) {
    for (TransactionFile& file : *files) {
        if (file.removeOnly) {
            file.changed = true;
            if (pathExists(file.targetPath)) {
                SetFileAttributesW(file.targetPath.c_str(), FILE_ATTRIBUTE_NORMAL);
                if (!DeleteFileW(file.targetPath.c_str())) {
                    return false;
                }
            }
        } else {
            // CopyFile may leave a partial destination on failure. Mark the
            // target before mutation so rollback always restores or removes it.
            file.changed = true;
            if (!copyFileWithRetry(file.sourcePath, file.targetPath, false)) {
                return false;
            }
        }
    }
    return true;
}

bool rollbackTransaction(std::vector<TransactionFile>* files) {
    bool ok = true;
    for (std::vector<TransactionFile>::reverse_iterator it = files->rbegin(); it != files->rend(); ++it) {
        if (!it->changed) {
            continue;
        }
        if (it->targetExisted) {
            ok = copyFileWithRetry(it->backupPath, it->targetPath, false) && ok;
        } else if (pathExists(it->targetPath)) {
            SetFileAttributesW(it->targetPath.c_str(), FILE_ATTRIBUTE_NORMAL);
            ok = DeleteFileW(it->targetPath.c_str()) != FALSE && ok;
        }
    }
    return ok;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args;
    for (int index = 0; index < argc; ++index) {
        args.push_back(argv[index]);
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
    if (isDirectory(joinPath(extractDir, L"WStart"))) {
        payloadDir = joinPath(extractDir, L"WStart");
    }

    std::vector<TransactionFile> transaction;
    if (!prepareTransaction(payloadDir, targetDir, backupDir, &transaction) || !backupTransactionFiles(&transaction)) {
        removeTree(backupDir);
        removeTree(extractDir);
        restartApplication(restartPath);
        return 4;
    }
    if (!applyTransaction(&transaction)) {
        const bool restored = rollbackTransaction(&transaction);
        removeTree(backupDir);
        removeTree(extractDir);
        restartApplication(restartPath);
        return restored ? 5 : 7;
    }

    removeTree(backupDir);
    removeTree(extractDir);
    restartApplication(restartPath);
    return 0;
}
