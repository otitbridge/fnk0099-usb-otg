# FNK0099 USB Host — HID・USB MSC検証

Freenove FNK0099（ESP32-S3-WROOM Board Lite）のUSB-OTG端子を使う、2種類の検証用ファームウェアです。USB-UART端子で給電・書き込み・ログ取得を行い、USB-OTG端子へ検証対象を接続します。

**Freenove FNK0099 USB-C OTG host tested with a USB HID keyboard and a USB MSC card reader.** Separate PlatformIO environments select the firmware. The HID setup was tested with a direct USB-C connection, without an additional power supply, hub, or board modification.

## ファームウェアを選ぶ

| 用途 | PlatformIO環境 | 確認済みの動作 | 操作手順・結果 |
|---|---|---|---|
| SFCコントローラーのHID入力 | `fnk0099-host`（既定） | 12ボタンのUART出力、保持・同時押し・切断復帰 | [HID操作](docs/hid-usage.md) / [HID検証結果](docs/validation.md) |
| USBカードリーダー経由のmicroSD読み書き | `fnk0099-msc` | 30バイト読み取り、31バイト・1MiB新規書き込み、Mac照合、再接続3回・RESET3回 | [MSC操作](docs/msc-usage.md) / [MSC検証結果](docs/msc-validation.md) |

環境ごとにファームを書き換えて使用します。環境指定のない `pio run` はHID用です。HIDとMSCを同時に処理する構成ではありません。

## 確認した機材

| 項目 | 構成 |
|---|---|
| Host | FNK0099 Rev 1.1、N16R8（16MB Flash / 8MB PSRAM） |
| 開発・照合PC | Mac、PlatformIO Core、USB-UART通信ケーブル |
| HID | UIAPduino V1.4内蔵SFC、USBキーボード版 v1.0.0（SELECT→Tab）、AT-PDCASTCC08 |
| MSC | カードリーダー `14cd:1212`、microSD 1GB、FAT16 |

PSRAMは使用していません。実績の範囲は上記の組合せです。HIDの30分連続稼働は保留、MSCの長時間稼働・電源断耐性・全容量書き込みは未検証です。

## 開始手順

1. FNK0099のUSB-UART端子をPCへ接続します。
2. [ビルド・書き込み手順](docs/build.md)で使用する環境を指定して書き込み、115200 baudでUARTを開きます。
3. USB-OTG端子に対象機器を接続し、上記の用途別手順で確認します。

MSCは認識時にマウントを試みます。ファイル作成はUARTの明示的なコマンドで行い、既存ファイルの上書き・自動フォーマットは行いません。取り外し前に `unmount` を実行し、`SAFE_TO_REMOVE` を確認します。

## 資料

- [ビルド・書き込み・ホストテスト・ログ取得](docs/build.md)
- [既知の制約・問題の切り分け](docs/known-issues.md)
- [公開証跡一覧](docs/evidence/README.md)
- [HIDの事前調査と設計方針](docs/design-notes.md)
- [MSC検証の過去の経過](docs/history/msc-validation-2026-11-17.md)
- [Freenove公式資料](https://docs.freenove.com/projects/fnk0099/en/latest/)

生ログ・機器情報・生成物はGit対象外の `.local/` に置きます。公開証跡には必要な技術情報と公開検証データだけを記録します。
