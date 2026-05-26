#include <windows.h>
#include <shlwapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

// Helper to convert std::wstring to UTF-8 std::string
std::string to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Helper to convert std::string (UTF-8) to std::wstring
std::wstring to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Helper to resolve absolute path of a file
std::wstring get_absolute_path(const std::wstring& rel_path) {
    wchar_t buffer[MAX_PATH];
    DWORD len = GetFullPathNameW(rel_path.c_str(), MAX_PATH, buffer, NULL);
    if (len == 0) return rel_path;
    return std::wstring(buffer);
}

// Helper to get directory of the current executable
std::wstring get_current_exe_directory() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::wstring path(buffer);
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return path.substr(0, pos);
    }
    return L"";
}

// Split PATH environment variable string into list of directory paths
std::vector<std::wstring> split_path(const std::wstring& path_str) {
    std::vector<std::wstring> tokens;
    std::wstringstream ss(path_str);
    std::wstring item;
    while (std::getline(ss, item, L';')) {
        if (!item.empty()) {
            tokens.push_back(item);
        }
    }
    return tokens;
}

// Join list of directory paths into a PATH environment variable string
std::wstring join_path(const std::vector<std::wstring>& tokens) {
    std::wstring path_str;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) path_str += L";";
        path_str += tokens[i];
    }
    return path_str;
}

// Case insensitive comparison for file/folder paths
bool path_equals(const std::wstring& a, const std::wstring& b) {
    if (a.length() != b.length()) return false;
    for (size_t i = 0; i < a.length(); ++i) {
        wchar_t ca = towlower(a[i]);
        wchar_t cb = towlower(b[i]);
        if (ca == L'/') ca = L'\\';
        if (cb == L'/') cb = L'\\';
        if (ca != cb) return false;
    }
    return true;
}

// Retrieve the associated executable path for a file extension
std::wstring get_associated_app(const std::wstring& ext) {
    wchar_t assocApp[MAX_PATH * 2] = { 0 };
    DWORD size = sizeof(assocApp) / sizeof(wchar_t);
    
    // Attempt 1: Call AssocQueryStringW
    HRESULT hr = AssocQueryStringW(
        ASSOCF_NOTRUNCATE,
        ASSOCSTR_EXECUTABLE,
        ext.c_str(),
        L"open",
        assocApp,
        &size
    );

    if (SUCCEEDED(hr)) {
        return std::wstring(assocApp);
    }

    // Attempt 2: Call AssocQueryStringW with default association fallback
    size = sizeof(assocApp) / sizeof(wchar_t);
    hr = AssocQueryStringW(
        ASSOCF_INIT_DEFAULTTOSTAR | ASSOCF_NOTRUNCATE,
        ASSOCSTR_EXECUTABLE,
        ext.c_str(),
        L"open",
        assocApp,
        &size
    );

    if (SUCCEEDED(hr)) {
        return std::wstring(assocApp);
    }

    // Fallback: Manual Registry Lookup (e.g. for legacy or custom configurations)
    HKEY hKey;
    std::wstring progId;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, ext.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t value[MAX_PATH] = { 0 };
        DWORD valueSize = sizeof(value);
        if (RegQueryValueExW(hKey, NULL, NULL, NULL, reinterpret_cast<BYTE*>(value), &valueSize) == ERROR_SUCCESS) {
            progId = value;
        }
        RegCloseKey(hKey);
    }

    if (!progId.empty()) {
        std::wstring cmdKey = progId + L"\\shell\\open\\command";
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, cmdKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t cmdValue[MAX_PATH * 2] = { 0 };
            DWORD cmdSize = sizeof(cmdValue);
            if (RegQueryValueExW(hKey, NULL, NULL, NULL, reinterpret_cast<BYTE*>(cmdValue), &cmdSize) == ERROR_SUCCESS) {
                std::wstring commandLine = cmdValue;
                std::wstring exePath;
                if (commandLine[0] == L'"') {
                    size_t nextQuote = commandLine.find(L'"', 1);
                    if (nextQuote != std::wstring::npos) {
                        exePath = commandLine.substr(1, nextQuote - 1);
                    }
                } else {
                    size_t space = commandLine.find(L' ');
                    if (space != std::wstring::npos) {
                        exePath = commandLine.substr(0, space);
                    } else {
                        exePath = commandLine;
                    }
                }
                RegCloseKey(hKey);
                return exePath;
            }
            RegCloseKey(hKey);
        }
    }

    return L"";
}

