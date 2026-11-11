# FNK0099 USB Host：事前調査と設計方針

FNK0099でUIAPduino内蔵SFCコントローラーの入力を受信するために参照した資料と、設計上の判断、実機検証から得られた考察をまとめています。調査・検証日は2026-11-11です。

現在の接続手順は[README](../README.md)、実機試験の詳細は[検証結果](validation.md)、未確認項目は[既知の制約](known-issues.md)を参照してください。

## 1. 対象と設計方針

Freenove FNK0099（ESP32-S3-WROOM Board Lite）の既設USB-OTG端子に、UIAPduino V1.4内蔵SFCコントローラーを接続します。USB HIDキーボードの状態を12ボタンのマスクへ変換し、USB-UART経由で出力します。

- 既設USB-OTG端子へのUSB-C直結を優先し、追加回路や基板改造は実測で必要性を確認してから検討する。
- USB-UARTを給電・書き込み・ログ取得に使い、USB-OTGをコントローラー接続に使う。
- UIAPduinoの通常版v1.0.0（SELECT→Tab）を使用し、入力受信はFNK0099側の独立したファームウェアで実装する。
- Wi-Fi送信、ESP-NOW、ゲームエミュレーター、USBゲームパッド化は対象に含めない。

実機試験ではFNK0099 Rev 1.1／N16R8とAT-PDCASTCC08を使用し、追加給電・USBハブ・基板改造なしで列挙と入力受信を確認しました。VBUS電圧やCC回路の実測、USB-C規格適合性の評価は行っていません。

## 2. USB-C接続に関する事前調査

### 公開資料の適用範囲

事前調査では、FNK0099 Liteの既設OTG端子からC-C直結でHIDキーボードを給電・列挙したと特定できる公開実績は確認できませんでした。同系統のFreenove製品の報告は参考になりますが、基板や接続経路の違いを区別する必要があります。

以下は事前調査時点の資料整理です。本プロジェクトの実機結果は[検証結果](validation.md)に記録しています。

