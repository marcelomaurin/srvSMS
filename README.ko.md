# srvSMS

`srvSMS`는 GSM 모뎀, AT 명령, MySQL/MariaDB 큐를 사용하여 SMS를 전송하는 오픈 소스 서버입니다. 선택적으로 Python TCP 서버를 통해 간단한 네트워크 연동도 제공합니다.

작성자: **Marcelo Maurin Martins**  
웹사이트: [http://maurinsoft.com.br](http://maurinsoft.com.br)  
연락처: [marcelomaurinmartins@gmail.com](mailto:marcelomaurinmartins@gmail.com)

---

## 개요

`srvSMS`는 GSM 모뎀이 직렬 포트, 일반적으로 `/dev/ttyUSB0`에 연결된 Linux 환경을 위해 만들어졌습니다.

주요 동작 모드:

1. **C daemon `srvSMS`**: MySQL/MariaDB의 `jobs` 테이블을 읽고 GSM 모뎀을 통해 SMS를 전송합니다.
2. **Python TCP server `srv/python/srvSMS.py`**: TCP로 ASCII 명령을 받아 직렬 모뎀으로 SMS를 직접 전송합니다.

---

## 요구 사항

- Debian, Ubuntu, Raspberry Pi OS 또는 호환 Linux.
- GCC, Make, MySQL/MariaDB 개발 라이브러리.
- MySQL 또는 MariaDB 서버.
- 직렬 포트로 접근 가능한 GSM 모뎀.
- TCP 서버를 사용할 경우 Python 3 및 PySerial.

```bash
sudo apt-get update
sudo apt-get install build-essential gcc make
sudo apt-get install default-mysql-server default-libmysqlclient-dev
sudo apt-get install mariadb-client libmariadb-dev
sudo apt-get install minicom netcat-openbsd
```

Python 의존성:

```bash
sudo apt-get install python3 python3-pip
pip3 install pyserial
```

---

## 데이터베이스

```bash
mysql -u root -p < mysql/srvSMSdb.sql
mysql -u root -p < mysql/jobs.sql
```

`jobs` 테이블 상태:

| 상태 | 의미 |
|---:|---|
| `0` | 전송 대기. |
| `1` | 전송 성공. |
| `9` | 오류 또는 잘못된 전화번호. |

예시:

```sql
USE srvSMSdb;
INSERT INTO jobs (telefone, mensagem, status)
VALUES ('16999999999', 'Test message', 0);
```

---

## 빌드 및 설치

```bash
make build
sudo make install
```

전체 설치:

```bash
sudo make all
```

---

## 설정

기본 설정 파일:

```text
/etc/SMS/sms.cfg
```

예시:

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

필드:

| 필드 | 설명 |
|---|---|
| `INICIO` | 전송 가능 시간 시작. |
| `FIM` | 전송 가능 시간 종료. |
| 요일 필드 | `1` 활성화, `0` 비활성화. |
| `MODO` | `A` 콘솔 모드, `S` 무음 daemon 모드. |
| `LOCALDB` | 데이터베이스 호스트. |
| `USERDB` | 데이터베이스 사용자. |
| `PASSDB` | 데이터베이스 비밀번호. |
| `ALIASDB` | 데이터베이스 이름. 보통 `srvSMSdb`. |
| `SMSC` | 통신사 SMS 센터 번호. |

권장 권한:

```bash
sudo chown root:root /etc/SMS/sms.cfg
sudo chmod 640 /etc/SMS/sms.cfg
```

---

## 환경 변수

| 변수 | 기능 |
|---|---|
| `SMS_LOCALDB` | `LOCALDB`를 덮어씁니다. |
| `SMS_USERDB` | `USERDB`를 덮어씁니다. |
| `SMS_PASSDB` | `PASSDB`를 덮어씁니다. |
| `SMS_ALIASDB` | `ALIASDB`를 덮어씁니다. |
| `SMS_SMSC` | `SMSC`를 덮어씁니다. |
| `SMS_FORCE_MODO` | 모드 `A` 또는 `S`를 강제합니다. |

---

## 실행

```bash
./srvSMS -d /dev/ttyUSB0 -b 9600 -m A
```

파라미터:

| 파라미터 | 설명 |
|---|---|
| `-d` | 직렬 포트. |
| `-b` | Baud rate. |
| `-m` | 실행 모드 `A` 또는 `S`. |

---

## 서비스

```bash
sudo service srvSMS start
sudo service srvSMS status
sudo service srvSMS stop
sudo service srvSMS restart
```

부팅 시 자동 실행:

```bash
sudo update-rc.d srvSMS defaults
```

---

## 로그

일별 로그 위치:

```text
/var/log/SMS/YYYY-MM-DD.log
```

daemon은 `/etc/SMS/sms.cfg` 변경을 감지하고 자동으로 설정을 다시 읽습니다.

---

## Python TCP 서버

```bash
cd srv/python
python3 srvSMS.py
```

프로토콜:

```text
SEND=<number>#<message><CR>
```

테스트:

```bash
nc 127.0.0.1 9095
```

---

## 보안

- TCP 포트를 인터넷에 직접 노출하지 마십시오.
- 방화벽, VPN 또는 허용 IP 목록을 사용하십시오.
- `/etc/SMS/sms.cfg`에는 자격 증명이 포함될 수 있으므로 보호하십시오.
- 최소 권한을 가진 전용 MySQL 사용자를 사용하십시오.

---

## 라이선스

**GNU General Public License v3.0**에 따라 배포됩니다. 자세한 내용은 [`LICENSE`](LICENSE)를 참조하십시오.
