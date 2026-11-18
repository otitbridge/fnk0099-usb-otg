# 検証証跡

各JSON・ログはその試験時点の記録です。過去の `pending` や失敗判定は、後続試験で解消されている場合があります。現在の結果は用途別の検証結果を参照してください。公開ログはローカルパス・MACアドレスなどを除去しています。生ログ・機器固有情報はGit対象外の `.local/` に保持します。

## Bolt：K950・M750（開発中、2026-11-17）

USB認識・全4IFの記述子取得と受信開始、入力解釈のホストテスト、コマンド過多の修正・再試験を確認しています。K950の固定キー操作、M750の3ボタン・4方向移動・縦ホイール両方向、同時保持・片側解放は合格です。再接続3回の機能復帰を確認し、抜去時の診断ログを評価中です。3回目のA追加操作はユーザーの押し直しと照合済みです。物理RESETは合計3回合格しました。3回目の操作不明な初回試行は合格回数に含めず、再試験で確認しています。[Bolt検証記録](../bolt-validation.md)を参照してください。

| ファイル | 内容 |
|---|---|
| [bolt-discovery-2026-11-17.json](bolt-discovery-2026-11-17.json) | USB構成、記述子のサイズ・ハッシュ、実機BIN、20秒の起動確認 |
| [bolt-decoder-preparation-2026-11-17.json](bolt-decoder-preparation-2026-11-17.json) | 入力解釈のホストテスト、実機BIN、監視時の失敗・コマンド過多の修正後の再試験。物理入力は未検証 |
| [bolt-keyboard-input-2026-11-17.json](bolt-keyboard-input-2026-11-17.json) | K950のA・左Shift＋A・右矢印、固定8遷移の実機照合 |
| [bolt-mouse-input-2026-11-17.json](bolt-mouse-input-2026-11-17.json) | M750の3ボタン・4方向移動・縦ホイール両方向、最終状態の実機照合 |
| [bolt-simultaneous-attempt1-2026-11-17.json](bolt-simultaneous-attempt1-2026-11-17.json) | 初回は同時保持24.38秒で時間条件未達。後続の再試験で解消 |
| [bolt-simultaneous-attempt2-2026-11-17.json](bolt-simultaneous-attempt2-2026-11-17.json) | 再試験で同時保持74.51秒・片側解放後5.34秒の保持と移動、入力復帰に合格 |
| [bolt-reconnect-cycle1-2026-11-17.json](bolt-reconnect-cycle1-2026-11-17.json) | 再接続1回の機能復帰を確認。抜去時エラー12行の評価保留、残りの試験は中断 |
| [bolt-reconnect-cycle2-2026-11-17.json](bolt-reconnect-cycle2-2026-11-17.json) | 2回目も機能復帰。抜去時10行にURB・0バイト入力の追加診断があり、3回目の前に停止 |
| [bolt-reconnect-cycle3-2026-11-17.json](bolt-reconnect-cycle3-2026-11-17.json) | 3回目も機能復帰。Aの2組はユーザーの押し直しと一致、当初の回数不一致判定と抜去時診断10行を保持 |
| [bolt-reset-cycle1-2026-11-17.json](bolt-reset-cycle1-2026-11-17.json) | 物理RESET1回目に合格。準備起動を除外し、4IF・A・左クリックの復帰、警告・エラー0を確認 |
| [bolt-reset-cycle2-2026-11-17.json](bolt-reset-cycle2-2026-11-17.json) | 物理RESET2回目に合格。監視下での再実施のみを集計し、4IF・A・左クリック復帰、警告・エラー0を確認 |
| [bolt-reset-cycle3-attempt1-2026-11-17.json](bolt-reset-cycle3-attempt1-2026-11-17.json) | 3回目の初回試行は操作不明。RESET起動を観測せず切断時診断で停止、合格回数に含めず再試験 |
| [bolt-reset-cycle3-attempt2-2026-11-17.json](bolt-reset-cycle3-attempt2-2026-11-17.json) | 3回目の再試験に合格。4IF・A・左クリック復帰、警告・エラー0を確認し、RESET合格は合計3回 |

