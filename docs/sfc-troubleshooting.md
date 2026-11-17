# SFCのUSB列挙失敗：トラブルシュート引き継ぎ

最終整理日：2026-11-17。対象はFNK0099 Rev 1.1 / N16R8と、UIAPduino内蔵のUSB-C化SFCコントローラーです。別チームが現在の状態から調査を再開するための資料です。[各試行の詳細履歴](history/sfc-troubleshooting-2026-11-17.md)と[公開証跡一覧](evidence/README.md)を併用してください。

## 現在の結論・実機状態

**SFCを挿したまま起動すると列挙に失敗します。Hostを先に起動してから挿す（Host先行接続）と、1台目は元Host 1.1.0で認識し、12ボタン単独入力にも合格しました。根本原因は未確定です。** 1台目・C to Cで、元Host 1.1.0とArduino／IDF 4.4.7比較版を前後比較しました。接続済み起動は両版で9/9失敗し、Host先行接続は両版で各1/1認識しています。今回の条件では、成否はSDKの版ではなく接続順序に沿って分かれました。ただし、各成功は1回だけです。また、以前は旧ケーブル構成でHost先行接続も失敗しています。

| 対象 | 引き継ぎ時の状態 |
|---|---|
| Git | `feat/bolt-keyboard-mouse`。実機Hostのビルド元は `07e005a6488f2a2fb0b8f5fded6109040666f893`。主ソース・依存設定の変更なし |
| FNK0099 | `fnk0099-host`、ESP-IDF 5.5.0、HID 1.1.0。比較後に0x0〜0x5DFFFを退避から書き戻し、全範囲の読み戻しで一致を確認。実行時版表示 `07e005a` |
| SFC | 1台目を接続中。SFCファームは書き換えておらず、版は未特定。観測したUsageは公開v1.0.0の割り当てと一致し、SELECTはTab。2台目は公開v1.0.0（Tab固定）で、取り外し中 |
| 接続 | ユーザー報告によるC to C直結。向きは未確認だが、比較中は固定。FNK0099のUSB-UARTはMacへ接続し、給電とログ取得に使用 |
| 最終観測 | Host先行接続で認識。12ボタン24遷移を照合し、約280秒間、警告・エラー・切断なし。同じ版の接続済み起動は6/6失敗 |
| 監視 | UARTは閉じている。結果記録の区切りで停止 |

現在の実機はBolt用ファームではありません。K950・M750の基本入力・同時保持・RESET3回は別のBolt用ファームで確認済みで、抜去時診断は評価保留です。[Boltの状態](bolt-validation.md)を参照してください。過去のSFC合格は[2026-11-11の検証](validation.md)であり、現在の列挙失敗を解消した実績ではありません。

## 試した条件と結果

「1台目」「2台目」はユーザーが区別した物理個体です。書き換え前の実機BINは特定できていません。旧ケーブル構成は、後からUSB-C to USB-Aケーブル＋USB-A to USB-C変換アダプタと報告されました。各試行の接続細部をすべて独立に検証したわけではありません。

