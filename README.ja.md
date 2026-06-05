# srvSMS

`srvSMS` は、GSM モデム、AT コマンド、MySQL/MariaDB のジョブキューを使用して SMS を送信するオープンソースのサーバーです。必要に応じて、Python TCP サーバーによる簡単なネットワーク連携も利用できます。

作者: **Marcelo Maurin Martins**  
サイト: [http://maurinsoft.com.br](http://maurinsoft.com.br)  
連絡先: [marcelomaurinmartins@gmail.com](mailto:marcelomaurinmartins@gmail.com)

---

## 概要

`srvSMS` は、GSM モデムがシリアルポート、通常は `/dev/ttyUSB0` に接続されている Linux 環境向けに作られています。

主な動作モード:

1. **C daemon `srvSMS`**: MySQL/MariaDB の `jobs` テーブルを読み、GSM モデム経由で SMS を送信します。
2. **Python TCP server `srv/python/srvSMS.py`**: TCP 経由で ASCII コマンドを受け取り、シリアルモデムから直接 SMS を送信します。

---

## 必要条件

- Debian、Ubuntu、Raspberry Pi OS、または互換 Linux。
- GCC、Make、MySQL/MariaDB 開発ライブラリ。
- MySQL または MariaDB サーバー。
- シリアルポートで利用できる GSM モデム。
- TCP サーバーを使う場合のみ Python 3 と PySerial。

```bash
sudo apt-get update
sudo apt-get install build-essential gcc make
sudo apt-get install default-mysql-server default-libmysqlclient-dev
sudo apt-get install mariadb-client libmariadb-dev
sudo apt-get install minicom netcat-openbsd
```

Python 用:

```bash
sudo apt-get install python3 python3-pip
pip3 install pyserial
```

---

## データベース

```bash
mysql -u root -p < mysql/srvSMSdb.sql
mysql -u root -p < mysql/jobs.sql
```

`jobs` テーブルの状態:

| 状態 | 意味 |
|---:|---|
| `0` | 送信待ち。 |
| `1` | 送信成功。 |
| `9` | エラーまたは電話番号が無効。 |

例:

```sql
USE srvSMSdb;
INSERT INTO jobs (telefone, mensagem, status)
VALUES ('16999999999', 'Test message', 0);
```

---

## ビルドとインストール

```bash
make build
sudo make install
```

完全インストール:

```bash
sudo make all
```

---

## 設定

メイン設定ファイル:

```text
/etc/SMS/sms.cfg
```

例:

```text
INICIO: 07:00
FIM: 20:00
SEGUNDA: 1
TERCA: 1
QUARTA: 1
QUINTA: 1
SEXTA: 1
SABADO: 1
DOMINGO: 1
MODO: A
LOCALDB: localhost
USERDB: smsuser
PASSDB: password
ALIASDB: srvSMSdb
SMSC: +5516999999999
```

主な項目:

| 項目 | 説明 |
|---|---|
| `INICIO` | 送信可能時間の開始。 |
| `FIM` | 送信可能時間の終了。 |
| 曜日項目 | `1` は有効、`0` は無効。 |
| `MODO` | `A` はコンソール、`S` はサイレント daemon。 |
| `LOCALDB` | データベースホスト。 |
| `USERDB` | データベースユーザー。 |
| `PASSDB` | データベースパスワード。 |
| `ALIASDB` | データベース名。通常は `srvSMSdb`。 |
| `SMSC` | 通信事業者の SMS センター番号。 |

推奨権限:

```bash
sudo chown root:root /etc/SMS/sms.cfg
sudo chmod 640 /etc/SMS/sms.cfg
```

---

## 環境変数

| 変数 | 説明 |
|---|---|
| `SMS_LOCALDB` | `LOCALDB` を上書きします。 |
| `SMS_USERDB` | `USERDB` を上書きします。 |
| `SMS_PASSDB` | `PASSDB` を上書きします。 |
| `SMS_ALIASDB` | `ALIASDB` を上書きします。 |
| `SMS_SMSC` | `SMSC` を上書きします。 |
| `SMS_FORCE_MODO` | `A` または `S` を強制します。 |

---

## 実行

```bash
./srvSMS -d /dev/ttyUSB0 -b 9600 -m A
```

パラメータ:

| パラメータ | 説明 |
|---|---|
| `-d` | シリアルポート。 |
| `-b` | ボーレート。 |
| `-m` | 実行モード `A` または `S`。 |

---

## サービス

```bash
sudo service srvSMS start
sudo service srvSMS status
sudo service srvSMS stop
sudo service srvSMS restart
```

起動時に有効化:

```bash
sudo update-rc.d srvSMS defaults
```

---

## ログ

日次ログ:

```text
/var/log/SMS/YYYY-MM-DD.log
```

daemon は `/etc/SMS/sms.cfg` の変更を検出し、自動的に再読み込みします。

---

## Python TCP サーバー

```bash
cd srv/python
python3 srvSMS.py
```

プロトコル:

```text
SEND=<number>#<message><CR>
```

テスト:

```bash
nc 127.0.0.1 9095
```

---

## セキュリティ

- TCP ポートを直接インターネットに公開しないでください。
- Firewall、VPN、または許可 IP リストを使用してください。
- `/etc/SMS/sms.cfg` には認証情報が含まれる可能性があるため保護してください。
- 最小権限の MySQL ユーザーを使用してください。

---

## ライセンス

**GNU General Public License v3.0** の下で配布されています。詳細は [`LICENSE`](LICENSE) を参照してください。
