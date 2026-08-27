# tally
[![日本語](https://img.shields.io/badge/lang-日本語-red)](README.md)
[![English](https://img.shields.io/badge/lang-English-blue)](README.en.md)
[![Release](https://img.shields.io/github/v/release/aviscaerulea/tally)](https://github.com/aviscaerulea/tally/releases/latest)
[![License](https://img.shields.io/github/license/aviscaerulea/tally)](LICENSE)
[![Build](https://github.com/aviscaerulea/tally/actions/workflows/release.yml/badge.svg)](https://github.com/aviscaerulea/tally/actions/workflows/release.yml)

A CLI tool that watches Windows' privacy management registry (CapabilityAccessManager) and detects whether you are in a web meeting by checking if the microphone or camera is in use.

```bash
$ tally
meeting
$ echo $?
0
```

## Features

- Lightweight and fast: judges mainly from registry reads, with no process monitoring or polling
- Privacy-aware data source: refers to the usage state officially managed by Windows
- Script-friendly: returns results via exit code and stdout, so other tools can call it easily

## Installation

### Requirements

- Windows 10/11

### Steps

#### From the release ZIP

Download the latest zip from [Releases](https://github.com/aviscaerulea/tally/releases/latest). Extracting it to any folder lets you run `tally.exe` directly.

#### From Scoop

You can install it with [Scoop](https://scoop.sh/).

```powershell
scoop bucket add aviscaerulea https://github.com/aviscaerulea/scoop-bucket
scoop install tally
```

## Usage

```bash
# Basic usage
tally

# Show detailed information (apps and devices in use)
tally --verbose

# Help
tally --help
```

It prints `meeting` or `idle` to stdout.

| Exit code | Meaning |
| --- | --- |
| `0` | In a meeting (microphone or camera in use) |
| `1` | Idle (neither in use) |
| `2` | Error |

From PowerShell, you can judge the result from the exit code.

```powershell
tally
if ($LASTEXITCODE -eq 0) {
    Write-Host "Meeting in progress"
} else {
    Write-Host "Idle"
}
```

## Limitations

- Windows only (CapabilityAccessManager is a Windows 10/11 feature)
- Judges a meeting is in progress if either the microphone or camera is in use, so it also reports `meeting` for audio-only recording
- Virtual audio devices may not appear in CapabilityAccessManager; WASAPI-based detection covers this case
- Exclusive-mode audio streams may still be undetectable even with WASAPI detection
- Detection may be inaccurate when a privacy setting disables an app's device access
