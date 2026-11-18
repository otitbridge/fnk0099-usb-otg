# SFCのUSB列挙失敗：トラブルシュート引き継ぎ

最終整理日：2026-11-18。対象はFNK0099 Rev 1.1 / N16R8と、UIAPduino内蔵のUSB-C化SFCコントローラーです。別チームが現在の状態から調査を再開するための資料です。各試行の詳細は、[2026-11-17の履歴](history/sfc-troubleshooting-2026-11-17.md)・[2026-11-18の履歴](history/sfc-troubleshooting-2026-11-18.md)と[公開証跡一覧](evidence/README.md)を併用してください。

## 現在の結論・実機状態

**SFCを挿したまま起動したときの失敗は、Hostが最初の列挙を始めるのが早すぎることで起きていました。最初の列挙を約1秒遅らせた試作版の結果は次のとおりです。**
- 1台目・C to C：どちらの起動条件でも10/10で認識し、回帰項目（段階1〜3：12ボタン・保持・同時押しと一部解放・切断復帰・物理RESET×3・Host先行接続）にも合格した。
- 2台目：C to Cと旧ケーブル構成のどちらでも12/12で認識した。

根本の理由（起動後約0.6秒の時点でSFCの準備ができていない理由）は、まだ確定していません。対策は試作ブランチにあり、主ブランチには未統合です。

1台目・C to Cの版ごとの結果は次のとおりです。2台目の結果は、この表の下にまとめています。

| 版 | 最初の列挙 | UARTリセットで起動（SFCは給電されたまま） | 電源を完全に切ってから起動 | Host先行接続 |
|---|---|---|---|---|
| 現行 `07e005a`（主ブランチ） | 約0.6秒 | 0/8 | 2/8 | 1/1（12ボタン合格） |
| 試作 `rcv1-pp`（回復処理あり） | 約10ms遅い | 0/5（ポート電源の入れ直しを3回試しても失敗） | 8/8 | 1/1 |
| 試作 `rcv1-pp-d1000`（給電を1秒遅らせる） | 約1.5秒 | **5/5**（物理RESETも3/3） | **5/5** | 2/2（抜き差し・SFCなし起動後） |

- 起動後約0.6秒で最初の要求を送ると失敗しやすく、**一度失敗するとSFCはその後のバスリセットに応答しなくなります。** ルートポート電源の入れ直し（VBUSは切れない）では、回復しませんでした。
- IDF 4.4.7比較版での接続済み起動も0/3で失敗しています。SDKの版ではなく、タイミングの問題です。
- **2台目（公開v1.0.0）での遅延版の結果：**
  - C to C：UARTリセット起動3/3、電源断起動3/3
  - 旧ケーブル構成（C-Aケーブル＋A-C変換アダプタ）：UARTリセット起動3/3、電源断起動3/3

  どれも1台目と同じタイミングで認識しました（1,771msに機器、1,871msに受信開始）。1台目＋旧ケーブルは、つなぎ替えができないため試していません。
- 試行回数は少数です。必要な最小の遅延は測っていません。当日の現行Hostでの対照は、1台目・C to Cでしか取っていません。