| 個体・SFC版 | Host・条件 | 観測結果 | 主な証跡 |
|---|---|---|---|
| 1台目・版未特定 | IDF 5.5.0 / HID 1.1.0、接続・接続済み起動・Macから戻す | CHECK_ADDRまたはCHECK_SHORT_DEV_DESCで失敗 | [初回](evidence/sfc-hid110-connect-attempt1-2026-11-17.json)、[起動](evidence/sfc-hid110-connect-attempt2-2026-11-17.json)、[戻した後](evidence/sfc-hid110-connect-attempt3-2026-11-17.json) |
| 1台目・版未特定 | Macへ直結 | `1209:d004`、Low-Speed、IF0マウス・IF1 Bootキーボードとして認識 | [Mac識別](evidence/sfc-hid110-mac-identity-2026-11-17.json) |
| 1台目・版未特定 | IDF 5.5.0 / HID 1.0.1の隔離比較版 | CHECK_SHORT_DEV_DESCで失敗。元1.1.0復元後も失敗 | [旧HID比較](evidence/sfc-hid101-comparison-2026-11-17.json) |
| 2台目・書き換え前 | 復元した元Host 1.1.0、最初の起動 | 認識成功、IF1受信開始760ms、8バイト全解放レポート770ms、エラー0 | [一度成功した記録](evidence/sfc-hid110-controller2-enumeration-2026-11-17.json) |
| 2台目・書き換え前 | 同じHost、UART再オープン時の起動／Host起動後接続 | いずれもCHECK_FULL_DEV_DESCで失敗 | [次の起動](evidence/sfc-hid110-controller2-startup-attempt2-2026-11-17.json)、[Host先行](evidence/sfc-hid110-controller2-host-first-2026-11-17.json) |
| 2台目・公開v1.0.0へ変更 | Macの書き込みモードでアップロード | パディング込み5,504バイトの全セクタ読み戻し一致、通常モード認識成功 | [SFC書き込み](evidence/sfc-controller2-v100-upload-2026-11-17.json) |
| 2台目・検証済みv1.0.0 | 元Host 1.1.0、Host起動後接続 | CHECK_ADDRで失敗 | [既知SFCでの接続](evidence/sfc-controller2-v100-hid110-host-first-2026-11-17.json) |
| 同上 | IDF 5.5.0 / HID 1.0.1、接続済み起動 | CHECK_SHORT_DEV_DESCで失敗。元1.1.0復元後も同じ失敗 | [既知参照構成](evidence/sfc-known-reference-pair-comparison-2026-11-17.json) |
| 同上 | Arduino / IDF 4.4.7 / HID 1.0.1のUSB専用比較版、接続済み起動 | CHECK_SHORT_DEV_DESCで失敗、受信0。比較後は元の構成へ復元 | [別SDK比較・復元](evidence/crane-game-usb-probe-comparison-2026-11-17.json) |
| 同上 | 元Host 1.1.0、C to C直結へ交換、接続済み起動 | 前回と同じ600ms・CHECK_SHORT_DEV_DESC・エラー3行 | [2台目のケーブル交換](evidence/sfc-controller2-c-to-c-startup-2026-11-17.json) |
| 1台目・版未特定、C to C | 元Host 1.1.0、接続済み起動×3（A）→比較後の復元Host×3（R） | 6/6失敗。CHECK_SHORT_DEV_DESC×5（600ms）、CHECK_ADDR×1（660ms） | [ABA比較](evidence/sfc-controller1-connection-order-comparison-2026-11-17.json) |
| 同上 | Arduino / IDF 4.4.7のUSB専用比較版、接続済み起動×3（B） | 3/3失敗。ログ時刻1,284msでCHECK_SHORT_DEV_DESC | 同上 |
| 同上 | 比較版、Host先行接続（B4） | 認識。IF0・IF1検出、受信35,110件、入力エラー0。右・B・左・上・START・Lの押下と解放を受信 | 同上 |
| 同上 | 復元Host 1.1.0、Host先行接続（R4） | 認識。12ボタン単独入力24遷移に合格、約280秒間エラー0 | [12ボタン記録](evidence/sfc-controller1-hid110-host-first-buttons-2026-11-17.json) |

1回の認識成功を安定認識とは扱いません。表全体は、異なる接続順序・個体・再起動条件を含みます。一変数だけを変えた厳密な実験は、1台目のABA比較（Host側だけを入れ替え）に限られます。Macでの認識成功も、FNK0099での接続・電気的条件を保証する結果ではありません。

Host先行接続が失敗した例もあります。2台目・旧ケーブル構成ではCHECK_FULL_DEV_DESCとCHECK_ADDR、1台目・旧ケーブル構成ではCHECK_ADDRで失敗しました。C to CでHost先行接続を試したのは、1台目の2回だけです。

## 参照プロジェクトから得た情報

関連プロジェクトは `crane_game` と `snes-controller-uiapduino` です。同じ作業PCでは既存プロジェクトを名前で探してください。公開資料にはユーザーの絶対パスを含めません。両プロジェクトのソースは今回変更していません。

### crane_game：1台目の動作実績

ユーザーは1台目SFCがFNK0099のOTGで動作したと報告しています。HEAD `9f2b6cd702c48511e68fdbef7bc18c50cb68b550`、SFC導入は `500ca81`。`Docs/FNK0099_SFC_USB操作.md` に受信開始・継続受信・方向とA/B・抜き差し復帰・受信エラー0が記録されています。成功時のSFC実機BIN・接続条件の完全一致は未確認です。

| 項目 | crane_gameの構成／比較版 | 本プロジェクトの元SFC Host |
|---|---|---|
| 実行環境 | Arduino-ESP32 2.0.17 / IDF 4.4.7 | ESP-IDF 5.5.0 |
| CPU / FreeRTOS tick | 240MHz / 1000Hz | 160MHz / 100Hz |
| HID | 同梱1.0.1 | 1.1.0。旧HID比較でもIDF本体は5.5.0のまま |
| ボード / Flash / PSRAM設定 | freenove_esp32_s3_wroom、8MB、qio_opi | esp32-s3-devkitc-1、16MB、DIO、PSRAM不使用 |
| タスク | Host core 0 priority 2、HID core 0 priority 4、入力priority 3 | Host core 0 priority 2、HID background priority 5、入力priority 4 |
| 記述子observer | 追加クライアントなし | 別クライアント・タスクあり |

