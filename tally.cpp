/*
 * tally - Web 会議検出ツール
 *
 * Windows のプライバシー管理レジストリ（CapabilityAccessManager）を監視し、
 * マイクまたはカメラが使用中かを判定して Web 会議中かどうかを検出する。
 *
 * 【終了コード】
 *   0: ミーティング中（マイクまたはカメラが使用中）
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
 *   1. レジストリ（CapabilityAccessManager）:
 *     HKCU\...\ConsentStore\microphone  - LastUsedTimeStop == 0 で使用中
 *     HKCU\...\ConsentStore\webcam      - LastUsedTimeStop == 0 で使用中
 *
 *   2. WASAPI（マイクのみ、レジストリで検出できなかった場合の補完）:
 *     キャプチャデバイスのオーディオセッション状態が Active なら使用中
 *     仮想オーディオデバイス経由のマイク使用を補完検出する
 */

#include <windows.h>
#include <iostream>
#include <string>
#include <mmdeviceapi.h>    // IMMDeviceEnumerator, IMMDeviceCollection
#include <audiopolicy.h>    // IAudioSessionManager2, IAudioSessionEnumerator

// COMオブジェクトの安全な解放
template<class T>
void SafeRelease(T** pp) {
    if (*pp) {
        (*pp)->Release();
        *pp = nullptr;
    }
}

// レジストリキー配下のサブキーを列挙し、LastUsedTimeStop == 0 のエントリを検出
// prefix は verbose 出力時のアプリ名の接頭辞（例: "NonPackaged\\"）
bool CheckSubKeysForUsage(const std::string& keyPath, const std::string& deviceType,
                          const std::string& prefix, bool verbose) {
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    bool inUse = false;
    char subKeyName[256];
    DWORD subKeyNameSize;

    for (DWORD index = 0; ; index++) {
        subKeyNameSize = sizeof(subKeyName);
        result = RegEnumKeyExA(hKey, index, subKeyName, &subKeyNameSize, nullptr, nullptr, nullptr, nullptr);
        if (result != ERROR_SUCCESS) break;

        // 直下列挙時は "NonPackaged" をスキップ（別途処理する）
        if (prefix.empty() && strcmp(subKeyName, "NonPackaged") == 0) {
            continue;
        }

        HKEY hSubKey;
        std::string subKeyPath = keyPath + "\\" + subKeyName;
        result = RegOpenKeyExA(HKEY_CURRENT_USER, subKeyPath.c_str(), 0, KEY_READ, &hSubKey);
        if (result != ERROR_SUCCESS) continue;

        DWORD64 lastUsedTimeStop = 0;
        DWORD dataSize = sizeof(lastUsedTimeStop);
        DWORD dataType;
        result = RegQueryValueExA(hSubKey, "LastUsedTimeStop", nullptr, &dataType,
                                  reinterpret_cast<LPBYTE>(&lastUsedTimeStop), &dataSize);

        if (result == ERROR_SUCCESS && dataType == REG_QWORD && lastUsedTimeStop == 0) {
            inUse = true;
            if (verbose) {
                std::cerr << "Device in use: " << deviceType << " by " << prefix << subKeyName << std::endl;
            }
        }

        RegCloseKey(hSubKey);
    }

    RegCloseKey(hKey);
    return inUse;
}

// デバイスタイプ別の使用状況をチェック（レジストリベース）
bool IsDeviceInUse(const std::string& deviceType, bool verbose) {
    std::string basePath = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\" + deviceType;

    // 直下のサブキーを列挙（UWP アプリ等）
    bool inUse = CheckSubKeysForUsage(basePath, deviceType, "", verbose);

    // NonPackaged 配下のサブキーを列挙（Win32 デスクトップアプリ）
    if (CheckSubKeysForUsage(basePath + "\\NonPackaged", deviceType, "NonPackaged\\", verbose)) {
        inUse = true;
    }

    return inUse;
}

