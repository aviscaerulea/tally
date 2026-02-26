Windows のプライバシー管理レジストリ（CapabilityAccessManager）を監視し、マイクまたはカメラが使用中かを判定して Web 会議中かどうかを検出する CLI ツール。

## 特徴

- **軽量・高速**: レジストリ読み取りのみで判定、プロセス監視不要
- **プライバシー情報源**: Windows が公式に管理している使用状況を参照
- **スクリプト連携**: 終了コードと標準出力で結果を返すため、他ツールから簡単に呼び出せる

## 使用方法

```bash
# 基本的な使用
tally

# 詳細情報を表示（使用中のアプリとデバイス）
tally --verbose

# ヘルプ
tally --help
```

### 出力

- **標準出力**: `meeting` または `idle`
- **終了コード**:
  - `0`: ミーティング中（マイクまたはカメラが使用中）
  - `1`: アイドル状態（どちらも使用中ではない）
  - `2`: エラー

### PowerShell からの使用例

```powershell
tally
if ($LASTEXITCODE -eq 0) {
    Write-Host "Meeting in progress"
} else {
    Write-Host "Idle"
}
```

## 環境条件

- **OS**: Windows 10/11
- **コンパイラ**: Visual Studio 2019 以降（C++ ビルドツール）
- **ビルドツール**: [Task](https://taskfile.dev/)
- **実行環境**: PowerShell 7 (pwsh)

## ビルド方法

PowerShell (pwsh) で:

```powershell
# VC++ 開発環境をロード
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
- リリース: `out/tally-1.1.0-x64.zip`

## 制限事項

- Windows 専用（CapabilityAccessManager は Windows 10/11 の機能）
- マイクまたはカメラのいずれかが使用中であればミーティング中と判定するため、音声録音のみの場合も `meeting` になる
- 仮想オーディオデバイスドライバは CapabilityAccessManager に記録されないことがあるが、WASAPI 補完検出で対応している（排他モードのストリームは検出できない場合あり）
- プライバシー設定でアプリのデバイスアクセスが無効化されている場合、正しく検出できない可能性がある

## 仕様

詳細な仕様は [SPEC.md](SPEC.md) を参照。

## ライセンス

[MIT License](LICENSE.txt)

