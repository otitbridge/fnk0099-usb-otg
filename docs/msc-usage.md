# MSC操作手順：USBカードリーダー・microSD

FNK0099のUSB-OTG端子へUSBカードリーダーを接続し、FatFs経由で検証用ファイルを読み書きします。確認した機材・合否・時間は[MSC検証結果](msc-validation.md)を参照してください。

## 接続と起動

- PC → USB-UART → FNK0099：給電・書き込み・115200 baudのUART。
- FNK0099 → USB-OTG → カードリーダー → microSD。

[共通のビルド手順](build.md)に従い、環境を明示します。`YOUR_UART_PORT` はローカルで確認したポートへ置き換えます。

```sh
pio run -e fnk0099-msc
pio run -e fnk0099-msc -t upload --upload-port YOUR_UART_PORT
pio device monitor --port YOUR_UART_PORT --baud 115200
```

認識時にメディア情報を取得してマウントを試みます。`MSC_HOST_READY`、`install_device: ESP_OK`、`mount: ESP_OK` を確認します。マウント失敗時は自動フォーマットせず、原因を確認してから進めます。

起動・接続・マウントだけではファイルを作成しません。新規作成は以下の書き込みコマンドで実行します。既存ファイルを上書き・追記・自動削除せず、失敗途中のファイルも残します。この動作は物理的な書き込み保護ではありません。

## UARTコマンド

各コマンドをCRまたはLFで送信し、完了ログを待ってから次へ進みます。シリアルポートを使うプログラムは同時に1つにします。

| コマンド | 動作・合格の目印 |
|---|---|
| `help` | コマンド一覧 |
| `status` | 接続・マウント状態、容量、空きヒープ、稼働時間 |
| `mount` | 認識済みで未マウントのメディアをマウント |
| `ls` | ルートの項目数を出力。既存ファイル名は出力しない |
| `read` | `PROBE.TXT` の内容・長さを照合。`READ_PASS bytes=30` とSHA-256 |
| `write` | `WRITE.TXT` を新規作成し、同期・クローズ・再オープン照合。`WRITE_SYNC_OK` → `WRITE_PASS bytes=31` |
| `verify-write` | `WRITE.TXT` を読み取り専用で照合。`WRITE_VERIFY_PASS bytes=31` |
| `write-1m` | `BULK1M.BIN` を新規作成し、同期・クローズ・再オープン照合。`BULK_SYNC_OK` → `BULK_PASS bytes=1048576` |
| `verify-1m` | `BULK1M.BIN` を読み取り専用で照合。`BULK_PASS bytes=1048576` |
| `unmount` | アンマウント。成功時の `SAFE_TO_REMOVE` を確認して取り外す |

同名ファイルがある場合、`write` は `WRITE_EXISTS`、`write-1m` は `BULK_EXISTS` で拒否します。既存の検証用ファイルの確認には `verify-write`・`verify-1m` を使います。拒否を新規書き込み成功として数えません。

## 検証データ

ファイルはすべてSDルートに置きます。小さい2ファイルはLF終端・BOMなしです。

| ファイル | サイズ | 準備元 |
|---|---|---|
| `PROBE.TXT` | 30バイト | PCから[公開フィクスチャ](../tests/fixtures/msc/PROBE.TXT)を新規コピー |
| `WRITE.TXT` | 31バイト | FNK0099の `write` で作成。[基準内容](../tests/fixtures/msc/WRITE.TXT) |
| `BULK1M.BIN` | 1,048,576バイト | FNK0099の `write-1m` で作成 |

SHA-256は次の値です。

```text
PROBE.TXT  96a1296c1224e6471cd1e6bfc3577a7b91622e19c95d42df2928e1bb6d531d9d
WRITE.TXT  168185ab9768450a0f5cec7d780cb0bdc2b81dbb9784abb37425e5efffca08ff
BULK1M.BIN f395c263d052f99409f2ac4837dfcc5f5c89270e9d347a8306043155dd08ecf7
```

1MiBパターンv1はオフセット `i` に対して次の式です。ファームは4KiBずつ生成・読み取りし、全量の1MiBバッファは確保しません。

```text
byte[i] = (73*i + 151*(i >> 8) + 199*(i >> 16) + 0x5A) mod 256
0 <= i < 1048576
```

## 新しいカードでの試験順序