同梱HID 1.0.1は従来比較版のドライバーとバイト単位で一致します。制御転送最大256バイト、balanced、debounce 250ms、reset hold/recovery 30ms、address recovery 10ms、割り込みLEVEL1、対象IF1とBoot protocolも共通です。

USB専用比較版は `src/fnk0099/usb_sfc.cpp`・`usb_sfc.h`・`sfc_input.h` と `lib/usb_host_hid/` を変更せずコピーし、UART状態表示だけを追加したものです。モーター・ネットワークのアプリ本体は含めていません。元の成功BINそのものではありません。[ソースハッシュ・設定比較](evidence/crane-game-usb-reference-review-2026-11-17.json)。

比較版は、最初に2台目・旧ケーブル構成の接続済み起動で試し、失敗しました。その後、1台目・C to Cで再比較しました。接続済み起動は元Hostと同じく失敗し、Host先行接続は元Hostと同じく認識しています。crane_gameの資料には、給電・ログにUSB-UART端子を使うことと、USBの抜き差しからの復帰が記載されています。一方、電源投入時にSFCを挿していたかどうかの記載はありません。

### snes-controller-uiapduino：SELECT切り替え版との差

参照時HEADは `b5c17f9`。変更 `67a0a71` は起動時のSELECT保持でTab／Backspaceを選ぶ機能で、公開v1.0.0はTab固定です。本HostはTab `0x2b` をSELECTへ変換し、Backspace `0x2a` は変換しません。ただし今回の失敗はキー解釈より前です。2台目をTab固定版へ書き換えても解消しなかったため、SELECT機能だけを原因とは絞れません。[ソース確認](evidence/sfc-controller-source-review-2026-11-17.json)。

公開v1.0.0参照BINはソース `fb06b04` に対応する配布物で5,496バイトです。2台目の旧BINは退避できていません。実機はv1.0.0になっていますが、相手プロジェクトのソース・ローカル最新BINは切り替え版のままです。

## 成果物と復元の範囲

| 用途 | サイズ | アプリBIN SHA-256 |
|---|---:|---|
| 現在の元SFC Host、`07e005a` / HID 1.1.0 | 270,256 | `aefa9acd1775727ec0009e85c1b8deb31669895a06fbd26827fb70f617446f6c` |
| 旧Host比較、`4348f79-hid101-ab` / HID 1.0.1 / IDF 5.5.0 | 269,728 | `5f046537ca18f720f52fbf0e5ad3904cc0769314564ee910cbe66ab596dcfc75` |
| Arduino／IDF 4.4.7 USB専用比較版 | 318,096 | `4ac0ccecb412406014816a83919b88512c19a80e9b267a048c1ecf26c9d8a7af` |
| 2台目SFCへ書いた公開v1.0.0 | 5,496 | `43a82eb36c7df4d298dd01f6b25460117fab0207c9206382b5eeaf92379a9079` |

Arduino比較前にFlash先頭64KiBを退避しました。比較版は4領域の書き込み検証に成功。比較後に先頭64KiB（offset `0x0`）と元Hostアプリ（`0x10000`）を戻し、2領域のハッシュ検証と先頭64KiBの読み戻し完全一致を確認しました。**Flash全体の退避・全バイト復元ではありません。** 復元後に `07e005a` とHost起動を確認し、その後のC to C試験では書き込みをしていません。

1台目のABA比較では、比較版が書き換える0x0〜0x5DFFF（385,024バイト）全体を退避しました。比較後はこの範囲を書き戻し、全範囲を読み戻して退避とのバイト一致を確認しています。この範囲には、元Hostの起動領域とアプリ全体が含まれます。比較版の書き込みは再ビルドせず、`dio / 80m / 8MB`、0x0・0x8000・0xe000・0x10000の4領域です。

以前のIDF版同士の比較はブートローダー `0x0`、パーティション `0x8000`、アプリ `0x10000` の3領域を復元済みです。`pio -t nobuild -t upload` のアプリだけの転送を、起動領域を含む復元と取り違えないでください。生成済み `flasher_args.json` のIDF出力名と実際のPlatformIO BIN名は異なるため、既存の補助スクリプトはoffsetとファイル名を対応づけています。

同じPCにのみ残る非公開資料（すべてGit対象外）：

