# srvSMS

Open source SMS sending server using a GSM modem, AT commands, a MySQL/MariaDB job queue and, optionally, a Python TCP server for simple network integration.

Author: **Marcelo Maurin Martins**  
Website: [http://maurinsoft.com.br](http://maurinsoft.com.br)  
Contact: [marcelomaurinmartins@gmail.com](mailto:marcelomaurinmartins@gmail.com)

---

## Overview

`srvSMS` was designed for Linux environments where a GSM modem is connected through a serial port, usually `/dev/ttyUSB0`, and applications need to send SMS messages through a database queue.

The project has two main operating modes:

1. **C daemon `srvSMS`**: reads pending records from the `jobs` table and sends SMS messages through the GSM modem.
2. **Python TCP server `srv/python/srvSMS.py`**: receives ASCII commands through TCP and sends SMS messages directly through the serial modem.

---

## Project structure

```text
.
├── README.md
├── README.en.md
├── LICENSE
├── Makefile
├── srvSMS.c
├── srvSMS_service
├── mysql/
│   ├── srvSMSdb.sql
│   ├── dropdb.sql
│   └── jobs.sql
└── srv/
    └── python/
        └── srvSMS.py
```

Main files:

| File | Purpose |
|---|---|
| `srvSMS.c` | Main C daemon. Reads SMS jobs from MySQL/MariaDB and sends them through a GSM modem. |
| `Makefile` | Builds and installs the daemon, configuration, service, logs and database scripts. |
| `srvSMS_service` | SysV init service script. |
| `mysql/srvSMSdb.sql` | Creates the `srvSMSdb` database. |
| `mysql/jobs.sql` | Creates the `jobs` queue table. |
| `srv/python/srvSMS.py` | Simple TCP SMS server written in Python. |

---

## Requirements

- Linux Debian, Ubuntu, Raspberry Pi OS or compatible distribution.
- GCC, Make and MySQL/MariaDB development libraries.
- MySQL or MariaDB server.
- GSM modem accessible through a serial port.
- Python 3 and PySerial only if using the TCP server.

Install common packages:

```bash
sudo apt-get update
sudo apt-get install build-essential gcc make
sudo apt-get install default-mysql-server default-libmysqlclient-dev
sudo apt-get install mariadb-client libmariadb-dev
sudo apt-get install minicom netcat-openbsd
```

Python TCP server dependencies:

```bash
sudo apt-get install python3 python3-pip
pip3 install pyserial
```

---

## Database

Create the database and queue table:

```bash
mysql -u root -p < mysql/srvSMSdb.sql
mysql -u root -p < mysql/jobs.sql
```

The main queue table is `jobs`:

```sql
CREATE TABLE IF NOT EXISTS `jobs` (
  `idjob` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `telefone` varchar(20) NOT NULL,
  `mensagem` varchar(500) NOT NULL,
  status int default 0,
  PRIMARY KEY (`idjob`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 AUTO_INCREMENT=1;
```

Job status:

| Status | Meaning |
|---:|---|
| `0` | Pending. |
| `1` | Sent successfully. |
| `9` | Error or invalid phone number. |

Insert a test message:

```sql
USE srvSMSdb;

INSERT INTO jobs (telefone, mensagem, status)
VALUES ('16999999999', 'Test SMS message', 0);
```

Brazilian phone numbers are normalized to E.164 format.

---

## Build and install the C daemon

Build:

```bash
make build
```

Install:

```bash
sudo make install
```

Complete installation:

```bash
sudo make all
```

Useful Makefile targets:

| Target | Description |
|---|---|
| `make build` | Builds `srvSMS.c`. |
| `sudo make install` | Installs `/usr/local/bin/srvSMS` and the SysV service. |
| `sudo make install-config` | Creates `/etc/SMS/sms.cfg` if missing. |
| `sudo make install-logdir` | Creates `/var/log/SMS`. |
| `sudo make install-logrotate` | Installs logrotate configuration. |
| `make show-config` | Displays `/etc/SMS/sms.cfg`. |
| `sudo make uninstall` | Removes the binary and service, preserving config and logs. |

---

## Daemon configuration

Default file:

```text
/etc/SMS/sms.cfg
```

Example:

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
PASSDB: database_password
ALIASDB: srvSMSdb
SMSC: +5516999999999
```

Fields:

| Field | Description |
|---|---|
| `INICIO` | Start time for the sending window. |
| `FIM` | End time for the sending window. |
| Weekday fields | `1` enables sending, `0` disables sending. |
| `MODO` | `A` for console/application mode, `S` for silent daemon mode. |
| `LOCALDB` | Database host. |
| `USERDB` | Database user. |
| `PASSDB` | Database password. |
| `ALIASDB` | Database name, usually `srvSMSdb`. |
| `SMSC` | SMS center number, used when the modem does not provide a valid SMSC. |

Recommended permissions:

```bash
sudo chown root:root /etc/SMS/sms.cfg
sudo chmod 640 /etc/SMS/sms.cfg
```

---

## Environment variables

Environment variables override `/etc/SMS/sms.cfg`.

| Variable | Purpose |
|---|---|
| `SMS_LOCALDB` | Overrides `LOCALDB`. |
| `SMS_USERDB` | Overrides `USERDB`. |
| `SMS_PASSDB` | Overrides `PASSDB`. |
| `SMS_ALIASDB` | Overrides `ALIASDB`. |
| `SMS_SMSC` | Overrides `SMSC`. |
| `SMS_FORCE_MODO` | Forces mode `A` or `S`. |

Example:

```bash
sudo env SMS_LOCALDB=localhost \
         SMS_USERDB=smsuser \
         SMS_PASSDB='password' \
         SMS_ALIASDB=srvSMSdb \
         SMS_SMSC='+5516999999999' \
         SMS_FORCE_MODO=A \
         /usr/local/bin/srvSMS -d /dev/ttyUSB0 -b 9600
```

Mode priority:

```text
CLI -m > SMS_FORCE_MODO > MODO from sms.cfg
```

---

## Manual execution

```bash
./srvSMS -d /dev/ttyUSB0 -b 9600 -m A
```

Parameters:

| Parameter | Description |
|---|---|
| `-d` | Serial device. Default: `/dev/ttyUSB0`. |
| `-b` | Baud rate. Default: `9600`. |
| `-m` | Execution mode: `A` or `S`. |

Supported baud rates: `4800`, `9600`, `19200`, `38400`, `57600`, `115200`.

---

## SysV init service

Commands:

```bash
sudo service srvSMS start
sudo service srvSMS status
sudo service srvSMS stop
sudo service srvSMS restart
```

Enable at boot:

```bash
sudo update-rc.d srvSMS defaults
```

If needed, edit `/etc/init.d/srvSMS` and adjust:

```bash
DAEMON_OPTS="-d /dev/ttyUSB0 -b 9600 -m S"
```

---

## Logs and hot reload

Daily logs are stored in:

```text
/var/log/SMS/YYYY-MM-DD.log
```

The daemon watches `/etc/SMS/sms.cfg` and reloads configuration automatically when the file changes.

Reloaded values include sending window, weekdays, database settings, mode and SMSC.

---

## Python TCP server

File:

```text
srv/python/srvSMS.py
```

Run:

```bash
cd srv/python
python3 srvSMS.py
```

Configuration file created on first run:

```ini
[CONFIG]
serial_port = /dev/ttyUSB0
baudrate = 9600
tcp_port = 9095
```

Protocol:

```text
SEND=<number>#<message><CR>
```

Example:

```text
SEND=5516999999999#Hello world\r
```

Possible responses:

| Response | Meaning |
|---|---|
| `OK` | Command accepted. |
| `ERRO` | Send or parsing error. |
| `COMANDO_INVALIDO` | Unknown command. |

Test:

```bash
nc 127.0.0.1 9095
```

---

## GSM modem test

```bash
sudo minicom -D /dev/ttyUSB0 -b 9600
```

Useful AT commands:

```text
AT
AT+CPIN?
AT+CSQ
AT+CREG?
AT+CMGF=1
AT+CSCA?
```

Expected responses include `OK`, `+CPIN: READY`, `+CREG: 0,1` or `+CREG: 0,5`.

---

## Troubleshooting

- If compilation fails, install MySQL/MariaDB development packages.
- If the serial port does not open, check `/dev/ttyUSB*`, permissions and the `dialout` group.
- If the modem does not answer `AT`, test with `minicom` and try another baud rate.
- `+CMS ERROR: 302` may indicate that the carrier or SIM does not allow SMS sending.
- `+CMS ERROR: 330` usually indicates an SMSC problem.
- If jobs remain pending, check the sending window and enabled weekdays.

---

## Security

- Do not expose the Python TCP port directly to the Internet.
- Use firewall, VPN or IP allowlists.
- Protect `/etc/SMS/sms.cfg` because it may contain database credentials.
- Use a dedicated MySQL user with minimum permissions.
- Review logs before sharing them, as they may contain phone numbers and messages.

---

## License

Distributed under the **GNU General Public License v3.0**.

See [`LICENSE`](LICENSE) for details.
