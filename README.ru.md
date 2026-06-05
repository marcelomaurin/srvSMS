# srvSMS

Открытый сервер отправки SMS с использованием GSM-модема, AT-команд, очереди MySQL/MariaDB и, при необходимости, TCP-сервера на Python.

Автор: **Marcelo Maurin Martins**  
Сайт: [http://maurinsoft.com.br](http://maurinsoft.com.br)  
Контакт: [marcelomaurinmartins@gmail.com](mailto:marcelomaurinmartins@gmail.com)

---

## Обзор

`srvSMS` предназначен для Linux-сред, где GSM-модем подключен к последовательному порту, например `/dev/ttyUSB0`, а приложения должны отправлять SMS через очередь в базе данных.

Основные режимы:

1. **C daemon `srvSMS`**: читает таблицу `jobs` в MySQL/MariaDB и отправляет SMS через GSM-модем.
2. **Python TCP server `srv/python/srvSMS.py`**: принимает ASCII-команды по TCP и отправляет SMS напрямую через последовательный модем.

---

## Требования

- Linux Debian, Ubuntu, Raspberry Pi OS или совместимая система.
- GCC, Make и библиотеки разработки MySQL/MariaDB.
- Сервер MySQL или MariaDB.
- GSM-модем на последовательном порту.
- Python 3 и PySerial только для TCP-сервера.

```bash
sudo apt-get update
sudo apt-get install build-essential gcc make
sudo apt-get install default-mysql-server default-libmysqlclient-dev
sudo apt-get install mariadb-client libmariadb-dev
sudo apt-get install minicom netcat-openbsd
```

Для Python:

```bash
sudo apt-get install python3 python3-pip
pip3 install pyserial
```

---

## База данных

```bash
mysql -u root -p < mysql/srvSMSdb.sql
mysql -u root -p < mysql/jobs.sql
```

Статусы таблицы `jobs`:

| Статус | Значение |
|---:|---|
| `0` | Ожидает отправки. |
| `1` | Успешно отправлено. |
| `9` | Ошибка или неверный номер. |

Пример:

```sql
USE srvSMSdb;
INSERT INTO jobs (telefone, mensagem, status)
VALUES ('16999999999', 'Test message', 0);
```

---

## Сборка и установка

```bash
make build
sudo make install
```

Полная установка:

```bash
sudo make all
```

---

## Конфигурация

Основной файл:

```text
/etc/SMS/sms.cfg
```

Пример:

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

Поля:

| Поле | Описание |
|---|---|
| `INICIO` | Начало окна отправки. |
| `FIM` | Конец окна отправки. |
| Дни недели | `1` включает отправку, `0` отключает. |
| `MODO` | `A` режим консоли, `S` тихий daemon. |
| `LOCALDB` | Хост базы данных. |
| `USERDB` | Пользователь базы. |
| `PASSDB` | Пароль базы. |
| `ALIASDB` | Имя базы, обычно `srvSMSdb`. |
| `SMSC` | SMS-центр оператора. |

Рекомендуемые права:

```bash
sudo chown root:root /etc/SMS/sms.cfg
sudo chmod 640 /etc/SMS/sms.cfg
```

---

## Переменные окружения

| Переменная | Назначение |
|---|---|
| `SMS_LOCALDB` | Переопределяет `LOCALDB`. |
| `SMS_USERDB` | Переопределяет `USERDB`. |
| `SMS_PASSDB` | Переопределяет `PASSDB`. |
| `SMS_ALIASDB` | Переопределяет `ALIASDB`. |
| `SMS_SMSC` | Переопределяет `SMSC`. |
| `SMS_FORCE_MODO` | Принудительно задает режим `A` или `S`. |

---

## Запуск

```bash
./srvSMS -d /dev/ttyUSB0 -b 9600 -m A
```

Параметры:

| Параметр | Описание |
|---|---|
| `-d` | Последовательный порт. |
| `-b` | Скорость порта. |
| `-m` | Режим `A` или `S`. |

---

## Сервис

```bash
sudo service srvSMS start
sudo service srvSMS status
sudo service srvSMS stop
sudo service srvSMS restart
```

Включить автозапуск:

```bash
sudo update-rc.d srvSMS defaults
```

---

## Логи

Ежедневные логи находятся здесь:

```text
/var/log/SMS/YYYY-MM-DD.log
```

Daemon автоматически перечитывает `/etc/SMS/sms.cfg` при изменении файла.

---

## Python TCP server

```bash
cd srv/python
python3 srvSMS.py
```

Протокол:

```text
SEND=<number>#<message><CR>
```

Тест:

```bash
nc 127.0.0.1 9095
```

---

## Безопасность

- Не открывайте TCP-порт напрямую в Интернет.
- Используйте firewall, VPN или список разрешенных IP.
- Защитите `/etc/SMS/sms.cfg`, так как он может содержать учетные данные.
- Используйте отдельного пользователя MySQL с минимальными правами.

---

## Лицензия

Распространяется под лицензией **GNU General Public License v3.0**. См. [`LICENSE`](LICENSE).
