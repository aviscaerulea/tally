# tally
[![日本語](https://img.shields.io/badge/lang-日本語-red)](README.md)
[![English](https://img.shields.io/badge/lang-English-blue)](README.en.md)
[![Release](https://img.shields.io/github/v/release/aviscaerulea/tally)](https://github.com/aviscaerulea/tally/releases/latest)
[![License](https://img.shields.io/github/license/aviscaerulea/tally)](LICENSE)
[![Build](https://github.com/aviscaerulea/tally/actions/workflows/release.yml/badge.svg)](https://github.com/aviscaerulea/tally/actions/workflows/release.yml)

Windows のプライバシー管理レジストリ（CapabilityAccessManager）を監視し、マイクまたはカメラが使用中かを判定して Web 会議中かどうかを検出する CLI ツールです。

```bash
$ tally
meeting
$ echo $?
0
```

## 機能

- 軽量・高速：レジストリ読み取りを主体に判定し、プロセス監視やポーリングを行わない
- プライバシー情報源：Windows が公式に管理している使用状況を参照する
- スクリプト連携：終了コードと標準出力で結果を返すため、他ツールから簡単に呼び出せる

## インストール

### 動作要件

- Windows 10/11

### 手順

#### リリースの zip から

[Releases](https://github.com/aviscaerulea/tally/releases/latest) から最新の zip をダウンロードしてください。任意のフォルダへ展開すると、`tally.exe` をそのまま実行できます。

#### Scoop から

[Scoop](https://scoop.sh/) でインストールできます。

```powershell
scoop bucket add aviscaerulea https://github.com/aviscaerulea/scoop-bucket
scoop install tally
```

## 使い方

```bash
# 基本的な使用
tally

# 詳細情報を表示（使用中のアプリとデバイス）
tally --verbose

# ヘルプ
tally --help
```

標準出力に `meeting` または `idle` を出力します。

| 終了コード | 意味 |
| --- | --- |
| `0` | Web 会議中（マイクまたはカメラが使用中） |
| `1` | アイドル状態（どちらも使用中ではない） |
| `2` | エラー |

PowerShell からは終了コードで判定できます。

```powershell
tally
if ($LASTEXITCODE -eq 0) {
    Write-Host "Meeting in progress"
} else {
    Write-Host "Idle"
}
```

## 制限事項

- Windows 専用（CapabilityAccessManager は Windows 10/11 の機能）
- マイクかカメラが使用中なら Web 会議中と判定するため、音声録音のみでも `meeting` になる
- 仮想オーディオデバイスは CapabilityAccessManager に現れない場合があり、WASAPI 検出で補完する
- 排他モードのオーディオストリームは WASAPI 検出でも捕捉できない場合がある
- プライバシー設定でアプリのデバイスアクセスを無効にしている場合、正しく検出できない
