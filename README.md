# FNK0099 USB Host検証

Freenove FNK0099（ESP32-S3-WROOM Board Lite）から、UIAPduino内蔵SFCコントローラーをUSB HIDキーボードとして読み取る独立プロジェクトです。

**P1：Host単体起動確認は実機で合格しました（2026-11-11）。** [結果](docs/evidence/p1-result-2026-11-11.json)・[UARTログ](docs/evidence/p1-uart-2026-11-11.log)を保存しています。40秒の記録中に35秒間隔分の稼働ログを確認し、再起動・panic・watchdogは観測されませんでした。

現在の実装は **P3/P4：記述子照合・キーボード受信** です。1209:D004のキーボードIF1だけを開始し、8バイトレポートを12ボタンのマスクへ変換します。同一レポートの繰り返し出力は省略し、切断・受信エラー時はマスクを全解放します。無変化時間だけでキーを解放する処理はありません。

## 構成と確認状況

| 項目 | 根拠・状態 |
|---|---|
| FNK0099 Rev 1.1、N16R8 | ユーザーが実物表記を確認 |
| ESP32-S3、16MB Flash、8MB PSRAM | esptoolによる実機照合済み。チップrevision v0.2は基板Revとは別 |
| Mac接続 | USB-UARTのみ。115200 baudで起動ログ受信済み |
| OTG | AT-PDCASTCC08でSFCを直結。USBアドレス1にマウスIF0・キーボードIF1を検出 |
| C-C直結 | P2でUSB列挙・HIDインターフェース検出に成功。VBUS電圧・CC回路は未測定・未確認 |
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

## P2：直結確認

2026-11-11、ユーザーがAT-PDCASTCC08でSFCコントローラーを既設OTGへ直結し、UARTでマウスIF0（protocol=2）・キーボードIF1（protocol=1）の検出と、その後のHost稼働継続を確認しました。[結果](docs/evidence/p2-result-2026-11-11.json)・[UARTログ](docs/evidence/p2-direct-uart-2026-11-11.log)。

ログの `Unexpected device` はP1実装が機器接続を通知する文言で、列挙失敗を示しません。このP2時点ではVID/PIDやエンドポイント照合、入力受信は未実施でした。

## P3：記述子・初期レポート

2026-11-11、VID:PID=1209:D004、Low-Speed、キーボードIF1・EP82（Interrupt IN、8バイト、10ms）を実機で照合しました。SET_PROTOCOL(BOOT)・SET_IDLE(0)・受信開始はすべてESP_OKで、全解放の8バイトレポートを受信しました。[結果](docs/evidence/p3-result-2026-11-11.json)・[ログ](docs/evidence/p3-enumeration-2026-11-11.log)。

ホスト上の変換テストは `cc -Wall -Wextra -Werror tests/test_report.c -o /tmp/fnk-report-test` でビルドし、生成したテストを実行できます。12ボタン対応、同時入力、部分・全解放、不正レポートを検査します。これは実機ボタン試験とは区別します。

## P4：12ボタン単独入力

全12ボタンを指定順に識別し、各押下後に全解放へ戻ることをUARTで確認しました。実測押下時間は220〜300msで、長押し試験とは区別します。[結果](docs/evidence/p4-result-2026-11-11.json)・[ログ](docs/evidence/p4-buttons-2026-11-11.log)。

## P5：長押し・部分解放

左32.81秒、右36.09秒の保持と解放を確認しました（合格基準は各30秒以上）。右＋Y＋Bを5.18秒保持し、Bだけ離した後も右＋Yが6.77秒継続、最後に全解放へ戻りました。[結果](docs/evidence/p5-hold-result-2026-11-11.json)・[ログ](docs/evidence/p5-hold-2026-11-11.log)。

初回の切断試験ではマスクの全解放は成功しましたが、HID後処理エラーにより再接続を認識できませんでした。[失敗記録](docs/evidence/p5-disconnect-failed-2026-11-11.json)。切断コールバック内でインターフェースを閉じるよう修正し、書き込みと初期認識を確認済みです。修正後の再試験では、切断時の全解放、インターフェースclose=ESP_OK、再列挙、右・Bの押下と解放に成功しました。[再試験結果](docs/evidence/p5-disconnect-retry-2026-11-11.json)。抜去時の `EP command error: ESP_ERR_INVALID_STATE` は残っていますが、この試験では復帰を妨げませんでした。発生原因は未確定として記録します。

## P6：再接続の繰り返し

全解放での抜き差し3回すべてで、再認識・右とBの押下と解放に成功しました。接続中の空きヒープは各回360,668バイトでした。[結果](docs/evidence/p6-reconnect-result-2026-11-11.json)。抜去時のEPエラーは継続し、1回はURBエラーと不正レポートのログも出ましたが、マスクは0のまま、次の接続・入力は成功しました。

## P6：Host再起動と確認範囲

P6のRESET再起動3回はすべて成功しました。毎回Host初期化・SFC再認識・右とBの押下と解放を確認し、この3回にはエラーログはありません。[結果](docs/evidence/p6-restart-result-2026-11-11.json)・[ログ](docs/evidence/p6-restart-2026-11-11.log)。

今回のP1〜P6の実施した機能試験は成功しました。ただし、12ボタン・長押し試験は切断処理修正前、再接続・再起動試験は修正版で実施しています。抜去時のEP／URB診断ログの原因は未確定です。完全電源断・USB-C両面の組合せ・長時間耐久・接続時のボタン保持・電圧測定は今回の確認範囲に含みません。

## 残るEP／URBログの重要度

**軽微（暫定）** と評価します。修正後に残るログはUSB抜去時に観測され、切断時の全解放、再接続3回、RESET再起動3回で操作復帰を確認しています。修正後の試験では入力残留・クラッシュ・復帰不能は確認していません。原因が解明済み、または無害と保証されたという意味ではありません。

修正前の「再接続できない問題」は重大な機能不具合として区別します。通常接続中にもログが発生する、入力停止・入力残留・クラッシュ・復帰不能を伴う場合は、重大へ評価を見直します。

電源やCCを原因と決めつけず、Host起動→デバイス起動→列挙→クラス要求→入力受信の順で切り分けます。既存UIAPduinoプロジェクトは変更しません。

## 根拠資料

- [検証計画・引き継ぎ書](FNK0099_USB_Host_Handoff_v02_retry.md)
- [Freenove FNK0099公式資料](https://docs.freenove.com/projects/fnk0099/en/latest/)
- [ESP-IDF公式HID Host例（v5.5.2）](https://github.com/espressif/esp-idf/tree/v5.5.2/examples/peripherals/usb/host/hid)
- [ESP-IDF 5.5 USB Host API](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/usb_host.html)

引き継ぎ書のケーブル表記「セリア、AT-PDCASTCC08」は既存資料の型番情報と混在しています。AT-PDCASTCC08とセリアEM-69Aは区別し、今回のP2ではAT-PDCASTCC08を使用したとユーザーが確認しました。