// WASAPIによるマイクキャプチャセッションの使用状況をチェック（レジストリ検出の補完）
// 仮想オーディオデバイス経由の使用を検出するために使用する
bool IsMicInUseWasapi(bool verbose) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // RPC_E_CHANGED_MODE: 別モードで初期化済み。CoUninitialize を呼んではいけない
    if (hr == RPC_E_CHANGED_MODE) {
        if (verbose) {
            std::cerr << "WASAPI: COM already initialized in different mode" << std::endl;
        }
        return false;
    }
    if (FAILED(hr)) {
        if (verbose) {
            std::cerr << "WASAPI: COM initialization failed" << std::endl;
        }
        return false;
    }

    bool found = false;
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDeviceCollection* pCollection = nullptr;

    // デバイス列挙子の生成
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        if (verbose) {
            std::cerr << "WASAPI: Failed to create device enumerator" << std::endl;
        }
        goto cleanup;
    }

    // アクティブなキャプチャデバイスの列挙
    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        if (verbose) {
            std::cerr << "WASAPI: Failed to enumerate capture endpoints" << std::endl;
        }
        goto cleanup;
    }

    UINT deviceCount;
    pCollection->GetCount(&deviceCount);

    for (UINT i = 0; i < deviceCount && !found; i++) {
        IMMDevice* pDevice = nullptr;
        hr = pCollection->Item(i, &pDevice);
        if (FAILED(hr)) continue;

        // セッションマネージャを取得
        IAudioSessionManager2* pSessionMgr = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                               nullptr, (void**)&pSessionMgr);
        if (FAILED(hr)) {
            SafeRelease(&pDevice);
            continue;
        }

        // セッション一覧を取得
        IAudioSessionEnumerator* pSessionEnum = nullptr;
        hr = pSessionMgr->GetSessionEnumerator(&pSessionEnum);
        if (FAILED(hr)) {
            SafeRelease(&pSessionMgr);
            SafeRelease(&pDevice);
            continue;
        }

        int sessionCount = 0;
        pSessionEnum->GetCount(&sessionCount);

        for (int s = 0; s < sessionCount && !found; s++) {
            IAudioSessionControl* pCtrl = nullptr;
            hr = pSessionEnum->GetSession(s, &pCtrl);
            if (FAILED(hr)) continue;

            // IAudioSessionControl2 を取得してシステムサウンドセッションを除外
            IAudioSessionControl2* pCtrl2 = nullptr;
            pCtrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pCtrl2);
            if (pCtrl2 && pCtrl2->IsSystemSoundsSession() == S_OK) {
                SafeRelease(&pCtrl2);
                SafeRelease(&pCtrl);
                continue;
            }

            // セッションがアクティブか確認
            AudioSessionState state;
            hr = pCtrl->GetState(&state);
            if (SUCCEEDED(hr) && state == AudioSessionStateActive) {
                found = true;
                if (verbose) {
                    DWORD pid = 0;
                    if (pCtrl2) pCtrl2->GetProcessId(&pid);
                    std::cerr << "WASAPI: Active capture session detected (PID: " << pid << ")" << std::endl;
                }
            }

            SafeRelease(&pCtrl2);
            SafeRelease(&pCtrl);
        }

        SafeRelease(&pSessionEnum);
        SafeRelease(&pSessionMgr);
        SafeRelease(&pDevice);
    }

cleanup:
    SafeRelease(&pCollection);
    SafeRelease(&pEnumerator);
    CoUninitialize();
    return found;
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
            std::cout << "  0: Meeting in progress (microphone OR camera in use)" << std::endl;
            std::cout << "  1: Idle (not in meeting)" << std::endl;
            std::cout << "  2: Error" << std::endl;
            std::cout << std::endl;
            std::cout << "Output:" << std::endl;
            std::cout << "  stdout: \"meeting\" or \"idle\"" << std::endl;
            std::cout << "  stderr: verbose information (with --verbose)" << std::endl;
            return 0;
        }
    }

    // マイクとカメラの使用状況をチェック（レジストリ）
    bool micInUse = IsDeviceInUse("microphone", verbose);
    bool camInUse = IsDeviceInUse("webcam", verbose);

    // レジストリで検出できなかった場合、WASAPIで補完チェック
    if (!micInUse) {
        micInUse = IsMicInUseWasapi(verbose);
    }

    if (verbose) {
        std::cerr << "Microphone in use: " << (micInUse ? "YES" : "NO") << std::endl;
        std::cerr << "Camera in use: " << (camInUse ? "YES" : "NO") << std::endl;
    }

    // いずれかが使用中ならミーティング中と判定
    if (micInUse || camInUse) {
        std::cout << "meeting" << std::endl;
        return 0;
    }

    std::cout << "idle" << std::endl;
    return 1;
}
