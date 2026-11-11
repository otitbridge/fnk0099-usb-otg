# FNK0099 × UIAPduino内蔵SFCコントローラー USB Host検証 — Codex引き継ぎ書

> この文書は実機試験前の初期計画です。現在の動作確認結果・制約は[README](README.md)と[検証結果](docs/validation.md)を参照してください。

- 文書版：v0.2
- 作成日：2026-11-11（日本時間）
- 対象：Freenove **FNK0099 / ESP32-S3-WROOM Board Lite** をUSB Hostとして使い、**UIAPduino内蔵SFCコントローラー**を読み取る
- 本書の位置づけ：公開情報に基づく事前調査・検証計画。**FNK0099＋UIAPduinoの実機接続成功報告ではない**
- 開発引き継ぎ先：Codex

> **最重要の訂正**：以前の検討で「FNK0099のUSB-C電源・CC周りが最大の障害候補」と評価したが、FNK0099固有の故障・給電不能・USB-C Host不対応を示す実証は得られていない。**未確認事項を障害と断定しないこと**。まず基板改造や部品追加なしで動作確認し、失敗した場合に電源／CC／列挙／HIDの順で切り分ける。

---

## 1. 目的・到達条件

### 1.1 目的

すでに製作済みのSFC純正コントローラー（UIAPduino V1.4を内蔵）を、iPhone/iPadではなく **FNK0099の既設USB-OTG端子**につなぎ、USB HIDキーボードとして列挙・受信する。受信したキー状態を12ボタンのマスクへ逆変換し、USB-UART経由のログで観測する。

```text
Mac/PC ──USB（USB-UART：開発・電源・ログ）── FNK0099
                                               │
                                  既設USB-OTG Type-C
                                               │
                        SFCコントローラー内蔵USB-Cケーブル
                                               │
                            UIAPduino V1.4（USB HID Device）
                                               │
                               SFC純正基板・12ボタン
```

### 1.2 MVPの合格条件

1. FNK0099のUSB Hostスタックが正常起動する。
2. **直接接続（追加給電・USBハブ・基板改造なし）**で、対象デバイスの検出・USB列挙に成功するかを記録する。失敗した場合も原因を切り分けて報告する。
3. `VID:PID = 1209:D004` のUIAPduino通常ファームを検出し、Keyboardインターフェースを特定する。
4. Interrupt INで押下／解放に対応する**現在の8バイトHIDキーボード状態**を受信する。
5. **B、A、十字キー、SELECT、START、X、Y、L、Rの12入力**について、ビットマスク復元を確認する。
6. 長押し、複数同時押し、部分解放、USB抜き差し時に、ホスト側状態が破綻しないことを確認する。
7. 不具合は「未確認」「実機で失敗」「原因確認済み」に区別して記録する。

**今回の範囲外**：Wi-Fi送信、ESP-NOW、ゲームエミュレーター実装、USBゲームパッド化、UIAPduino側のファーム改造、FNK0099の配線改造・電源強制注入。

---

## 2. 使用機材・既存資産

| 項目 | 使用物・条件 | 状態 |
|---|---|---|
| USB Hostボード | **Freenove FNK0099**、ESP32-S3-WROOM Board **Lite**。想定構成 N16R8（16MB Flash / 8MB PSRAM） | 今回の対象。実装前に基板刻印・搭載モジュール表記を再確認 |
| USB Device | 純正SFCコントローラーに**UIAPduino V1.4**とUSB-C通信ケーブルを内蔵 | iPhone／iPadで動作確認済み |
| コントローラー変換ファーム | `otitbridge/snes-controller-uiapduino`、通常版 v1.0.0（SELECT→Tab） | 公開・実機確認済み。原則変更しない |
| ケーブル | 完成したSFCコントローラーから出るUSB-Cケーブル（セリア、AT-PDCASTCC08） | iPhone/iPadで通信実績あり。**FNK0099とのC-C接続は未検証** |
| 開発・ログPC | MacまたはPC、FNK0099のUSB-UART端子に接続 | FNK0099側開発環境はこれから確認 |

### 対象のGitHubリポジトリ