| 対象 | 引き継ぎ時の状態 |
|---|---|
| Git | 主ブランチは `feat/bolt-keyboard-mouse`。実機Hostのビルド元は `07e005a6488f2a2fb0b8f5fded6109040666f893` で、主ソース・依存設定の変更はない。試作はローカルブランチ `proto/sfc-port-recovery`（`c77386c` 回復処理、`721f972` 遅延版env）で、未統合・未push |
| FNK0099 | **試作 `rcv1-pp-d1000` のアプリ**（SHA-256 `89256b2b…`）を0x10000に書き込んである。ブートローダーとパーティションは07e005aのまま。主ブランチのビルドではない。元に戻すには、退避を書き戻して全範囲を照合する（下記） |
| SFC | **2台目**（公開v1.0.0、Tab固定）を接続中で、認識している。1台目（SFCファームは書き換えておらず版は未特定。Usageは公開v1.0.0と一致し、SELECTはTab）は取り外し中 |
| 接続 | 2台目は**旧ケーブル構成**（USB-C to USB-Aケーブル＋USB-A to USB-C変換アダプタ、ユーザー報告）。C to Cケーブルは外してある。FNK0099のUSB-UARTはMacへ接続し、給電とログ取得に使用 |
| 最終観測 | 遅延版で、2台目・旧ケーブルの電源断起動3/3（1,771msに機器、1,871msに受信開始。右・Bを受信） |
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
| 同上 | 元Host 1.1.0。USB-UARTをMac側で抜いて15〜17秒後に挿し直す電源断起動×3（C1〜C3） | C2のみ認識（670msにデバイス、760msにkeyboard start、右・B受信）。C1・C3は入力なし、空きヒープ363,016 | [電源断起動](evidence/sfc-controller1-cold-power-on-2026-11-17.json) |
| 同上 | 同じ日の対照、UARTリセット起動（K1） | 600msにCHECK_SHORT_DEV_DESCで失敗 | 同上 |
| 同上（2026-11-18） | 元Host 1.1.0、UARTリセット起動（K0） | 660msにCHECK_ADDRで失敗 | [回復処理の試作](evidence/sfc-root-port-recovery-prototype-2026-11-18.json) |
| 同上 | 試作 `rcv1-pp`（ルートポート電源の入れ直し）、UARTリセット起動×5（T2） | 5/5で回復せず。電源オフで機器は毎回解放されたが、3回の再接続とも `CHECK_SHORT_DEV_DESC` で失敗 | 同上 |
| 同上 | 試作 `rcv1-pp`：SFCなし（T0）、Host先行接続（T1）、諦めた後の抜き差し（T4） | どれも正常。回復処理の誤動作なし | 同上 |
| 同上 | 試作 `rcv1-pp`、電源断起動×8（T3、Brcv） | 8/8が初回で認識。回復処理は動かない | [時刻比較](evidence/sfc-first-enumeration-timing-2026-11-18.json) |
| 同上 | 元Host 1.1.0、電源断起動×5（A07、同日の比較） | 1/5（A07-5で660msのCHECK_ADDRを捕捉） | 同上 |
| 同上 | 試作 `rcv1-pp-d1000`（1,155msに給電）、電源断起動×5・UARTリセット起動×5 | **10/10**。1,771msに機器、1,871msに受信開始 | 同上 |
| 同上 | 試作 `rcv1-pp-d1000`、回帰確認R1〜R4 | 12ボタン24遷移、左35.5秒・右33.4秒の保持、右＋Y＋BからBだけ離す、押したまま抜くと全解放して再接続後に復帰、物理RESET×3は3/3、SFCなしで起動後に挿して認識。抜いた瞬間のE行1件以外に異常なし | [回帰確認](evidence/sfc-delayed-enumeration-regression-2026-11-18.json) |
| 2台目・公開v1.0.0（2026-11-18） | 試作 `rcv1-pp-d1000`、C to C：UARTリセット起動×3・電源断起動×3 | 6/6で認識（1,771msに機器、1,871msに受信開始）。右・Bを受信 | [2台目・ケーブル別](evidence/sfc-delayed-enumeration-controller2-cables-2026-11-18.json) |
| 同上 | 試作 `rcv1-pp-d1000`、旧ケーブル構成：UARTリセット起動×3・電源断起動×3 | 6/6で認識。右・Bを受信（3回目でBが短く3回押下・解放された記録あり） | 同上 |

1回の認識成功を安定認識とは扱いません。表全体は、異なる接続順序・個体・再起動条件を含みます。一変数だけを変えた厳密な実験は、1台目のABA比較（Host側だけを入れ替え）に限られます。Macでの認識成功も、FNK0099での接続・電気的条件を保証する結果ではありません。

