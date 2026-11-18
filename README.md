# FNK0099 USB Host — HID・USB MSC検証

Freenove FNK0099（ESP32-S3-WROOM Board Lite）のUSB-OTG端子を使う、用途別の検証用ファームウェアです。USB-UART端子で給電・書き込み・ログ取得を行い、USB-OTG端子へ検証対象を接続します。

**Freenove FNK0099 USB-C OTG host tested with a USB HID keyboard and a USB MSC card reader.** Separate PlatformIO environments select the firmware. The HID setup was tested with a direct USB-C connection, without an additional power supply, hub, or board modification.

## ファームウェアを選ぶ

| 用途 | PlatformIO環境 | 確認済みの動作 | 操作手順・結果 |
|---|---|---|---|
| SFCコントローラーのHID入力 | `fnk0099-host`（既定） | 従来HID 1.0.1で12ボタン・保持・切断復帰を確認。現行HID 1.1.0はHost先行接続で12ボタン単独入力のみ確認。SFCを挿したままの起動は、最初の列挙を遅らせる試作で改善 | [HID操作](docs/hid-usage.md) / [HID検証結果](docs/validation.md) |
| Bolt経由のキーボード・マウス（開発中） | `fnk0099-hid` | 基本入力・同時保持・RESET3回の復帰を確認。再接続時の診断ログを評価中 | [Bolt対応の手順・検証記録](docs/bolt-validation.md) |
| USBカードリーダー経由のmicroSD読み書き | `fnk0099-msc` | 30バイト読み取り、31バイト・1MiB新規書き込み、Mac照合、再接続3回・RESET3回 | [MSC操作](docs/msc-usage.md) / [MSC検証結果](docs/msc-validation.md) |

Bolt対応は別ブランチで開発中です。既存のSFC/MSC試験実績とは区別します。

SFCの現行版の回帰確認は途中です。1台目・C to C直結での結果は次のとおりです。

- 主ブランチのHostでSFCを挿したまま起動すると、USB列挙に失敗しがちです（UARTリセットによる起動は0/8、電源を切ってからの起動は2/8）。
- Hostを起動してから挿すと、Host 1.1.0で認識し、12ボタン単独入力に合格しました。
- 原因は、最初の列挙を始めるのが早すぎることでした。試作版で最初の列挙を約1秒遅らせると、SFCを挿したままの起動でも10/10で認識しました。試作はローカルブランチ `proto/sfc-port-recovery` にあり、主ブランチには未統合です。

遅延版では、1台目が回帰項目（12ボタン、保持、同時押しと一部解放、切断からの復帰、物理RESET×3、Host先行接続）に合格しました。確認できているのは1台目とC to Cケーブルだけで、試行回数も少数です。実機には現在、試作の遅延版を書き込んであります。[別チーム向け引き継ぎ資料](docs/sfc-troubleshooting.md)に、条件比較・成果物・再開手順をまとめています。

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
- [SFC列挙失敗の引き継ぎ・再開手順](docs/sfc-troubleshooting.md)
- [既知の制約・問題の切り分け](docs/known-issues.md)
- [公開証跡一覧](docs/evidence/README.md)
- [HIDの事前調査と設計方針](docs/design-notes.md)
- [MSC検証の過去の経過](docs/history/msc-validation-2026-11-17.md)
- [Freenove公式資料](https://docs.freenove.com/projects/fnk0099/en/latest/)

生ログ・機器情報・生成物はGit対象外の `.local/` に置きます。公開証跡には必要な技術情報と公開検証データだけを記録します。