## SFC：HID更新後の回帰（最初の列挙を遅らせた試作で接続済み起動10/10、2026-11-17〜18）

現在の実機状態・条件比較・成果物・再開手順は[引き継ぎ資料](../sfc-troubleshooting.md)を参照してください。試行時点の詳細は、[11-17の履歴](../history/sfc-troubleshooting-2026-11-17.md)と[11-18の履歴](../history/sfc-troubleshooting-2026-11-18.md)にあります。

| ファイル | 内容 |
|---|---|
| [sfc-hid110-preparation-2026-11-17.json](sfc-hid110-preparation-2026-11-17.json) | 既存SFC変換テスト、1.1.0でのビルド・書き込み・Host起動。SFC実機入力は未検証 |
| [sfc-hid110-connect-attempt1-2026-11-17.json](sfc-hid110-connect-attempt1-2026-11-17.json) | 接続切替後、CHECK_ADDRの列挙エラー3行で停止。機器識別・実機入力は未確認 |
| [sfc-hid110-connect-attempt2-2026-11-17.json](sfc-hid110-connect-attempt2-2026-11-17.json) | 接続を維持した起動でもCHECK_SHORT_DEV_DESCのエラー3行。接続条件の確認待ち |
| [sfc-hid110-mac-identity-2026-11-17.json](sfc-hid110-mac-identity-2026-11-17.json) | Macで1209:d004、Low-Speed、IF1 Boot Keyboard・8バイト入力を確認。FNK0099の認識失敗原因は未確定 |
| [sfc-hid110-connect-attempt3-2026-11-17.json](sfc-hid110-connect-attempt3-2026-11-17.json) | Macから戻してもCHECK_ADDRのエラー3行。プラグ反転は未確認、向きの比較には数えず停止 |
| [sfc-hid101-baseline-preparation-2026-11-17.json](sfc-hid101-baseline-preparation-2026-11-17.json) | 従来4348f79／HID 1.0.1の隔離ビルド成功。比較BINの識別情報、実機は未書き換え |
| [sfc-hid101-comparison-2026-11-17.json](sfc-hid101-comparison-2026-11-17.json) | 1.0.1比較版でもCHECK_SHORT_DEV_DESC失敗。元の1.1.0全3領域を復元し、再確認でも失敗。原因未確定 |
| [sfc-hid110-controller2-enumeration-2026-11-17.json](sfc-hid110-controller2-enumeration-2026-11-17.json) | 2台目のSFCは同じ1.1.0版で認識・全解放受信成功。警告・エラー0、ボタン回帰は未実施 |
| [sfc-hid110-controller2-startup-attempt2-2026-11-17.json](sfc-hid110-controller2-startup-attempt2-2026-11-17.json) | 2台目も次の起動ではCHECK_FULL_DEV_DESC失敗。初回成功と併記し、入力回帰は未開始 |
| [sfc-hid110-controller2-host-first-2026-11-17.json](sfc-hid110-controller2-host-first-2026-11-17.json) | Host起動後の接続でもCHECK_FULL_DEV_DESC失敗、ボタン試験未開始で停止 |
| [sfc-controller-source-review-2026-11-17.json](sfc-controller-source-review-2026-11-17.json) | 起動時SELECT切り替えと既知v1.0.0 BINを確認。実機BINは未特定、SFC書き換えは未実施 |
| [sfc-controller2-v100-upload-2026-11-17.json](sfc-controller2-v100-upload-2026-11-17.json) | 2台目を既知v1.0.0へ書き込み、読み戻し一致・起動・Mac認識成功。FNK0099での比較は接続待ち |
| [sfc-controller2-v100-hid110-host-first-2026-11-17.json](sfc-controller2-v100-hid110-host-first-2026-11-17.json) | 検証済みSFC v1.0.0でもHost 1.1.0の接続でCHECK_ADDR失敗、入力未開始 |
| [sfc-known-reference-pair-comparison-2026-11-17.json](sfc-known-reference-pair-comparison-2026-11-17.json) | 検証済みSFC v1.0.0＋従来Host 1.0.1でも失敗。元Host 1.1.0へ全領域復元済み、SFC回帰は保留 |
| [crane-game-usb-reference-review-2026-11-17.json](crane-game-usb-reference-review-2026-11-17.json) | 成功実績はArduino／IDF 4.4.7。従来の比較は5.5.0のままと判明。USBだけの隔離比較版ビルド済み・未書き込み |
| [crane-game-usb-probe-comparison-2026-11-17.json](crane-game-usb-probe-comparison-2026-11-17.json) | Arduino／IDF 4.4.7のUSB専用版でもCHECK_SHORT_DEV_DESC失敗。元の先頭64KiB・SFC用1.1.0アプリを復元・検証済み |
| [sfc-controller2-c-to-c-startup-2026-11-17.json](sfc-controller2-c-to-c-startup-2026-11-17.json) | 同じ2台目・元HostのままC to C直結へ交換。接続済み起動でもCHECK_SHORT_DEV_DESCのエラー3行、入力未開始 |
| [sfc-controller1-connection-order-comparison-2026-11-17.json](sfc-controller1-connection-order-comparison-2026-11-17.json) | 1台目・C to Cで元Host→IDF 4.4.7比較版→復元Hostを比較。接続済み起動は両版9/9失敗、Host先行接続は両版1/1認識。0x0〜0x5DFFFの退避・全範囲読み戻し一致 |
| [sfc-controller1-hid110-host-first-buttons-2026-11-17.json](sfc-controller1-hid110-host-first-buttons-2026-11-17.json) | Host 1.1.0・1台目のHost先行接続で認識、12ボタン単独入力24遷移に合格。保持・部分解放・切断復帰は未実施 |
| [sfc-controller1-cold-power-on-2026-11-17.json](sfc-controller1-cold-power-on-2026-11-17.json) | リセットしないUART接続を確認。SFC接続済みで電源を完全に切った起動は1/3認識、当日のUARTリセット対照は失敗 |
| [sfc-root-port-recovery-prototype-2026-11-18.json](sfc-root-port-recovery-prototype-2026-11-18.json) | 試作 `rcv1-pp`（ルートポート電源の入れ直し）。機器の解放は毎回成功するが、UARTリセット起動は0/5で回復せず。SFCなし・Host先行接続・抜き差しは正常 |
| [sfc-first-enumeration-timing-2026-11-18.json](sfc-first-enumeration-timing-2026-11-18.json) | 最初の列挙の時刻を比較。電源断起動は現行2/8・試作8/8、給電を1秒遅らせた `rcv1-pp-d1000` は電源断5/5・UARTリセット5/5 |