// Add current directory to the User PATH
bool install_click() {
    std::wstring exe_dir = get_current_exe_directory();
    if (exe_dir.empty()) {
        std::cout << "Error: Could not retrieve current executable directory.\n";
        return false;
    }

    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ | KEY_WRITE, &hKey);
    if (lRes != ERROR_SUCCESS) {
        std::cout << "Error: Could not open HKEY_CURRENT_USER\\Environment key. Code: " << lRes << "\n";
        return false;
    }

    DWORD dwType = 0;
    DWORD dwSize = 0;
    lRes = RegQueryValueExW(hKey, L"Path", NULL, &dwType, NULL, &dwSize);
    
    std::wstring current_path_val;
    if (lRes == ERROR_SUCCESS || lRes == ERROR_MORE_DATA) {
        std::vector<wchar_t> buffer(dwSize / sizeof(wchar_t) + 2, 0);
        lRes = RegQueryValueExW(hKey, L"Path", NULL, &dwType, reinterpret_cast<BYTE*>(buffer.data()), &dwSize);
        if (lRes == ERROR_SUCCESS) {
            current_path_val = buffer.data();
        }
    }

    std::vector<std::wstring> path_tokens = split_path(current_path_val);
    bool already_exists = false;
    for (const auto& token : path_tokens) {
        if (path_equals(token, exe_dir)) {
            already_exists = true;
            break;
        }
    }

    if (already_exists) {
        std::cout << "click is already in the User PATH: " << to_utf8(exe_dir) << "\n";
        RegCloseKey(hKey);
        return true;
    }

    path_tokens.push_back(exe_dir);
    std::wstring new_path_val = join_path(path_tokens);

    lRes = RegSetValueExW(
        hKey,
        L"Path",
        0,
        REG_EXPAND_SZ,
        reinterpret_cast<const BYTE*>(new_path_val.c_str()),
        static_cast<DWORD>((new_path_val.length() + 1) * sizeof(wchar_t))
    );

    RegCloseKey(hKey);

    if (lRes != ERROR_SUCCESS) {
        std::cout << "Error: Failed to write Environment Registry. Code: " << lRes << "\n";
        return false;
    }

    // Broadcast system settings change to notify open shells
    DWORD_PTR dwResult;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Environment"), SMTO_ABORTIFHUNG, 2000, &dwResult);

    std::cout << "Successfully added click directory to User PATH environment variable:\n  " << to_utf8(exe_dir) << "\n";
    std::cout << "Please restart your shell (PowerShell/CMD) to apply changes.\n";
    return true;
}

// Remove current directory from the User PATH
bool uninstall_click() {
    std::wstring exe_dir = get_current_exe_directory();
    if (exe_dir.empty()) {
        std::cout << "Error: Could not retrieve current executable directory.\n";
        return false;
    }

    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ | KEY_WRITE, &hKey);
    if (lRes != ERROR_SUCCESS) {
        std::cout << "Error: Could not open HKEY_CURRENT_USER\\Environment key. Code: " << lRes << "\n";
        return false;
    }

    DWORD dwType = 0;
    DWORD dwSize = 0;
    lRes = RegQueryValueExW(hKey, L"Path", NULL, &dwType, NULL, &dwSize);
    
    std::wstring current_path_val;
    if (lRes == ERROR_SUCCESS || lRes == ERROR_MORE_DATA) {
        std::vector<wchar_t> buffer(dwSize / sizeof(wchar_t) + 2, 0);
        lRes = RegQueryValueExW(hKey, L"Path", NULL, &dwType, reinterpret_cast<BYTE*>(buffer.data()), &dwSize);
        if (lRes == ERROR_SUCCESS) {
            current_path_val = buffer.data();
        }
    }

    std::vector<std::wstring> path_tokens = split_path(current_path_val);
    std::vector<std::wstring> new_path_tokens;
    bool found = false;

    for (const auto& token : path_tokens) {
        if (path_equals(token, exe_dir)) {
            found = true;
        } else {
            new_path_tokens.push_back(token);
        }
    }

    if (!found) {
        std::cout << "click directory is not in your User PATH environment variable.\n";
        RegCloseKey(hKey);
        return true;
    }

    std::wstring new_path_val = join_path(new_path_tokens);

    lRes = RegSetValueExW(
        hKey,
        L"Path",
        0,
        REG_EXPAND_SZ,
        reinterpret_cast<const BYTE*>(new_path_val.c_str()),
        static_cast<DWORD>((new_path_val.length() + 1) * sizeof(wchar_t))
    );

    RegCloseKey(hKey);

    if (lRes != ERROR_SUCCESS) {
        std::cout << "Error: Failed to update Environment Registry. Code: " << lRes << "\n";
        return false;
    }

    // Broadcast system settings change to notify open shells
    DWORD_PTR dwResult;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Environment"), SMTO_ABORTIFHUNG, 2000, &dwResult);

    std::cout << "Successfully removed click directory from User PATH environment variable.\n";
    return true;
}

