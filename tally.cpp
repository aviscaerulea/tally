/*
 * tally - Web 会議検出ツール
 *
 * Windows のプライバシー管理レジストリ（CapabilityAccessManager）を監視し、
 * マイクとカメラが同時に使用中かを判定して Web 会議中かどうかを検出する。
 *
 * 【終了コード】
 *   0: ミーティング中（マイクとカメラの両方が使用中）
 *   1: アイドル状態（両方使用中ではない）
 *   2: エラー
 *
 * 【標準出力】
 *   "meeting" または "idle"
 *
 * 【オプション】
 *   --verbose: 使用中のアプリとデバイスを stderr に出力
 *   --help: ヘルプを表示
 *
 * 【検出方式】
 *   レジストリパス:
 *     HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone
 *     HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\webcam
 *
 *   LastUsedTimeStop == 0 のアプリがそのデバイスを現在使用中と判定
 */

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

// デバイスタイプ別の使用状況をチェック
bool IsDeviceInUse(const std::string& deviceType, bool verbose) {
    std::string basePath = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\" + deviceType;
    HKEY hKey;
    bool inUse = false;

    // デバイスタイプのルートキーを開く
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, basePath.c_str(), 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        if (verbose) {
            std::cerr << "Failed to open registry key: " << basePath << " (Error: " << result << ")" << std::endl;
        }
        return false;
    }

    // 直下のサブキーを列挙（UWP アプリ等）
    DWORD index = 0;
    char subKeyName[256];
    DWORD subKeyNameSize;

    while (true) {
        subKeyNameSize = sizeof(subKeyName);
        result = RegEnumKeyExA(hKey, index, subKeyName, &subKeyNameSize, nullptr, nullptr, nullptr, nullptr);

        if (result == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (result != ERROR_SUCCESS) {
            break;
        }

        index++;

        // "NonPackaged" は後で別途処理
        if (strcmp(subKeyName, "NonPackaged") == 0) {
            continue;
        }

        // サブキーを開いて LastUsedTimeStop を読み取る
        std::string subKeyPath = basePath + "\\" + subKeyName;
        HKEY hSubKey;
        result = RegOpenKeyExA(HKEY_CURRENT_USER, subKeyPath.c_str(), 0, KEY_READ, &hSubKey);
        if (result == ERROR_SUCCESS) {
            DWORD64 lastUsedTimeStop = 0;
            DWORD dataSize = sizeof(lastUsedTimeStop);
            DWORD dataType;

            result = RegQueryValueExA(hSubKey, "LastUsedTimeStop", nullptr, &dataType,
                                     reinterpret_cast<LPBYTE>(&lastUsedTimeStop), &dataSize);

            if (result == ERROR_SUCCESS && dataType == REG_QWORD && lastUsedTimeStop == 0) {
                inUse = true;
                if (verbose) {
                    std::cerr << "Device in use: " << deviceType << " by " << subKeyName << std::endl;
                }
            }

            RegCloseKey(hSubKey);
        }
    }

    // NonPackaged 配下のサブキーを列挙（Win32 デスクトップアプリ）
    std::string nonPackagedPath = basePath + "\\NonPackaged";
    HKEY hNonPackagedKey;
    result = RegOpenKeyExA(HKEY_CURRENT_USER, nonPackagedPath.c_str(), 0, KEY_READ, &hNonPackagedKey);

    if (result == ERROR_SUCCESS) {
        index = 0;
        while (true) {
            subKeyNameSize = sizeof(subKeyName);
            result = RegEnumKeyExA(hNonPackagedKey, index, subKeyName, &subKeyNameSize, nullptr, nullptr, nullptr, nullptr);

            if (result == ERROR_NO_MORE_ITEMS) {
                break;
            }
            if (result != ERROR_SUCCESS) {
                break;
            }

            index++;

            // サブキーを開いて LastUsedTimeStop を読み取る
            std::string appKeyPath = nonPackagedPath + "\\" + subKeyName;
            HKEY hAppKey;
            result = RegOpenKeyExA(HKEY_CURRENT_USER, appKeyPath.c_str(), 0, KEY_READ, &hAppKey);
            if (result == ERROR_SUCCESS) {
                DWORD64 lastUsedTimeStop = 0;
                DWORD dataSize = sizeof(lastUsedTimeStop);
                DWORD dataType;

                result = RegQueryValueExA(hAppKey, "LastUsedTimeStop", nullptr, &dataType,
                                         reinterpret_cast<LPBYTE>(&lastUsedTimeStop), &dataSize);

                if (result == ERROR_SUCCESS && dataType == REG_QWORD && lastUsedTimeStop == 0) {
                    inUse = true;
                    if (verbose) {
                        std::cerr << "Device in use: " << deviceType << " by NonPackaged\\" << subKeyName << std::endl;
                    }
                }

                RegCloseKey(hAppKey);
            }
        }

        RegCloseKey(hNonPackagedKey);
    }

    RegCloseKey(hKey);
    return inUse;
}

int main(int argc, char* argv[]) {
    bool verbose = false;

    // コマンドライン引数パース
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--verbose") {
            verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "tally - Web meeting detector" << std::endl;
            std::cout << std::endl;
            std::cout << "Usage: tally [--verbose] [--help]" << std::endl;
            std::cout << std::endl;
            std::cout << "Exit codes:" << std::endl;
            std::cout << "  0: Meeting in progress (microphone AND camera in use)" << std::endl;
            std::cout << "  1: Idle (not in meeting)" << std::endl;
            std::cout << "  2: Error" << std::endl;
            std::cout << std::endl;
            std::cout << "Output:" << std::endl;
            std::cout << "  stdout: \"meeting\" or \"idle\"" << std::endl;
            std::cout << "  stderr: verbose information (with --verbose)" << std::endl;
            return 0;
        }
    }

    // マイクとカメラの使用状況をチェック
    bool micInUse = IsDeviceInUse("microphone", verbose);
    bool camInUse = IsDeviceInUse("webcam", verbose);

    if (verbose) {
        std::cerr << "Microphone in use: " << (micInUse ? "YES" : "NO") << std::endl;
        std::cerr << "Camera in use: " << (camInUse ? "YES" : "NO") << std::endl;
    }

    // 両方使用中の場合のみミーティング中と判定
    if (micInUse && camInUse) {
        std::cout << "meeting" << std::endl;
        return 0;
    } else {
        std::cout << "idle" << std::endl;
        return 1;
    }
}
