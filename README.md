# FNK0099 USB Host検証

Freenove FNK0099（ESP32-S3-WROOM Board Lite）から、UIAPduino内蔵SFCコントローラーをUSB HIDキーボードとして読み取る独立プロジェクトです。

**P1：Host単体起動確認は実機で合格しました（2026-11-11）。** [結果](docs/evidence/p1-result-2026-11-11.json)・[UARTログ](docs/evidence/p1-uart-2026-11-11.log)を保存しています。40秒の記録中に35秒間隔分の稼働ログを確認し、再起動・panic・watchdogは観測されませんでした。

現在の実装は **P1：Host単体起動確認用** です。USB HostライブラリとHIDドライバーを起動し、5秒ごとにUARTへ稼働ログを出します。入力の受信・12ボタン変換は次工程です。OTG端子にはまだ機器を接続しません。

## 構成と確認状況

| 項目 | 根拠・状態 |
|---|---|
| FNK0099 Rev 1.1、N16R8 | ユーザーが実物表記を確認 |
| ESP32-S3、16MB Flash、8MB PSRAM | esptoolによる実機照合済み。チップrevision v0.2は基板Revとは別 |
| Mac接続 | USB-UARTのみ。115200 baudで起動ログ受信済み |
| OTG | ユーザー報告では未接続 |
| VBUS供給・CC回路・C-C直結 | 未検証。Host初期化成功だけで給電・列挙成功とはしない |
| PSRAM | P1では使用しない |

## ビルド・書き込み

PlatformIO Coreを用います。プラットフォーム6.12.0、ESP-IDF 5.5.0、HIDドライバー1.0.1を固定しています。引き継ぎ書で参照したESP-IDF公式例は5.5.2ですが、既存PlatformIOとの対応を優先して5.5.0で検証します。

```sh
pio run -e fnk0099-host
pio device list
pio run -e fnk0099-host -t upload --upload-port <USB-UARTポート>
pio device monitor --port <USB-UARTポート> --baud 115200
```

`esp32-s3-devkitc-1`はビルド用の汎用定義として使い、Flash容量を16MBに上書きします。FNK0099と同じ基板という意味ではありません。USBコンソールは使わずUART0を使用します。

P1の合格条件は `usb_host_install: ESP_OK`、`hid_host_install: ESP_OK; HOST_READY` と、30秒以上の継続した `alive` 出力です。再起動ループ・panic・watchdogがないことも確認します。

## 続く検証

P2/P3では既設OTG端子へ追加給電・ハブ・改造なしでC-C直結し、VID:PID=1209:D004、キーボードIF1・EP82を確認します。その後P4/P5で12入力、長押し、同時押し、部分解放、切断時の全解放を確認します。現段階では未実施です。

電源やCCを原因と決めつけず、Host起動→デバイス起動→列挙→クラス要求→入力受信の順で切り分けます。既存UIAPduinoプロジェクトは変更しません。

## 根拠資料

- [検証計画・引き継ぎ書](FNK0099_USB_Host_Handoff_v02_retry.md)
- [Freenove FNK0099公式資料](https://docs.freenove.com/projects/fnk0099/en/latest/)
- [ESP-IDF公式HID Host例（v5.5.2）](https://github.com/espressif/esp-idf/tree/v5.5.2/examples/peripherals/usb/host/hid)
- [ESP-IDF 5.5 USB Host API](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/usb_host.html)

引き継ぎ書のケーブル表記「セリア、AT-PDCASTCC08」は既存資料の型番情報と混在しています。AT-PDCASTCC08とセリアEM-69Aは区別し、直結試験前に実際に使うケーブルを確認します。