Host先行接続が失敗した例もあります。2台目・旧ケーブル構成ではCHECK_FULL_DEV_DESCとCHECK_ADDR、1台目・旧ケーブル構成ではCHECK_ADDRで失敗しました。C to CでHost先行接続を試したのは1台目だけで、計4回（11-17に2回、11-18に試作 `rcv1-pp` で2回）です。4回とも認識しました。

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
| `.local/sfc-next-trial/cold-boot/` | 電源断起動の手順書・`cold.py`（リセットしないUART接続、電源の切断と復帰の待機、受信時刻付きの取得、判定、`RECOVERY` 行の集計）・条件記録・生ログ。2026-11-18の試験ログもここにある |
| `.local/sfc-next-trial/port-recovery/` | 回復処理の試作手順書・`rcv.py`（Flash照合・アプリだけの書き込み）・調査結果・ビルドログと記録 |
| `.local/worktrees/sfc-port-recovery/` | 試作ブランチ `proto/sfc-port-recovery` のworktree。`.pio/build/fnk0099-host-rcv-pp*/` に試作版の成果物がある |
| `.local/preserved/07e005a/` | 元Hostの3ファイルのコピー（ハッシュ照合済み） |
| `.local/bolt-validation/` 内のログ・JSON・補助Python | 生UART・転送ログ・解析状態・復元処理。ポートや固有情報を含むためそのまま公開しない |

新しいcloneには上記の非公開ファイルはありません。公開証跡の設定・ソースコミット・ハッシュを使って再構築し、過去の実機BINと同一とは未確認のまま扱ってください。再ビルドでは埋め込みGit版情報などでハッシュが変わる場合があります。

## 再開時の手順と判断の区切り

最初は、FNK0099に今どの版が入っているかを確認してください。2026-11-18の終了時点では、試作 `rcv1-pp-d1000` が入っています。そのうえで、接続個体・ケーブル・向き・接続順序を確認し、条件を一つずつ記録します。

現時点で毎回認識できている方法は、次の2つです。
- **最初の列挙を約1秒遅らせた試作版**（`rcv1-pp-d1000`）：SFCを挿したままの起動でも10/10で認識。
- **主ブランチのHostの場合**：SFCを抜いた状態でHostを起動し、`HOST_READY` を確認してから全解放で挿す。

**主ブランチのHostへ戻す場合：** `.local/sfc-next-trial/controller1-crane-probe/trial.py restore` を使います。退避した0x0〜0x5DFFF（07e005a）を書き戻し、全範囲を読み戻して照合します。試作版を書き込む場合は、`.local/sfc-next-trial/port-recovery/rcv.py` を使います。手順は、`verify-board` で元の退避との一致を確認してから、`flash-app` でアプリ領域だけを書き込み、ハッシュを検証する、の順です。試作のソースはローカルブランチ `proto/sfc-port-recovery` にあり、worktreeは `.local/worktrees/sfc-port-recovery` です。

**UARTを開くとFNK0099が再起動します。** pyserialはポートを開いた後、DTR、RTSの順に信号を設定します。`tools/capture_uart.py` はどちらもオフを指定するため、途中で「DTRオフ・RTSオン」の状態が一瞬でき、自動リセット回路がENを下げます。動作中のボードの状態を保ったまま見る場合は、次のように接続してください。

- DTR・RTSともオンを指定して開く。
- 閉じるときは、RTS → DTRの順に下げる。

この方法で、開くときも閉じるときもリセットしないことを確認済みです（`.local/sfc-next-trial/cold-boot/cold.py` の `--open quiet`）。電源を入れた直後は、Macでポートが開けるのがログ時刻でおよそ650ms以降になるため、約600msの列挙ログは取り逃がします。

07e005aの5秒ごとのheartbeatの空きヒープは、状態の補助指標になります。試作版では値が少し異なり、認識済みで360,556、失敗後で363,000、機器の動きなしで364,432でした。

| 空きヒープ | 状態 |
|---|---|
| 364,448 | 機器の動きなし |
| 363,016 | 列挙失敗後 |
| 360,572 | 認識済み |

