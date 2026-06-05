# srvSMS

`srvSMS` 是一个开源短信发送服务器，使用 GSM 调制解调器、AT 命令、MySQL/MariaDB 队列，并可选提供 Python TCP 服务器用于简单的网络集成。

作者: **Marcelo Maurin Martins**  
网站: [http://maurinsoft.com.br](http://maurinsoft.com.br)  
联系: [marcelomaurinmartins@gmail.com](mailto:marcelomaurinmartins@gmail.com)

---

## 概述

`srvSMS` 适用于 Linux 环境，其中 GSM 调制解调器连接到串口，通常是 `/dev/ttyUSB0`，应用程序通过数据库队列发送短信。

主要模式:

1. **C daemon `srvSMS`**: 从 MySQL/MariaDB 的 `jobs` 表读取待发送短信，并通过 GSM 调制解调器发送。
2. **Python TCP server `srv/python/srvSMS.py`**: 通过 TCP 接收 ASCII 命令，并直接通过串口调制解调器发送短信。

---

## 需求

- Debian、Ubuntu、Raspberry Pi OS 或兼容 Linux。
- GCC、Make 和 MySQL/MariaDB 开发库。
- MySQL 或 MariaDB 服务器。
- 可通过串口访问的 GSM 调制解调器。
- 仅在使用 TCP 服务器时需要 Python 3 和 PySerial。

```bash
sudo apt-get update
sudo apt-get install build-essential gcc make
sudo apt-get install default-mysql-server default-libmysqlclient-dev
sudo apt-get install mariadb-client libmariadb-dev
sudo apt-get install minicom netcat-openbsd
```

Python 依赖:

```bash
sudo apt-get install python3 python3-pip
pip3 install pyserial
```

---

## 数据库

```bash
mysql -u root -p < mysql/srvSMSdb.sql
mysql -u root -p < mysql/jobs.sql
```

`jobs` 表状态:

| 状态 | 含义 |
|---:|---|
| `0` | 等待发送。 |
| `1` | 发送成功。 |
| `9` | 错误或号码无效。 |

示例:

```sql
USE srvSMSdb;
INSERT INTO jobs (telefone, mensagem, status)
VALUES ('16999999999', 'Test message', 0);
```

---

## 编译和安装

```bash
make build
sudo make install
```

完整安装:

```bash
sudo make all
```

---

## 配置

主配置文件:

```text
/etc/SMS/sms.cfg
```

示例:

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

字段说明:

| 字段 | 描述 |
|---|---|
| `INICIO` | 发送时间窗口开始。 |
| `FIM` | 发送时间窗口结束。 |
| 星期字段 | `1` 启用，`0` 禁用。 |
| `MODO` | `A` 控制台模式，`S` 静默 daemon 模式。 |
| `LOCALDB` | 数据库主机。 |
| `USERDB` | 数据库用户。 |
| `PASSDB` | 数据库密码。 |
| `ALIASDB` | 数据库名称，通常为 `srvSMSdb`。 |
| `SMSC` | 运营商短信中心号码。 |

推荐权限:

```bash
sudo chown root:root /etc/SMS/sms.cfg
sudo chmod 640 /etc/SMS/sms.cfg
```

---

## 环境变量

| 变量 | 作用 |
|---|---|
| `SMS_LOCALDB` | 覆盖 `LOCALDB`。 |
| `SMS_USERDB` | 覆盖 `USERDB`。 |
| `SMS_PASSDB` | 覆盖 `PASSDB`。 |
| `SMS_ALIASDB` | 覆盖 `ALIASDB`。 |
| `SMS_SMSC` | 覆盖 `SMSC`。 |
| `SMS_FORCE_MODO` | 强制模式 `A` 或 `S`。 |

---

## 运行

```bash
./srvSMS -d /dev/ttyUSB0 -b 9600 -m A
```

参数:

| 参数 | 描述 |
|---|---|
| `-d` | 串口设备。 |
| `-b` | 波特率。 |
| `-m` | 运行模式 `A` 或 `S`。 |

---

## 服务

```bash
sudo service srvSMS start
sudo service srvSMS status
sudo service srvSMS stop
sudo service srvSMS restart
```

开机启动:

```bash
sudo update-rc.d srvSMS defaults
```

---

## 日志

每日日志保存在:

```text
/var/log/SMS/YYYY-MM-DD.log
```

daemon 会监视 `/etc/SMS/sms.cfg`，文件改变时自动重新加载配置。

---

## Python TCP 服务器

```bash
cd srv/python
python3 srvSMS.py
```

协议:

```text
SEND=<number>#<message><CR>
```

测试:

```bash
nc 127.0.0.1 9095
```

---

## 安全

- 不要将 TCP 端口直接暴露到互联网。
- 使用防火墙、VPN 或允许的 IP 列表。
- 保护 `/etc/SMS/sms.cfg`，因为它可能包含数据库凭据。
- 使用最小权限的专用 MySQL 用户。

---

## 许可证

本项目基于 **GNU General Public License v3.0** 发布。详见 [`LICENSE`](LICENSE)。