| 出典 | 公開された事実 | **FNK0099 Liteへ一般化できない部分** |
|---|---|---|
| [EspUsbHost：実機確認済みボード一覧](https://github.com/tanakamasayuki/EspUsbHost/blob/main/docs/tested-devices.ja.md) | 「Freenove ESP32-S3-WROOM Board」はOTG Type-Cから給電可能と記載 | **Lite / FNK0099の明記がない** |
| [EspUsbHost：USB Host開発ガイド](https://github.com/tanakamasayuki/EspUsbHost/blob/main/docs/usb-host-guide.ja.md) | VBUSの実測を推奨。Hostには接続デバイスへの給電が必要 | この一般論のみでFNK0099の給電不能は示せない |
| [Qiita：Freenove ESP32-S3 WROOM N8R8でUSBキーボード入力](https://qiita.com/Mobu_Kyoto/items/762a2467fc58b558952a)（2025-06-21公開） | ESP32-S3でワイヤレスキーボードのUSBレシーバー受信に成功。GPIO19=D−、GPIO20=D＋、5V=VBUSを使用 | **USB-Aメス端子を外付け**。Liteの型番・既設C-C端子経由の給電実績ではない |
| [note：ESP-IDF HID Hostの実装記事](https://note.com/ndenki/n/n7a8d171f2791)（2026-03-24） | ESP-IDFのHID Host例を解説。Freenove Board Liteを入門用に紹介 | **記事内の商品の紹介 ≠ FNK0099のC-C直結の実機証明** |
| [Freenove FNK0099公式製品ページ](https://store.freenove.com/products/fnk0099) | LiteとしてUSB-OTG機能、8/16MB Flashの選択肢を明示 | **USB HostとしてのVBUS供給・CC回路の詳細までは明示しない** |
| [Freenove FNK0099公式リポジトリ](https://github.com/Freenove/Freenove_ESP32_S3_WROOM_Board_Lite) | Lite専用のピン配置・例・資料を公開 | 公開フォルダにUSB-C電源・CC回路を確定できる完全な回路図があるとは確認していない |
| [betaflight/bridge](https://github.com/betaflight/bridge) | Freenove ESP32-S3-WROOM N8R8 v1.1をUSB Hostとして利用する構成が公開されている | **FNK0099 Liteと同一基板か不明**。USB-C電源の内部回路を特定する証拠ではない |


`FNK0085/0085B`と`FNK0099`は別製品です。N8R8／N16R8はメモリ構成を示すため、この表記だけで電源回路が同一とは判断できません。

### 電源・CC回路の評価

USB-CのSource／Sink役割はCC端子のRp／Rdなどで判定されます。接続動作の確認と回路の規格適合性は別の評価項目です。

- VBUSを測定する場合は、接続状態、Hostの初期化状態、測定位置を併記する。無接続時の0Vだけで給電不良とは判断しない。
- 給電、USB列挙、HIDクラス要求、入力解析を分けて切り分ける。
- 未確認のままVBUSへ外部電源を注入したり、CC抵抗や基板パターンを変更したりしない。
- 通電中のUSB-C端子内への直接プロービングは避け、測定には端子を分離できる治具を使用する。

## 3. USB Host実装の選択

ESP-IDFのUSB HostとHID Hostドライバーを採用しました。接続イベント、記述子、クラス要求、受信レポートを個別に記録できることを重視しています。

事前調査では[ESP-IDF v5.5.2のHID Host例](https://github.com/espressif/esp-idf/tree/v5.5.2/examples/peripherals/usb/host/hid)を参照しました。実装・実機検証に使用した版はESP-IDF **5.5.0**、`espressif/usb_host_hid` **1.0.1**です。再現手順と依存関係は[ビルド手順](build.md)にまとめています。

代替候補として[Arduino-ESP32向けEspUsbHost](https://github.com/tanakamasayuki/EspUsbHost)も調査しました。v2はv1とAPI互換ではないため、採用する場合は使用版のREADMEとサンプルに合わせた実装が必要です。

## 4. 入力処理の設計

対象は`1209:D004`の複合HIDデバイスです。マウスのInterface 0を入力解析から除外し、キーボードのInterface 1を受信します。記述子ではInterrupt INエンドポイントは`0x82`、レポートはReport IDなしの8バイト、ポーリング間隔は10msです。

1. `report[0]`を修飾キー、`report[1]`を予約、`report[2..7]`を最大6個のHID Usage IDとして扱う。
2. 各レポートから現在の12ボタンマスクを再構成し、前回とのXORで変化を求める。
3. USB切断や入力エラー時は内部マスクを全解放に戻す。
4. ASCII変換やOSのキーリピートに依存せず、HID Usage IDを使う。

ボタンとビットの対応は[README](../README.md)、実装は[src/sfc_report.h](../src/sfc_report.h)を参照してください。元のファームウェアが送信するのは最大6入力であり、Host側で7個目以降を復元することはできません。

`SET_PROTOCOL`／`SET_IDLE`などのクラス要求への応答は、記述子の宣言とは別に確認する必要があります。本構成では初期化要求の成功と入力受信を実機ログで確認しています。切断時のHIDハンドル解放の修正経緯は[検証結果](validation.md)に記録しています。

読み取り周期とUSBポーリング間隔だけから、エンドツーエンドの入力遅延は算定しません。接続時保持やサスペンド・復帰など、未試験の動作は[既知の制約](known-issues.md)にまとめています。

## 5. 検証記録の方針

Host起動、直結、列挙、単独入力、複合入力、再接続の順に確認します。各段階の結果と実施条件は[検証結果](validation.md)、ログと集計は[証跡一覧](evidence/README.md)に集約しています。

記録には基板Rev、モジュール、給電経路、ケーブル、使用ソフトウェアの版、VID/PID、インターフェース、Raw HIDレポート、変換後のマスクを含めます。公開資料の記述、実機ログ、操作者の報告、推測、未試験事項を区別します。

## 6. 検証結果からの考察

### USB-C直結と追加回路の必要性

**確認結果**：FNK0099 Rev 1.1／N16R8、UIAPduino V1.4のキーボード版v1.0.0、AT-PDCASTCC08の組み合わせで、既設USB-OTG端子への直結による列挙・入力受信に成功しました。給電はMacからFNK0099のUSB-UART経由で行い、追加電源やハブ、基板改造は使用していません。

**考察**：今回の構成では、入力受信のための追加給電回路やCC回路の変更は不要と判断できます。事前調査で公開実績が見つからないことだけを理由に、回路変更を前提とする必要はありませんでした。ただし、これは使用した組み合わせの動作確認であり、別の基板Rev・電源・ケーブル・USBデバイスへの一般化やUSB-C規格適合性の保証はできません。

### キーボード入力からのボタン状態の復元

**確認結果**：修正版で12ボタンそれぞれの押下・解放、左右30秒以上の保持、右＋Y＋Bの同時入力、Bだけを離した後の右＋Y保持、最後の全解放を確認しました。

**考察**：現在のHIDレポートから毎回マスクを再構成する方式で、単独入力と試験した複合入力を復元できています。この受信目的には、既存のUIAPduinoキーボード版を変更せず利用できます。12ボタンの確認は各ボタンの識別を意味し、12個同時押しを意味しません。最大6入力という送信側の制約は残ります。

### 再接続処理とソフトウェア側の切り分け

**確認結果**：初期実装では切断時のマスク解放はできましたが、HID後処理エラーにより再接続を認識できませんでした。HIDインターフェースのcloseを切断コールバック内で完了するよう修正し、再接続後の入力復帰、再接続3回、RESET再起動3回を確認しました。

**考察**：今回の再接続不良は、Host側のソフトウェア修正で改善しました。接続問題の調査では、電源や配線に加えて、使用ドライバーが要求するリソース解放のタイミングも確認する必要があります。接続時の処理だけでなく、押下中の切断から全解放・再列挙・入力復帰までを一連の試験として扱うことが有効でした。

### 残る課題と評価の範囲

抜去時のEP／URB診断ログは原因未確定です。修正後の試験では入力残留・クラッシュ・復帰不能を認めないため、**軽微（暫定）**と評価しています。正常接続中の発生や機能停止を伴う場合は評価を見直します。修正済みの再接続不良と、残存する診断ログは区別します。

**30分連続稼働試験は保留・未完了**です。今回の機能試験だけで長時間の安定性までは結論づけません。接続時保持、サスペンド復帰、VBUS電圧、CC回路の適合性も未確認です。給電を切って戻す試験は成功しましたが、UART再オープン時のリセット線の影響は分離していません。

詳細な測定値と試験条件は[検証結果](validation.md)、各ログは[証跡一覧](evidence/README.md)、重要度を見直す条件は[既知の制約](known-issues.md)を参照してください。

## 7. 参照資料

### 対象ハードと既存デバイス

- Freenove FNK0099公式リポジトリ：<https://github.com/Freenove/Freenove_ESP32_S3_WROOM_Board_Lite>
- Freenove FNK0099公式製品ページ：<https://store.freenove.com/products/fnk0099>
- Freenove FNK0099公式ドキュメント：<https://docs.freenove.com/projects/fnk0099/en/latest/>
- 自作UIAPduino版：<https://github.com/otitbridge/snes-controller-uiapduino>
- UIAPduino HID用コアのUSBディスクリプタ：<https://github.com/tarosay/arduino_core_ch32/blob/9471a150c9837c9055ef62ef759a1a593eddf1b1/cores/arduino/usb_config.h>

### 公開された実機報告・Host実装

- EspUsbHost 実機確認一覧：<https://github.com/tanakamasayuki/EspUsbHost/blob/main/docs/tested-devices.ja.md>
- EspUsbHost 日本語Host開発ガイド：<https://github.com/tanakamasayuki/EspUsbHost/blob/main/docs/usb-host-guide.ja.md>
- Qiita（FreenoveのUSB-A外付けキーボード検証）：<https://qiita.com/Mobu_Kyoto/items/762a2467fc58b558952a>
- note（ESP-IDF HID Host入門・Freenove Lite紹介）：<https://note.com/ndenki/n/n7a8d171f2791>
- Betaflight USB Host Wi-Fi Bridge（Freenove N8R8、Lite同定不可）：<https://github.com/betaflight/bridge>
- ESP-IDF公式HID Host例（v5.5.2）：<https://github.com/espressif/esp-idf/tree/v5.5.2/examples/peripherals/usb/host/hid>