- 完成済みSFCコントローラー：<https://github.com/otitbridge/snes-controller-uiapduino>
- [公開README](https://github.com/otitbridge/snes-controller-uiapduino/blob/main/README.md)
- [ファームウェアのキー割り当て](https://github.com/otitbridge/snes-controller-uiapduino/blob/main/firmware/snes_controller/snes_hid_report.h)
- [既知の制約](https://github.com/otitbridge/snes-controller-uiapduino/blob/main/docs/known-issues.md)
- [v1.0.0配布版](https://github.com/otitbridge/snes-controller-uiapduino/releases/tag/v1.0.0)

**既知の端末検証**：iPhone 17＋Delta、iPad Air（M2）＋Deltaで操作確認済み。ただしこれは**FNK0099のUSB Host動作を証明しない**。

---

## 3. 公開情報の再調査：電源・USB-Cの結論

### 3.1 結論

- **FNK0099固有の「OTG端子から給電できない」「USB-C Hostに非対応」という実測結果は確認していない。**
- 一方、**FNK0099 Liteの既設OTG Type-Cから、C-C直結でHIDキーボードを給電・列挙できた**と、型番・電源経路・接続方法まで特定できる公開実績も確認できていない。
- 同系統の**Freenove ESP32-S3-WROOM Board（Liteと明記されない製品）**では、OTG Type-CのVBUS給電ができるとの実機資料がある。
- したがって、FNK0099での試験は十分合理的だが、USB-C直結を「実績あり」とは言えない。**追加の部品購入・回路変更を前提とせず、最初に現物で試す**。

### 3.2 出典別の根拠と限界

| 出典 | 公開された事実 | **FNK0099 Liteへ一般化できない部分** |
|---|---|---|
| [EspUsbHost：実機確認済みボード一覧](https://github.com/tanakamasayuki/EspUsbHost/blob/main/docs/tested-devices.ja.md) | 「Freenove ESP32-S3-WROOM Board」はOTG Type-Cから給電可能と記載 | **Lite / FNK0099の明記がない** |
| [EspUsbHost：USB Host開発ガイド](https://github.com/tanakamasayuki/EspUsbHost/blob/main/docs/usb-host-guide.ja.md) | VBUSの実測を推奨。Hostには接続デバイスへの給電が必要 | この一般論のみでFNK0099の給電不能は示せない |
| [Qiita：Freenove ESP32-S3 WROOM N8R8でUSBキーボード入力](https://qiita.com/Mobu_Kyoto/items/762a2467fc58b558952a)（2025-06-21公開） | ESP32-S3でワイヤレスキーボードのUSBレシーバー受信に成功。GPIO19=D−、GPIO20=D＋、5V=VBUSを使用 | **USB-Aメス端子を外付け**。Liteの型番・既設C-C端子経由の給電実績ではない |
| [note：ESP-IDF HID Hostの実装記事](https://note.com/ndenki/n/n7a8d171f2791)（2026-03-24） | ESP-IDFのHID Host例を解説。Freenove Board Liteを入門用に紹介 | **記事内の商品の紹介 ≠ FNK0099のC-C直結の実機証明** |
| [Freenove FNK0099公式製品ページ](https://store.freenove.com/products/fnk0099) | LiteとしてUSB-OTG機能、8/16MB Flashの選択肢を明示 | **USB HostとしてのVBUS供給・CC回路の詳細までは明示しない** |
| [Freenove FNK0099公式リポジトリ](https://github.com/Freenove/Freenove_ESP32_S3_WROOM_Board_Lite) | Lite専用のピン配置・例・資料を公開 | 公開フォルダにUSB-C電源・CC回路を確定できる完全な回路図があるとは確認していない |
| [betaflight/bridge](https://github.com/betaflight/bridge) | Freenove ESP32-S3-WROOM N8R8 v1.1をUSB Hostとして利用する構成が公開されている | **FNK0099 Liteと同一基板か不明**。USB-C電源の内部回路を特定する証拠ではない |

> **調査方法の注意**：`FNK0085/0085B` と `FNK0099` は別製品。N8R8/N16R8というメモリ表記だけで基板の電源回路を同一視しない。記事の「Freenove ESP32-S3-WROOM」は、製品名だけではLiteと断定できない。

### 3.3 USB-C CCとVBUSについての正確な扱い

USB-CではSource/Sink役割をCC端子のRp/Rdなどで判定するのが規格上の基本。だが、FNK0099の具体的な端子処理や5V経路を**回路図・実測なしに推測して故障原因と決めてはいけない**。

- USB-C C-Cで**正常給電するかは試験する**。
- **無接続時のVBUSが0Vでも直ちに不良ではない**。CC検出後に5Vを出す設計もある。接続状態とHostの初期化状態を併記して評価する。
- 実装された回路が規格に適合しているかと、目の前の組み合わせが実際に動くかは別の評価項目。
- UIAPduino側はこれまでiPhone/iPadで正常動作しているため、接続失敗時はFNK0099の電源・Host側と、USB列挙・プロトコル相性を区別する。

**安全上の禁止事項**：

- 未確認のままFNK0099の5V/GPIO端子からUSB-CのVBUSにジャンパ線で給電しない（逆流・短絡リスク）。
- CC抵抗・基板パターンの切断／追加、USB-C端子の改造を初動で行わない。
- 実通電中のType-Cレセプタクル内部へテスタープローブを直接差し込んで短絡させない。計測時は**端子を分離して測れるUSB-Cブレークアウト・テスト治具**を用い、適切な接続条件で行う。
- 電圧測定はデバイスを保護できる方法で行い、不明な電源の並列接続は避ける。

---

## 4. UIAPduino側のUSB仕様（現在の公開実装から）

完成済みコントローラーは**USB HIDゲームパッドではなく、キーボード＋マウスの複合HIDデバイス**。FNK0099側は**キーボードインターフェースのみ受信**すればよい。

| 項目 | 公開実装の値 |
|---|---|
| ファーム | UIAPduino版 `sfc-keyboard` v1.0.0、SELECT=Tab |
| VID:PID | `1209:D004`（コード・設定で使用） |
| USB転送速度 | Low-Speed（1.5 Mbps） |
| Interface 0 | HID Mouse / Boot Mouse |
| **Interface 1** | **HID Keyboard / Boot Keyboard** |
| Keyboard Interrupt IN endpoint | **`0x82`** |
| Keyboard report | **8バイト**、Report IDなし |
| キー枠 | 修飾キー1バイト、予約1バイト、HID Usage ID×6 |
| Endpoint polling interval | **10 ms**（記述子） |
| ボタン読み取り周期 | 約10ms（ファーム仕様） |

参照：

- <https://github.com/tarosay/arduino_core_ch32/blob/9471a150c9837c9055ef62ef759a1a593eddf1b1/cores/arduino/usb_config.h>
- <https://github.com/otitbridge/snes-controller-uiapduino/blob/main/firmware/snes_controller/snes_hid_report.h>
- <https://github.com/otitbridge/snes-controller-uiapduino/blob/main/docs/sfc-keyboard.md>
- <https://github.com/otitbridge/snes-controller-uiapduino/tree/main/patches>

**要注意（USBクラス要求）**：UIAPduinoのベースコアは`SET_PROTOCOL`／`SET_IDLE`の実装が十分とは限らない。プロジェクトでは`GET_REPORT(Input)`等を修正済みだが、**ESP-IDF HID Hostのサンプルが要求する初期化手順を、このデバイスが満たすかは未検証**。列挙成功後にクラス要求で止まった場合、最初に受信ログとデスクリプタを採り、Hostドライバーの要求・応答を確認する。いきなりDeviceファームの改造に進まない。

### 4.1 HID Usage ID → SFCボタン対応表

| SFCのシリアル順 / ビット | SFCボタン | USBキー | Usage ID |
|---:|---|---|---|
| 0 | B | Z | `0x1D` |
| 1 | Y | A | `0x04` |
| 2 | SELECT | Tab | `0x2B` |
| 3 | START | Enter | `0x28` |
| 4 | 上 | ↑ | `0x52` |
| 5 | 下 | ↓ | `0x51` |
| 6 | 左 | ← | `0x50` |
| 7 | 右 | → | `0x4F` |
| 8 | A | X | `0x1B` |
| 9 | X | S | `0x16` |
| 10 | L | Q | `0x14` |
| 11 | R | W | `0x1A` |

既存UIAPduinoファームでは**最大6キーを選択**し、7キー以上で後順位のボタンがレポートに含まれない仕様。FNK0099側で復元できるのは**USBに送られた最大6入力**であり、存在しない7番目以降の情報は復元できない。

### 4.2 ホスト側の解析方針

1. `report[0]`＝修飾キー、`report[1]`＝予約、`report[2..7]`＝最大6個のHID Usage IDとして解析する。
2. 上表に従って`uint16_t mask`のbit 0〜11を立てる。押下中は1。
3. **レポート全体から毎回現在状態を再構成**する。前回との`XOR`で押下／解放イベントを作る。
4. USB切断、エンドポイント停止、Host再初期化では必ず`mask = 0`に戻す。
5. OSの自動キーリピートやASCII変換は必要ない。**Raw Usage ID**を優先する。
6. マウス用インターフェース／レポートをキーボードと誤読しない。
7. 接続時にホスト側が受信を始める前の入力保持、サスペンド／復帰時の初期状態は未保証として別試験にする。

---

## 5. FNK0099側のUSB Host実装方針

### 5.1 優先候補

- **第一候補：ESP-IDF公式 USB HID Hostサンプル**
  - <https://github.com/espressif/esp-idf/tree/v5.5.2/examples/peripherals/usb/host/hid>
  - 長所：接続／クラス要求／ディスクリプタ／受信を低レベルで追跡しやすい。
  - 注意：バージョン・依存ライブラリ・対応する初期化プロトコルを記録する。サンプルで`SET_PROTOCOL`が失敗したときは原因切り分けを行う。
- **第二候補：Arduino-ESP32 + `EspUsbHost`**
  - <https://github.com/tanakamasayuki/EspUsbHost>
  - ArduinoでUSB HID入力を試せる。ただし**ライブラリv2はv1とAPI互換ではない**。Codexは実際に採用した版のREADME／サンプルを読んでからAPIを確定する。

**禁止**：以前の会話中の未検証スケッチのAPI名を、現行ライブラリにあるものと仮定してコピーペーストしない。ビルドで確認できる根拠のあるAPIのみ使う。

### 5.2 開発環境の切り分け

- USB-UART端子：PC/Macとの書き込み・`115200 baud`等のログ通信に使用。実際のポート名は列挙結果に従う。
- USB-OTG端子：SFCコントローラーのUSB-Cケーブル接続先。**コネクターの位置を思い込みで決めない**。
- `GPIO19 = D−`、`GPIO20 = D＋`はESP32-S3側USB OTGの標準接続だが、**FNK0099の基板配線は実物・製品資料で確認**。
- 既存UIAPduino側のファームは原則として触らず、Host側の最小機能から着手する。

---

## 6. 実機検証計画（フェーズと判定基準）

| フェーズ | 具体的作業 | 合格条件 | 失敗時の分類 |
|---|---|---|---|
| P0: 機材同定 | FNK0099実物写真／刻印、N16R8、2端子の役割、基板Revを記録 | Host対象基板・端子を特定 | ボード同定未完 |
| P1: Host単体起動 | USB-UARTで書き込み・ログ。Host初期化サンプルを実行 | `ESP_OK`等の初期化成功、PCログ継続 | Host初期化問題 |
| P2: 直結の確認 | **追加部品・改造なし**でコントローラーをOTGへ接続 | UIAPduino起動、USB接続イベント。成功／失敗を記録 | 電源／CC／接続の未切り分け |
| P3: 列挙・HID検出 | Device／Configuration／Interface／Endpoint descriptorを記録 | `1209:D004`、Keyboard Interface 1、EP `0x82` | 列挙・クラス要求問題 |
| P4: ボタン取得 | 8バイトレポートを16進でログ出力 | 12入力を単独押下／解放で識別 | HID受信・解析問題 |
| P5: 複合入力 | 上＋B、右＋Y＋Bの部分解放、長押し、抜き差し | 状態マスク正常、切断後0 | 状態同期・例外処理問題 |
| P6: 再現性 | 再起動、複数回再接続、ログ・コード・版数保存 | 他者が同手順を追試できる | 記録・再現性不足 |

**安全な失敗時の分岐**：

```text
接続できない
  ├─ USB Hostスタック未起動 → ソフトウェア設定・ピン・コンソールを確認
  ├─ UIAPduinoが起動していない
  │    └─ USB-Cテスト治具等でVBUS／接続状態を確認（改造禁止）
  ├─ UIAPduinoが起動しているが列挙されない
  │    └─ OTG端子・D+/D−・CCの実装／ケーブル方向／USBイベントを確認
  ├─ 列挙はするがHID開始できない
  │    └─ Interface 1、EP 0x82、SET_PROTOCOL／SET_IDLE、GET_REPORTを調査
  └─ HID受信できるが入力がおかしい
       └─ 8バイトレポート・Usage ID・解放・同時押しの変換ロジックを確認
```

※ USB-Cでは**無接続でのVBUS=0Vを給電不良と断定しない**。接続状態・Host起動状況・適切な試験治具の有無を記録する。

### 6.1 推奨ログ例（**想定出力**。実測ではない）

```text
[HOST] FNK0099 / ESP32-S3 / native USB OTG initialized
[USB ] CONNECTED: low-speed, VID:PID=1209:D004
[HID ] interface=1, boot-keyboard, ep_in=0x82, mps=8, interval=10ms
[RAW ] 00 00 1D 4F 00 00 00 00
[SFC ] mask=0x081  B + RIGHT
[RAW ] 00 00 4F 00 00 00 00 00
[SFC ] mask=0x080  RIGHT
[USB ] DISCONNECTED
[SFC ] mask=0x000  NONE
```

上の2キー同時押しにおけるキー配列順序は、現行UIAPduinoファームの固定優先順に基づく。

### 6.2 最低限残す証跡

- FNK0099の基板表裏写真・シルク印字・Rev・搭載モジュール
- PC/Macへの接続ポートと給電経路、**セルフパワーHub使用の有無**
- USB-C接続方式（直結 C-C、C端子向き、USB-Cテスト治具・ハブ等）
- ESP-IDF/Arduino Core/USB Hostライブラリの版・コミット
- 実行したサンプル／自作コードのコミット
- VID/PID・列挙された速度・インターフェース・エンドポイント
- 8バイトRaw HID・ボタンマスク・接続／切断ログ
- VBUSを測った場合は**測定条件・測定位置・値**を記載。未測定なら「未測定」
- 失敗内容、再現手順、観測と推測の区別

---

## 7. 実装上の注意・設計判断

1. **既存資産を温存**：`otitbridge/snes-controller-uiapduino`に不要な変更・pushをしない。必要ならFNK0099側の独立プロジェクトとして作業する。
2. **ファームに関する制約**：現行UIAPduinoはUSB HIDキーボード。USBゲームパッドとして認識させる必要はない。
3. **ブートキーボード扱いに注意**：ディスクリプタはKeyboard Boot Interfaceだが、USB Hostのクラス要求に完全対応するかは別。ログを根拠に互換性を判定する。
4. **キーの押しっぱなし防止**：Device切断・Hostエラー・受信停止時に内部マスクを解放状態へ戻す。
5. **6-key rollover**：7入力以上を読み取れないのは現行Deviceファームの仕様。FNK0099側で解消できない。
6. **性能は未測定**：読み取り約10ms、Interrupt IN 10msから厳密なエンドツーエンド遅延を断定しない。
7. **追加回路は後段**：直結で失敗しても原因確定前に5V強制注入やCC変更をしない。必要なら適合するセルフパワーハブ／Host向け給電回路を安全条件とともに別案として検討する。
8. **記録の透明性**：ユーザー実機報告、公開OSS上の動作報告、推論、ホストテスト、未試験を混ぜない。

---

## 8. Codexへの開始指示（そのままコピー可）

> 本書に沿い、Freenove FNK0099（ESP32-S3-WROOM Board Lite、想定N16R8）をUSB Hostとして動作させ、既存の `otitbridge/snes-controller-uiapduino` v1.0.0をUSB HIDキーボードとして認識し、12ボタン状態をログ表示する最小実装を進めてください。
>
> 最初に **FNK0099 Lite固有のUSB OTG・USB-UART・VBUS・CCに関する公開一次資料**と、使用するHostライブラリの現行API・サンプルを確認してください。同系統Freenove Boardの実績をFNK0099 Liteの実績と混同しないでください。
>
> 外付け5V回路・セルフパワーハブ・基板改造を前提とせず、既設OTG Type-Cへの**C-C直結を第一試験**としてください。ユーザーが実測していない値や成功結果を作らず、実機試験が必要な箇所は明示してください。
>
> 最初の成果物は、(1) ボード／USB電源回路について分かったことと未確認事項の表、(2) 最小Hostファームウェア、(3) ビルド・書き込み・ログ出力手順、(4) USB列挙／HID解析／12ボタン復元の試験手順、(5) 不具合時の切り分け表です。
>
> UIAPduinoの既存ファームウェアは原則変更しません。ファイル編集、コミット、pushなどをする場合は作業対象を明確にし、実機操作・結果はユーザーの報告と区別してください。

---

## 9. 参照資料

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

---

**現時点の採用方針**：FNK0099の既設OTGへUIAPduino内蔵SFCコントローラーを直結して検証する。**「FNK0099の給電不能」は確認済みの問題として扱わない**。USB Hostの成功・失敗は実機のログ・電源観測をもって判定する。
