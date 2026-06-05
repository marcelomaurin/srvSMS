# srvSMS

خادم مفتوح المصدر لإرسال رسائل SMS باستخدام مودم GSM وأوامر AT وقاعدة بيانات MySQL أو MariaDB.

المؤلف: Marcelo Maurin Martins

---

## نظرة عامة

يعمل `srvSMS` على Linux مع مودم GSM متصل بمنفذ تسلسلي مثل:

```text
/dev/ttyUSB0
```

يوجد وضعان رئيسيان:

1. برنامج C باسم `srvSMS` يقرأ جدول `jobs` ويرسل الرسائل.
2. خادم Python باسم `srv/python/srvSMS.py` يستقبل أوامر TCP ويرسل الرسائل مباشرة.

---

## المتطلبات

```bash
sudo apt-get update
sudo apt-get install build-essential gcc make
sudo apt-get install default-mysql-server default-libmysqlclient-dev
sudo apt-get install mariadb-client libmariadb-dev
sudo apt-get install minicom netcat-openbsd
```

لخادم Python:

```bash
sudo apt-get install python3 python3-pip
pip3 install pyserial
```

---

## قاعدة البيانات

```bash
mysql -u root -p < mysql/srvSMSdb.sql
mysql -u root -p < mysql/jobs.sql
```

حالات جدول `jobs`:

| الحالة | المعنى |
|---:|---|
| `0` | في انتظار الإرسال. |
| `1` | تم الإرسال بنجاح. |
| `9` | خطأ أو رقم غير صالح. |

مثال:

```sql
USE srvSMSdb;
INSERT INTO jobs (telefone, mensagem, status)
VALUES ('16999999999', 'Test message', 0);
```

---

## التثبيت

```bash
make build
sudo make install
```

تثبيت كامل:

```bash
sudo make all
```

---

## الإعداد

ملف الإعداد:

```text
/etc/SMS/sms.cfg
```

مثال:

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

---

## التشغيل

```bash
./srvSMS -d /dev/ttyUSB0 -b 9600 -m A
```

---

## الخدمة

```bash
sudo service srvSMS start
sudo service srvSMS status
sudo service srvSMS stop
sudo service srvSMS restart
```

---

## السجلات

```text
/var/log/SMS/YYYY-MM-DD.log
```

---

## خادم Python TCP

```bash
cd srv/python
python3 srvSMS.py
```

البروتوكول:

```text
SEND=<number>#<message><CR>
```

---

## الأمان

لا تعرض منفذ TCP مباشرة على الإنترنت. احم ملف `/etc/SMS/sms.cfg` واستخدم مستخدم MySQL بصلاحيات محدودة.

---

## الترخيص

GNU General Public License v3.0