## MSC：カードリーダー・microSD（2026-11-17）

現在の[MSC検証結果](../msc-validation.md)に、当初計画7項目の結果、実機BIN、書き込み・再マウント・Mac照合をまとめています。各時点の詳細は[経過記録](../history/msc-validation-2026-11-17.md)を参照してください。

| ファイル | 内容 |
|---|---|
| [msc-reconnect-reset-2026-11-17.json](msc-reconnect-reset-2026-11-17.json) | 再接続3回・RESET3回、各回の3ファイルのハッシュ・時間・ヒープ、最終状態 |

## HID：SFCコントローラー（2026-11-11）

現在の状態は[HID検証結果](../validation.md)を参照してください。修正版BINは[p5-fix-firmware](p5-fix-firmware-2026-11-11.json)、最終版の機能確認は[regression-final](regression-final-2026-11-11.json)で識別できます。失敗・保留記録も経過として保存しています。

| ファイル | 種別 |
|---|---|
| [cold-start-2026-11-11.json](cold-start-2026-11-11.json) | 判定・メタデータ |
| [cold-start-2026-11-11.log](cold-start-2026-11-11.log) | ログ |
| [endurance-deferred-2026-11-11.json](endurance-deferred-2026-11-11.json) | 判定・メタデータ |
| [endurance-deferred-2026-11-11.log](endurance-deferred-2026-11-11.log) | ログ |
| [p0-flash-id-2026-11-11.log](p0-flash-id-2026-11-11.log) | ログ |
| [p1-build-2026-11-11.log](p1-build-2026-11-11.log) | ログ |
| [p1-result-2026-11-11.json](p1-result-2026-11-11.json) | 判定・メタデータ |
| [p1-uart-2026-11-11.log](p1-uart-2026-11-11.log) | ログ |
| [p1-upload-2026-11-11.log](p1-upload-2026-11-11.log) | ログ |
| [p2-direct-uart-2026-11-11.log](p2-direct-uart-2026-11-11.log) | ログ |
| [p2-result-2026-11-11.json](p2-result-2026-11-11.json) | 判定・メタデータ |
| [p3-build-2026-11-11.log](p3-build-2026-11-11.log) | ログ |
| [p3-enumeration-2026-11-11.log](p3-enumeration-2026-11-11.log) | ログ |
| [p3-result-2026-11-11.json](p3-result-2026-11-11.json) | 判定・メタデータ |
| [p3-upload-2026-11-11.log](p3-upload-2026-11-11.log) | ログ |
| [p4-buttons-2026-11-11.log](p4-buttons-2026-11-11.log) | ログ |
| [p4-result-2026-11-11.json](p4-result-2026-11-11.json) | 判定・メタデータ |
| [p5-disconnect-failed-2026-11-11.json](p5-disconnect-failed-2026-11-11.json) | 判定・メタデータ |
| [p5-disconnect-failed-2026-11-11.log](p5-disconnect-failed-2026-11-11.log) | ログ |
| [p5-disconnect-retry-2026-11-11.json](p5-disconnect-retry-2026-11-11.json) | 判定・メタデータ |
| [p5-disconnect-retry-2026-11-11.log](p5-disconnect-retry-2026-11-11.log) | ログ |
| [p5-fix-build-2026-11-11.log](p5-fix-build-2026-11-11.log) | ログ |
| [p5-fix-firmware-2026-11-11.json](p5-fix-firmware-2026-11-11.json) | 判定・メタデータ |
| [p5-fix-upload-2026-11-11.log](p5-fix-upload-2026-11-11.log) | ログ |
| [p5-hold-2026-11-11.log](p5-hold-2026-11-11.log) | ログ |
| [p5-hold-result-2026-11-11.json](p5-hold-result-2026-11-11.json) | 判定・メタデータ |
| [p6-reconnect-2026-11-11.log](p6-reconnect-2026-11-11.log) | ログ |
| [p6-reconnect-result-2026-11-11.json](p6-reconnect-result-2026-11-11.json) | 判定・メタデータ |
| [p6-restart-2026-11-11.log](p6-restart-2026-11-11.log) | ログ |
| [p6-restart-result-2026-11-11.json](p6-restart-result-2026-11-11.json) | 判定・メタデータ |
| [regression-buttons-2026-11-11.json](regression-buttons-2026-11-11.json) | 判定・メタデータ |
| [regression-buttons-2026-11-11.log](regression-buttons-2026-11-11.log) | ログ |
| [regression-final-2026-11-11.json](regression-final-2026-11-11.json) | 判定・メタデータ |
| [regression-final-2026-11-11.log](regression-final-2026-11-11.log) | ログ |
| [regression-hold-attempt1-2026-11-11.json](regression-hold-attempt1-2026-11-11.json) | 判定・メタデータ |
| [regression-hold-attempt1-2026-11-11.log](regression-hold-attempt1-2026-11-11.log) | ログ |
| [usb-c-orientation-2026-11-11.json](usb-c-orientation-2026-11-11.json) | 判定・メタデータ |
| [usb-c-orientation-2026-11-11.log](usb-c-orientation-2026-11-11.log) | ログ |

[READMEへ戻る](../../README.md)
