# srvSMS

Servidor open source para envío de SMS usando un módem GSM, comandos AT, una cola MySQL/MariaDB y, opcionalmente, un servidor TCP en Python para integración por red.

Autor: **Marcelo Maurin Martins**  
Sitio web: [http://maurinsoft.com.br](http://maurinsoft.com.br)  
Contacto: [marcelomaurinmartins@gmail.com](mailto:marcelomaurinmartins@gmail.com)

---

## Descripción general

`srvSMS` fue creado para ambientes Linux donde un módem GSM está conectado por puerto serial, normalmente `/dev/ttyUSB0`, y las aplicaciones necesitan enviar SMS desde una cola de base de datos.

El proyecto tiene dos modos principales:

1. **Daemon C `srvSMS`**: lee registros pendientes de la tabla `jobs` y envía SMS por el módem GSM.
2. **Servidor TCP Python `srv/python/srvSMS.py`**: recibe comandos ASCII por TCP y envía SMS directamente por el módem serial.

---

## Requisitos

- Linux Debian, Ubuntu, Raspberry Pi OS o distribución compatible.
- GCC, Make y bibliotecas de desarrollo MySQL/MariaDB.
- Servidor MySQL o MariaDB.
- Módem GSM disponible en un puerto serial.
- Python 3 y PySerial solo si se usa el servidor TCP.

Instalación básica:

```bash
sudo apt-get update
sudo apt-get install build-essential gcc make
sudo apt-get install default-mysql-server default-libmysqlclient-dev
sudo apt-get install mariadb-client libmariadb-dev
sudo apt-get install minicom netcat-openbsd
```

Dependencias Python:

```bash
sudo apt-get install python3 python3-pip
pip3 install pyserial
```

---

## Base de datos

Crear la base y la tabla:

```bash
mysql -u root -p < mysql/srvSMSdb.sql
mysql -u root -p < mysql/jobs.sql
```

Tabla principal:

```sql
CREATE TABLE IF NOT EXISTS `jobs` (
  `idjob` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `telefone` varchar(20) NOT NULL,
  `mensagem` varchar(500) NOT NULL,
  status int default 0,
  PRIMARY KEY (`idjob`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 AUTO_INCREMENT=1;
```

Estados:

| Estado | Significado |
|---:|---|
| `0` | Pendiente. |
| `1` | Enviado correctamente. |
| `9` | Error o teléfono inválido. |

Insertar un SMS:

```sql
USE srvSMSdb;
INSERT INTO jobs (telefone, mensagem, status)
VALUES ('16999999999', 'Mensaje de prueba', 0);
```

---

## Compilación e instalación

```bash
make build
sudo make install
```

Instalación completa:

```bash
sudo make all
```

Archivos instalados:

```text
/usr/local/bin/srvSMS
/etc/init.d/srvSMS
/etc/SMS/sms.cfg
/var/log/SMS
```

---

## Configuración

Archivo principal:

```text
/etc/SMS/sms.cfg
```

Ejemplo:

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
PASSDB: contraseña
ALIASDB: srvSMSdb
SMSC: +5516999999999
```

Campos principales:

| Campo | Descripción |
|---|---|
| `INICIO` | Hora inicial de envío. |
| `FIM` | Hora final de envío. |
| Días de la semana | `1` habilita, `0` deshabilita. |
| `MODO` | `A` consola/aplicación, `S` daemon silencioso. |
| `LOCALDB` | Host de la base de datos. |
| `USERDB` | Usuario de la base. |
| `PASSDB` | Contraseña de la base. |
| `ALIASDB` | Nombre de la base, normalmente `srvSMSdb`. |
| `SMSC` | Centro de mensajes de la operadora. |

Permisos recomendados:

```bash
sudo chown root:root /etc/SMS/sms.cfg
sudo chmod 640 /etc/SMS/sms.cfg
```

---

## Variables de entorno

| Variable | Función |
|---|---|
| `SMS_LOCALDB` | Sobrescribe `LOCALDB`. |
| `SMS_USERDB` | Sobrescribe `USERDB`. |
| `SMS_PASSDB` | Sobrescribe `PASSDB`. |
| `SMS_ALIASDB` | Sobrescribe `ALIASDB`. |
| `SMS_SMSC` | Sobrescribe `SMSC`. |
| `SMS_FORCE_MODO` | Fuerza modo `A` o `S`. |

Ejemplo:

```bash
sudo env SMS_LOCALDB=localhost \
         SMS_USERDB=smsuser \
         SMS_PASSDB='contraseña' \
         SMS_ALIASDB=srvSMSdb \
         SMS_FORCE_MODO=A \
         /usr/local/bin/srvSMS -d /dev/ttyUSB0 -b 9600
```

---

## Ejecución

```bash
./srvSMS -d /dev/ttyUSB0 -b 9600 -m A
```

Parámetros:

| Parámetro | Descripción |
|---|---|
| `-d` | Puerto serial. |
| `-b` | Velocidad baud. |
| `-m` | Modo `A` o `S`. |

---

## Servicio

```bash
sudo service srvSMS start
sudo service srvSMS status
sudo service srvSMS stop
sudo service srvSMS restart
```

Habilitar en el arranque:

```bash
sudo update-rc.d srvSMS defaults
```

---

## Logs

Los logs diarios se guardan en:

```text
/var/log/SMS/YYYY-MM-DD.log
```

El daemon recarga automáticamente `/etc/SMS/sms.cfg` cuando el archivo cambia.

---

## Servidor TCP Python

```bash
cd srv/python
python3 srvSMS.py
```

Configuración generada:

```ini
[CONFIG]
serial_port = /dev/ttyUSB0
baudrate = 9600
tcp_port = 9095
```

Protocolo:

```text
SEND=<numero>#<mensaje><CR>
```

Prueba:

```bash
nc 127.0.0.1 9095
```

---

## Seguridad

- No exponga el puerto TCP directamente en Internet.
- Use firewall, VPN o lista de IPs permitidas.
- Proteja `/etc/SMS/sms.cfg` porque puede contener credenciales.
- Use un usuario MySQL específico y con permisos mínimos.

---

## Licencia

Distribuido bajo **GNU General Public License v3.0**. Consulte [`LICENSE`](LICENSE).
