# 検証証跡

各JSON・ログはその試験時点の記録です。過去の `pending` や失敗判定は、後続試験で解消されている場合があります。現在の結果は用途別の検証結果を参照してください。公開ログはローカルパス・MACアドレスなどを除去しています。生ログ・機器固有情報はGit対象外の `.local/` に保持します。

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
