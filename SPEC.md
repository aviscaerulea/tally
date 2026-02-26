## 概要

Windows の CapabilityAccessManager レジストリを監視し、マイクまたはカメラが使用中かを判定することで Web 会議中かどうかを検出する CLI ツール。

## 目的

他のスクリプト（PowerShell, Python 等）から呼び出し、現在 Web 会議中かどうかを判定する情報を提供する。

## 技術仕様

### 検出方式

マイクの検出には2つの方式を組み合わせる。カメラはレジストリのみ。

#### 方式1: レジストリ（CapabilityAccessManager）

Windows のプライバシー管理機能が記録している各アプリのデバイス使用状況をレジストリから読み取る。

#### レジストリパス

```
HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone
HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\webcam
```

#### レジストリ構造

各デバイスタイプ（microphone, webcam）配下に、アプリごとのサブキーが存在:

```
ConsentStore\microphone\
  ├── Microsoft.Windows.Cortana_cw5n1h2txyewy  ← UWP/パッケージアプリ
  │   └── LastUsedTimeStop: REG_QWORD
  ├── NonPackaged\                              ← Win32 デスクトップアプリ
  │   ├── C:#Program Files#Microsoft#Teams#current#Teams.exe
  │   │   └── LastUsedTimeStop: REG_QWORD
  │   ├── C:#Users#...#AppData#Local#Zoom#bin#Zoom.exe
  │   │   └── LastUsedTimeStop: REG_QWORD
  │   └── ...
  └── ...
```

#### LastUsedTimeStop の意味

- **値が 0**: そのアプリが現在デバイスを使用中
- **値が 0 以外**: 最後に使用を停止した時刻（FILETIME 形式）

#### 方式2: WASAPI（マイクのみ、レジストリ検出の補完）

仮想オーディオデバイスドライバはカーネルレベルでデバイスを制御するため、CapabilityAccessManager に記録されない場合がある。この場合の補完として、WASAPI のオーディオセッション API でキャプチャデバイスのアクティブセッションを直接確認する。

- レジストリでマイク使用中と判定された場合は WASAPI チェックをスキップ
- `eCapture` 方向の全アクティブデバイスのオーディオセッションを列挙
- システムサウンドセッション（`IsSystemSoundsSession()`）は除外
- セッション状態が `AudioSessionStateActive` なら使用中と判定

### 判定ロジック