| 場所 | 内容 |
|---|---|
| `.pio/build/fnk0099-host/` | 元Hostの保持済み `firmware.bin`・`bootloader.bin`・`partitions.bin`。再ビルド前にハッシュ確認・保全 |
| `.local/bolt-validation/sfc-hid101-baseline/` | 旧HID比較用プロジェクト・成果物 |
| `.local/bolt-validation/crane-usb-probe/` | USB専用Arduino比較プロジェクト、出典マニフェスト、成果物 |
| `.local/bolt-validation/controller-v1.0.0-reference.bin` | SFCへ書いた既知BIN |
| `.local/bolt-validation/crane-compare-original-prefix-20261117.bin` | 比較前の先頭64KiB退避。内容もハッシュも公開対象外 |
| `.local/bolt-validation/crane-compare-restored-prefix-readback-20261117.bin` | 復元後の先頭領域読み戻し |
| `.local/sfc-next-trial/controller1-crane-probe/` | 1台目ABA比較の手順書・`trial.py`（事前確認・退避・書き込み・復元・取得・集計）・条件記録・生ログ・0x0〜0x5DFFFの退避。退避の内容とハッシュは公開対象外 |
| `.local/bolt-validation/` 内のログ・JSON・補助Python | 生UART・転送ログ・解析状態・復元処理。ポートや固有情報を含むためそのまま公開しない |

新しいcloneには上記の非公開ファイルはありません。公開証跡の設定・ソースコミット・ハッシュを使って再構築し、過去の実機BINと同一とは未確認のまま扱ってください。再ビルドでは埋め込みGit版情報などでハッシュが変わる場合があります。

## 再開時の手順と判断の区切り

最初はファームウェアを書き換えず、接続個体・ケーブル・向き・接続順序を確認してから、条件を一つずつ記録してください。現時点で認識実績がある手順は、**SFCを抜いた状態でHostを起動し、`HOST_READY` を確認してから全解放で挿す**ことです。UARTを開くとFNK0099が再起動し、挿したままの起動条件に戻ります。そのため、認識後の試験は同じUARTセッションのまま続けてください。

未実施の候補は次のとおりです。いずれも根本原因の断定ではありません。

- 電源を完全に切った状態（USB-UARTを抜く）から、SFCを挿したまま起動する。
- Host先行接続での保持・同時押し・部分解放・切断復帰・物理RESET。
- 2台目でのC to C・Host先行接続。
- 旧ケーブル構成でのHost先行接続の再比較。
- 測定手段があれば、VBUS・信号の観測。

比較版を再び使う場合は、`.local/sfc-next-trial/controller1-crane-probe/README.md` の手順（退避→書き込み→復元→全範囲照合）を流用できます。

UARTポートはローカルで再確認し、例の `YOUR_UART_PORT` を置き換えます。115200 baud。既存のログ名を上書きせず、次の試行用ディレクトリへ保存します。

```sh
mkdir -p .local/sfc-next-trial
python3 tools/capture_uart.py --port YOUR_UART_PORT --seconds 20 --output .local/sfc-next-trial/startup.log > .local/sfc-next-trial/console.log 2>&1
```

Python環境にはpyserialが必要です。[環境とログ取得手順](build.md)を参照してください。`capture_uart.py` は生ログを標準出力にも表示するため、上の例では両方をGit対象外へ保存しています。UARTのオープンでFNK0099が再起動する場合があります。準備の起動を物理RESET試験として集計せず、成功した接続からボタン試験へ進む際は同じUARTを維持する監視方法を先に用意してください。

認識成功の確認点は `HOST_READY` だけでなく、次のすべてです。

- `1209:d004`、Low-Speed、IF1（subclass/protocol `1/1`）、Interrupt IN `0x82`、MPS 8、interval 10ms。
- `keyboard open`、`SET_PROTOCOL(BOOT)`、`SET_IDLE(0)`、`keyboard start` がすべて `ESP_OK`。
- 全解放レポート、`SFC mask=0x000`、警告・エラー・余分な再起動の有無。

認識が確認できた区切りで共有し、その後にB → Y → SELECT → START → 上 → 下 → 左 → 右 → A → X → L → Rの押下・解放24遷移を照合します。このうち12ボタン単独入力は、1台目のHost先行接続で合格しています。保持・部分解放・切断復帰は、さらに別の段階で、まだ新しい合格実績がありません。

ユーザーは、ビルド成功後の書き込み・検証を継続して行うことを許可しています。一方、判断が必要な場面と作業の区切りでは停止する方針です。1台目のABA比較では、新しいビルドはしていません。保持済みの比較版の書き込みと、退避からの復元だけを行いました。

公開前には実際のUARTポート、ユーザーの絶対パス、MAC、固有番号、認証情報、Flash退避を除外してください。USB標準属性・固定試験操作・ソース／ファームウェアのハッシュだけを選んで記録します。
