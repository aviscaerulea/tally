@README.md
@SPEC.md

## ビルド環境

- **OS**: Windows 11
- **コンパイラ**: Visual Studio 2019 以降（VC++ ツールチェーン）
- **ビルドツール**: [Task](https://taskfile.dev/)
- **実行環境**: PowerShell 7 (pwsh)

## ビルド方法

### 前提条件

- Visual Studio の C++ ビルドツールがインストールされていること
- [Task](https://taskfile.dev/) がインストールされていること

### ビルド手順

PowerShell (pwsh) で以下を実行:

```powershell
# Visual Studio の開発環境をロード
Enable-VSDev

# デバッグビルド
task build

# リリースビルド（zip パッケージ付き）
task release

# ビルド成果物のクリーニング
task clean
```

### 成果物

- デバッグ: `out/tally.exe`
- リリース: `out/tally-1.0.0-x64.zip`

## 動作確認

### 基本動作確認

```powershell
# Web 会議アプリなしで実行
.\out\tally.exe
# 期待結果: "idle" と表示、$LASTEXITCODE == 1

# Web 会議アプリ（Teams, Zoom 等）を起動してカメラとマイクを ON
.\out\tally.exe
# 期待結果: "meeting" と表示、$LASTEXITCODE == 0

# 詳細情報を表示
.\out\tally.exe --verbose
# 使用中のアプリとデバイスが stderr に出力される
```

### テストケース

1. **アイドル状態**: カメラもマイクも使用していない → `idle` (exit 1)
2. **カメラのみ**: カメラのみ使用中 → `meeting` (exit 0)
3. **マイクのみ**: マイクのみ使用中 → `meeting` (exit 0)
4. **ミーティング中**: カメラとマイク両方使用中 → `meeting` (exit 0)

## デバッグ

レジストリの状態を直接確認する場合:

```powershell
# マイクの使用状況
reg query "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone" /s

# カメラの使用状況
reg query "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\webcam" /s
```

`LastUsedTimeStop` が `0x0` のエントリがあれば、そのアプリがデバイスを使用中。

## 依存関係

- Windows API (Registry API, WASAPI)
- C++ 標準ライブラリ
- advapi32.lib (レジストリアクセス用)
- ole32.lib (COM/WASAPI 用)

