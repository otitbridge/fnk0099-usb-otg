# ビルド・書き込み・ログ取得

## 前提と固定バージョン

プロジェクトを取得し、`platformio.ini` があるディレクトリで操作します。PlatformIO Coreの `pio` が実行できる環境と、初回の依存取得にインターネット接続が必要です。実機で確認した開発PCはMacです。

| 項目 | 採用版 |
|---|---|
| PlatformIO platform | espressif32 6.12.0 |
| ESP-IDF | 5.5.0 |
| espressif/usb_host_hid | 1.0.1 |

`platformio.ini`、`src/idf_component.yml`、`dependencies.lock` で固定しています。初期調査の参照例はESP-IDF 5.5.2ですが、実装・検証は5.5.0です。

ビルド用の汎用定義 `esp32-s3-devkitc-1` に対してFlashを16MBに設定しています。FNK0099とDevKitCが同一基板という意味ではありません。UART0をコンソールに使用し、USBコンソールとPSRAMは無効です。

## ビルドと書き込み

FNK0099のUSB-UART端子をPCへ接続します。USB-OTG端子はコントローラー接続用です。

```sh
pio run -e fnk0099-host
pio device list
```

一覧でUSB-UARTポートを特定し、次の `YOUR_UART_PORT` をそのポート名へ置き換えます。別の開発ボードと取り違えないでください。

```sh
pio run -e fnk0099-host -t upload --upload-port YOUR_UART_PORT
pio device monitor --port YOUR_UART_PORT --baud 115200
```

書き込み時は他のシリアルモニターを終了します。今回の実機ではUART経由の自動リセットで書き込めました。書き込みのハッシュ検証成功を確認してください。モニターはCtrl+Cで終了します。

## 受信確認

SFCを全解放でUSB-OTGへ直結し、次を確認します。

- `usb_host_install: ESP_OK` と `hid_host_install: ESP_OK; HOST_READY`
- `VID:PID=1209:d004`、Low-Speed、IF1、EP `0x82`、8バイト、10ms
- `SET_PROTOCOL(BOOT)`、`SET_IDLE(0)`、`keyboard start` がESP_OK
- 最初の全解放レポートと、ボタン操作時の `RAW` / `SFC mask`

マウスIF0は入力処理の対象外です。クラス要求が失敗すると受信開始を見送り、その結果をログに残します。無変化時間だけを理由に保持中の入力を解放する処理はありません。

## ログをファイルへ保存

`tools/capture_uart.py` はPython 3とpyserialを使用します。専用仮想環境を使う場合の例です。

```sh
python3 -m venv .venv
.venv/bin/python -m pip install pyserial
mkdir -p .local
.venv/bin/python tools/capture_uart.py --port YOUR_UART_PORT --seconds 120 --output .local/uart.log
```

指定秒数で終了します。USB-UART自体を抜くと、このスクリプトは自動再接続しません。UARTを開く際に基板が再起動する場合があります。電源断試験では取得開始時の影響と実際の電源断を区別してください。

## 変換ロジックのテスト

Cコンパイラーのある環境で実行します。

```sh
mkdir -p .local
cc -Wall -Wextra -Werror tests/test_report.c -o .local/test-report
.local/test-report
```

12入力の対応、同時押し、部分・全解放、不正レポートを検査します。実機のUSB通信試験を代替するものではありません。

## 実機試験の順序

1. B → Y → SELECT → START → 上 → 下 → 左 → 右 → A → X → L → Rを単独押下・解放。
2. 左右それぞれ35秒以上を目安に保持して解放。ログ上で各30秒以上を確認。
3. 右＋Y＋Bを保持し、Bだけ離して右＋Yが継続することを確認、全解放。
4. 右を押したままOTG側を抜き、マスク0を確認。抜いた状態で右を離し、再接続後に右・Bを確認。
5. 全解放で再接続3回と、両USBを保持したRESET再起動3回を確認。

各試験はUARTログと照合します。30分試験は現在保留のため、上記を終えても連続稼働の合格とはしません。

[READMEへ戻る](../README.md)