1. `microphone` 配下の全サブキー（直下 + `NonPackaged\` 配下）を列挙
2. 各サブキーの `LastUsedTimeStop` を読み取る
3. いずれかが `0` の場合、マイクが使用中と判定
4. マイクがレジストリで未検出の場合、WASAPI でキャプチャセッションを確認
5. `webcam` でレジストリ検出を実行
6. **マイク OR カメラのいずれかが使用中** → ミーティング中
7. 両方未使用 → アイドル状態

### インターフェース仕様

#### コマンドライン引数

- `--verbose`: 使用中のアプリ名とデバイス情報を stderr に出力（デバッグ用）
- `--help`, `-h`: ヘルプメッセージを表示して終了

#### 標準出力

- `meeting`: ミーティング中（マイクまたはカメラが使用中）
- `idle`: アイドル状態（どちらも使用中ではない）

#### 終了コード

| コード | 意味 | 説明 |
|--------|------|------|
| `0` | ミーティング中 | マイクまたはカメラが使用中 |
| `1` | アイドル | どちらも使用中ではない |
| `2` | エラー | 現在未使用（将来の拡張用） |

#### stderr 出力（--verbose 時のみ）

```
Device in use: microphone by NonPackaged\C:#Program Files#Microsoft#Teams#current#Teams.exe
Device in use: webcam by NonPackaged\C:#Program Files#Microsoft#Teams#current#Teams.exe
Microphone in use: YES
Camera in use: YES
```

### 実装詳細

#### 関数構成

##### `bool CheckSubKeysForUsage(const std::string& keyPath, const std::string& deviceType, const std::string& prefix, bool verbose)`

指定されたレジストリキー配下のサブキーを列挙し、`LastUsedTimeStop == 0` のエントリを検出する内部ヘルパー。

##### `bool IsDeviceInUse(const std::string& deviceType, bool verbose)`

指定されたデバイスタイプが現在使用中かを判定する。

**引数**:
- `deviceType`: `"microphone"` または `"webcam"`
- `verbose`: 詳細情報を stderr に出力するか

**戻り値**:
- `true`: デバイスが使用中
- `false`: デバイスは未使用

**処理フロー**:
1. `CheckSubKeysForUsage` でルート直下のサブキーを列挙（`NonPackaged` を除く）
2. `CheckSubKeysForUsage` で `NonPackaged` 配下のサブキーを列挙

##### `bool IsMicInUseWasapi(bool verbose)`

WASAPI でマイクキャプチャセッションの使用状況をチェックする。レジストリで検出できなかった場合の補完として呼び出される。

**処理フロー**:
1. `CoInitializeEx` で COM を初期化
2. `IMMDeviceEnumerator` でアクティブなキャプチャデバイスを列挙
3. 各デバイスの `IAudioSessionManager2` からセッション一覧を取得
4. システムサウンドセッションを除外し、`AudioSessionStateActive` のセッションがあれば使用中と判定
5. verbose モード時は使用中セッションの PID を stderr に出力

##### `int main(int argc, char* argv[])`

メインエントリポイント。

**処理フロー**:
1. コマンドライン引数をパース（`--verbose`, `--help`）
2. `IsDeviceInUse("microphone", verbose)` を呼び出し
3. マイクが未検出の場合、`IsMicInUseWasapi(verbose)` で補完チェック
4. `IsDeviceInUse("webcam", verbose)` を呼び出し
5. いずれかが `true` の場合:
   - stdout に `"meeting"` を出力
   - 終了コード `0` で終了
5. 両方が `false` の場合:
   - stdout に `"idle"` を出力
   - 終了コード `1` で終了

### エラーハンドリング

現在の実装では以下のエラーケースを想定:

1. レジストリキーが開けない場合
   - verbose モード時に stderr に警告を出力
   - デバイス未使用として扱う（`false` を返す）
2. レジストリ値の読み取りに失敗した場合
   - そのサブキーをスキップして次へ進む

将来的に必要に応じて終了コード `2` でエラーを返すよう拡張可能。

### 制限事項

- Windows 専用（CapabilityAccessManager は Windows 10/11 の機能）
- マイクまたはカメラのいずれかが使用中の場合にミーティング中と判定するため、会議以外（例: 音声録音のみ）でも `meeting` になる場合がある
- 仮想オーディオデバイスドライバはカーネルレベルでデバイスを制御するため、CapabilityAccessManager に使用中として記録されないことがある。WASAPI 補完検出により改善されているが、排他モードのストリームは WASAPI セッションとして現れない場合がある
- プライバシー設定でアプリのデバイスアクセスが無効化されている場合、正しく検出できない可能性がある

### パフォーマンス

- レジストリ読み取りのみのため高速（通常 < 10ms）
- プロセス監視やポーリング不要
- CPU 使用率: ほぼゼロ

## テストシナリオ

### ケース 1: アイドル状態

**前提条件**: カメラもマイクも使用していない

**実行**:
```powershell
.\tally.exe
```

**期待結果**:
- stdout: `idle`
- 終了コード: `1`

### ケース 2: カメラのみ使用中

**前提条件**: カメラアプリを起動してマイクは OFF

**実行**:
```powershell
.\tally.exe
```

**期待結果**:
- stdout: `meeting`
- 終了コード: `0`

### ケース 3: マイクのみ使用中

**前提条件**: 音声録音アプリを起動してカメラは OFF

**実行**:
```powershell
.\tally.exe
```

**期待結果**:
- stdout: `meeting`
- 終了コード: `0`

### ケース 4: ミーティング中

**前提条件**: Teams/Zoom 等でカメラとマイクの両方を ON

**実行**:
```powershell
.\tally.exe
```

**期待結果**:
- stdout: `meeting`
- 終了コード: `0`

### ケース 5: Verbose モード

**前提条件**: Teams でカメラとマイクの両方を ON

**実行**:
```powershell
.\tally.exe --verbose
```

**期待結果**:
- stdout: `meeting`
- stderr: 使用中のアプリとデバイス情報が出力される
  ```
  Device in use: microphone by NonPackaged\C:#Program Files#Microsoft#Teams#current#Teams.exe
  Device in use: webcam by NonPackaged\C:#Program Files#Microsoft#Teams#current#Teams.exe
  Microphone in use: YES
  Camera in use: YES
  ```
- 終了コード: `0`

## 将来の拡張可能性

- `--microphone-only`, `--camera-only` オプションで片方のみの検出
- JSON 出力モード（`--json`）でスクリプトからのパースを容易に
- 定期的なポーリング機能（`--watch` オプション）
- 使用中アプリ名のフィルタリング（特定アプリのみ検出）

