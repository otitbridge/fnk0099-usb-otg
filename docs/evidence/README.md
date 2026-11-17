# 検証証跡

各JSON・ログはその試験時点の記録です。過去の `pending` や失敗判定は、後続試験で解消されている場合があります。現在の結果は用途別の検証結果を参照してください。公開ログはローカルパス・MACアドレスなどを除去しています。生ログ・機器固有情報はGit対象外の `.local/` に保持します。

## Bolt：K950・M750（開発中、2026-11-17）

USB認識・全4IFの記述子取得と受信開始、入力解釈のホストテスト、コマンド過多の修正・再試験を確認しています。K950の固定キー操作、M750の3ボタン・4方向移動・縦ホイール両方向、同時保持・片側解放は合格です。[Bolt検証記録](../bolt-validation.md)を参照してください。

| ファイル | 内容 |
|---|---|
| [bolt-discovery-2026-11-17.json](bolt-discovery-2026-11-17.json) | USB構成、記述子のサイズ・ハッシュ、実機BIN、20秒の起動確認 |
| [bolt-decoder-preparation-2026-11-17.json](bolt-decoder-preparation-2026-11-17.json) | 入力解釈のホストテスト、実機BIN、監視時の失敗・コマンド過多の修正後の再試験。物理入力は未検証 |
| [bolt-keyboard-input-2026-11-17.json](bolt-keyboard-input-2026-11-17.json) | K950のA・左Shift＋A・右矢印、固定8遷移の実機照合 |
| [bolt-mouse-input-2026-11-17.json](bolt-mouse-input-2026-11-17.json) | M750の3ボタン・4方向移動・縦ホイール両方向、最終状態の実機照合 |
| [bolt-simultaneous-attempt1-2026-11-17.json](bolt-simultaneous-attempt1-2026-11-17.json) | 初回は同時保持24.38秒で時間条件未達。後続の再試験で解消 |
| [bolt-simultaneous-attempt2-2026-11-17.json](bolt-simultaneous-attempt2-2026-11-17.json) | 再試験で同時保持74.51秒・片側解放後5.34秒の保持と移動、入力復帰に合格 |

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