// Open file with default application (simulates double click)
bool open_file_default(const std::wstring& file_path) {
    std::wstring abs_path = get_absolute_path(file_path);
    
    DWORD attribs = GetFileAttributesW(abs_path.c_str());
    if (attribs == INVALID_FILE_ATTRIBUTES) {
        std::cout << "Error: File or directory does not exist: " << to_utf8(abs_path) << "\n";
        return false;
    }

    HINSTANCE hInst = ShellExecuteW(
        NULL,
        L"open",
        abs_path.c_str(),
        NULL,
        NULL,
        SW_SHOWNORMAL
    );

    if (reinterpret_cast<INT_PTR>(hInst) <= 32) {
        std::cout << "Error: Failed to open file: " << to_utf8(abs_path) << " (ShellExecute error code: " << reinterpret_cast<INT_PTR>(hInst) << ")\n";
        return false;
    }

    std::cout << "Opening " << to_utf8(abs_path) << "...\n";
    return true;
}

// Direct Application Specification: Open file with the specified application
bool open_file_direct(const std::wstring& app_path, const std::wstring& file_path) {
    std::wstring abs_app_path = get_absolute_path(app_path);
    std::wstring abs_file_path = get_absolute_path(file_path);

    DWORD app_attribs = GetFileAttributesW(abs_app_path.c_str());
    if (app_attribs == INVALID_FILE_ATTRIBUTES || (app_attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        std::cout << "Error: Application executable does not exist or is a directory: " << to_utf8(abs_app_path) << "\n";
        return false;
    }

    DWORD file_attribs = GetFileAttributesW(abs_file_path.c_str());
    if (file_attribs == INVALID_FILE_ATTRIBUTES) {
        std::cout << "Error: File does not exist: " << to_utf8(abs_file_path) << "\n";
        return false;
    }

    std::wstring params = L"\"" + abs_file_path + L"\"";

    HINSTANCE hInst = ShellExecuteW(
        NULL,
        L"open",
        abs_app_path.c_str(),
        params.c_str(),
        NULL,
        SW_SHOWNORMAL
    );

    if (reinterpret_cast<INT_PTR>(hInst) <= 32) {
        std::cout << "Error: Failed to run application: " << to_utf8(abs_app_path) << " (ShellExecute error code: " << reinterpret_cast<INT_PTR>(hInst) << ")\n";
        return false;
    }

    std::cout << "Opening " << to_utf8(abs_file_path) << " with " << to_utf8(abs_app_path) << "...\n";
    return true;
}

// Indirect Application Specification: Open file with application associated with another extension
bool open_file_indirect(const std::wstring& ext, const std::wstring& file_path) {
    std::wstring normalized_ext = ext;
    if (normalized_ext[0] != L'.') {
        normalized_ext = L"." + normalized_ext;
    }

    std::wstring assoc_app = get_associated_app(normalized_ext);
    if (assoc_app.empty()) {
        std::cout << "Error: No associated application found in registry for extension: " << to_utf8(normalized_ext) << "\n";
        return false;
    }

    return open_file_direct(assoc_app, file_path);
}

void print_help() {
    std::cout << "click.exe - Windows CLI File Opener\n";
    std::cout << "Mimics GUI double-click to open files using registered applications.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  click <file_path>                   - Open file with its default application\n";
    std::cout << "  click -a <app_path> <file_path>     - Direct specify: Open file with the specified application\n";
    std::cout << "  click --app <app_path> <file_path>\n";
    std::cout << "  click -e <extension> <file_path>    - Indirect specify: Open file with the application associated\n";
    std::cout << "  click --ext <extension> <file_path>   with the specified extension (e.g., txt, pdf)\n";
    std::cout << "  click --install                     - Add the directory of click.exe to the User PATH\n";
    std::cout << "  click --uninstall                   - Remove the directory of click.exe from the User PATH\n";
    std::cout << "  click -h / --help                   - Show this help message\n";
}

int wmain(int argc, wchar_t* argv[]) {
    // Set console output code page to UTF-8
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        print_help();
        return 1;
    }

    std::wstring arg1 = argv[1];

    if (arg1 == L"-h" || arg1 == L"--help") {
        print_help();
        return 0;
    }
    else if (arg1 == L"--install") {
        return install_click() ? 0 : 1;
    }
    else if (arg1 == L"--uninstall") {
        return uninstall_click() ? 0 : 1;
    }
    else if (arg1 == L"-a" || arg1 == L"--app") {
        if (argc < 4) {
            std::cout << "Error: Missing application path or file path.\n";
            std::cout << "Usage: click -a <app_path> <file_path>\n";
            return 1;
        }
        std::wstring app_path = argv[2];
        std::wstring file_path = argv[3];
        return open_file_direct(app_path, file_path) ? 0 : 1;
    }
    else if (arg1 == L"-e" || arg1 == L"--ext") {
        if (argc < 4) {
            std::cout << "Error: Missing extension or file path.\n";
            std::cout << "Usage: click -e <extension> <file_path>\n";
            return 1;
        }
        std::wstring ext = argv[2];
        std::wstring file_path = argv[3];
        return open_file_indirect(ext, file_path) ? 0 : 1;
    }
    else {
        // Default execution: Open file with default association
        return open_file_default(arg1) ? 0 : 1;
    }
}