実施済みで、結論が出た候補：
- **Host側の回復処理（ルートポート電源の入れ直し）：** 機器の解放は毎回成功しました。しかし、最初の列挙に失敗した後のSFCは応答せず、回復しませんでした（0/5）。
- **最初の列挙を遅らせる：** 1秒の遅延で、どちらの起動条件でも10/10で認識しました。
- **遅延版の回帰確認：** 段階1〜3（12ボタン・保持・同時押しと一部解放・押したまま抜いたときの全解放と再接続・物理RESET×3・SFCなしで起動してから挿す）に合格しました。抜いた瞬間に `USB HOST: EP command error: ESP_ERR_INVALID_STATE` が1件出ましたが、動作には影響しませんでした。このE行は、キーボードを開いたまま2台目を抜いたときにも同じように出ました。原因は未調査です。
- **2台目と旧ケーブル構成での確認：** 遅延版で、2台目のC to Cと旧ケーブル構成のどちらでも、UARTリセット起動と電源断起動が各3/3で認識しました（計12/12）。

未実施の候補は次のとおりです。いずれも根本原因の断定ではありません。

- **1台目＋旧ケーブル構成での確認：** ユーザー環境でつなぎ替えができないため、未実施です。
- **2台目のBボタンの確認：** 短い押下・解放が繰り返された記録があり、軽く押しただけか接点のばたつきかを確かめる。
- **必要な最小の遅延の測定：** 300ms・500msなど。
- **主ブランチへの取り込み：** 回復処理の状態機械は不要です。`root_port_unpowered=true` でインストールし、一定時間後に `usb_host_lib_set_root_port_power(true)` を呼ぶだけの、最小の変更で足ります。
- **原因の確認：** ESP32のリセット中にUSB-Serial-JTAGがD+を引き上げていることや、SFCのブートローダーからアプリへの切り替えとの関係。測定手段があれば、D+/D-とVBUSを観測する。

比較版を再び使う場合は、`.local/sfc-next-trial/controller1-crane-probe/README.md` の手順（退避→書き込み→復元→全範囲照合）を流用できます。

UARTポートはローカルで再確認し、例の `YOUR_UART_PORT` を置き換えます。115200 baud。既存のログ名を上書きせず、次の試行用ディレクトリへ保存します。

```sh
mkdir -p .local/sfc-next-trial
python3 tools/capture_uart.py --port YOUR_UART_PORT --seconds 20 --output .local/sfc-next-trial/startup.log > .local/sfc-next-trial/console.log 2>&1
```

Python環境にはpyserialが必要です。[環境とログ取得手順](build.md)を参照してください。`capture_uart.py` は生ログを標準出力にも表示するため、上の例では両方をGit対象外へ保存しています。

`capture_uart.py` でポートを開くと、上記のとおりFNK0099がENリセットされます（SFCへの給電は続きます）。この起動を物理RESET試験として集計しないでください。成功した接続からボタン試験へ進む場合は、同じUARTセッションを維持するか、リセットしない接続を使ってください。

認識成功の確認点は `HOST_READY` だけでなく、次のすべてです。

- `1209:d004`、Low-Speed、IF1（subclass/protocol `1/1`）、Interrupt IN `0x82`、MPS 8、interval 10ms。
- `keyboard open`、`SET_PROTOCOL(BOOT)`、`SET_IDLE(0)`、`keyboard start` がすべて `ESP_OK`。
- 全解放レポート、`SFC mask=0x000`、警告・エラー・余分な再起動の有無。

認識が確認できた区切りで共有し、その後にB → Y → SELECT → START → 上 → 下 → 左 → 右 → A → X → L → Rの押下・解放24遷移を照合します。主ブランチのHostでは、1台目のHost先行接続で12ボタン単独入力に合格しています。保持・部分解放・切断復帰を含む段階1〜3は、試作の遅延版 `rcv1-pp-d1000` で1台目が合格しました（2026-11-18）。主ブランチの版では、まだ試していません。

ユーザーは、ビルド成功後の書き込み・検証を継続して行うことを許可しています。一方、判断が必要な場面と作業の区切りでは停止する方針です。1台目のABA比較では、新しいビルドはしていません。保持済みの比較版の書き込みと、退避からの復元だけを行いました。電源断起動の試験では、書き込みもしていません。2026-11-18は、試作ブランチでビルドし、アプリ領域だけの書き込みと、退避からの復元を繰り返しました。

公開前には実際のUARTポート、ユーザーの絶対パス、MAC、固有番号、認証情報、Flash退避を除外してください。USB標準属性・固定試験操作・ソース／ファームウェアのハッシュだけを選んで記録します。