対象メディアを特定し、残したいデータの扱いを確認してから始めます。カードのフォーマットはこの手順に含めません。同名ファイルとの衝突や各操作の失敗時は停止し、ログを確認します。

1. FNK0099でUSB認識・容量・マウントを確認し、`status` → `ls` → `unmount` を実行します。`SAFE_TO_REMOVE` を待ちます。
2. カードリーダーをPCへ差し替え、`PROBE.TXT` だけをSDルートへ新規コピーし、元データとの一致を確認します。安全にイジェクトしてFNK0099へ戻します。
3. `status` → `read` で30バイト・内容・SHA-256一致を確認します。
4. `write` → `unmount` → `mount` → `verify-write` → `read` で31バイトの新規作成と再マウント後の一致、元ファイルの保持を確認します。
5. `write-1m` → `unmount` → `mount` → `verify-1m` → `read` → `verify-write` → `unmount` で1MiBの新規作成・再マウント照合と小さい2ファイルの保持を確認します。
6. `SAFE_TO_REMOVE` を確認してPCへ差し替え、下記の独立照合を行ってイジェクトします。
7. FNK0099へ戻し、次節の再接続3回・RESET3回を実施します。

PCでの準備例です。リポジトリ直下で実行し、`YOUR_SD_ROOT` を確認済みのSDルートパスへ置き換えます。`xb` により同名ファイルは上書きしません。

```sh
python3 - <<'PY'
import os
from pathlib import Path
sd = Path('YOUR_SD_ROOT')
data = Path('tests/fixtures/msc/PROBE.TXT').read_bytes()
target = sd / 'PROBE.TXT'
with target.open('xb') as out:
    out.write(data)
    out.flush()
    os.fsync(out.fileno())
if target.read_bytes() != data:
    raise SystemExit('PROBE.TXT verification: FAIL')
print('PROBE.TXT verification: PASS')
PY
```

PC照合時は、小さい2ファイルを公開フィクスチャと比較します。

```sh
python3 - <<'PY'
from pathlib import Path
sd = Path('YOUR_SD_ROOT')
for name in ('PROBE.TXT', 'WRITE.TXT'):
    expected = (Path('tests/fixtures/msc') / name).read_bytes()
    if (sd / name).read_bytes() != expected:
        raise SystemExit(f'{name}: FAIL')
    print(f'{name}: PASS')
PY
python3 tools/msc_pattern.py --verify YOUR_SD_BULK1M_FILE
```

`YOUR_SD_BULK1M_FILE` は対象SD上の `BULK1M.BIN` のパスです。[Pythonツール](../tools/msc_pattern.py)はサイズ・SHA-256を読み取り専用で照合します。PC照合後はOSのイジェクトを完了してから取り外します。`WRITE.TXT`・`BULK1M.BIN` をPCで事前作成すると、FNK0099での新規書き込み試験と衝突します。

ローカルの基準バイナリが必要な場合は次のコマンドを使えます。SDへの事前コピーには使用しません。

```sh
mkdir -p .local
python3 tools/msc_pattern.py
python3 tools/msc_pattern.py --output .local/BULK1M-reference.bin
```

`--output` も排他的な新規作成です。生成バイナリをGitへ追加しないでください。

## 再接続・RESET試験

検証用3ファイルが揃った状態で、UARTを開いたまま実施します。

1. `unmount` の `SAFE_TO_REMOVE` を確認し、UART給電を維持してOTG側カードリーダーだけを抜き差しします。
2. 再認識・マウント後、`read` → `verify-write` → `verify-1m` → `unmount` → `status` を実行し、3ファイルの一致と `mounted=0 cleanup_failed=0` を確認します。計3回繰り返します。
3. アンマウント後、両USBを保持して基板のRESETボタンを押します。起動・認識後に同じ照合とアンマウントを行い、計3回繰り返します。

各試行の警告・エラー、所要時間、同じ接続状態での空きヒープを記録します。UART再オープンによるリセットと物理操作を混同しないようにします。後処理失敗で `CLEANUP_FAILED` が出た場合は先へ進まずRESETが必要です。試験結果を電源断耐性や長時間安定性へ一般化しません。

[MSC検証結果](msc-validation.md) / [制約と切り分け](known-issues.md) / [READMEへ戻る](../README.md)
