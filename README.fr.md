# srvSMS

Serveur open source pour envoyer des SMS avec un modem GSM, des commandes AT, une file MySQL/MariaDB et, en option, un serveur TCP Python.

Auteur: **Marcelo Maurin Martins**  
Site: [http://maurinsoft.com.br](http://maurinsoft.com.br)  
Contact: [marcelomaurinmartins@gmail.com](mailto:marcelomaurinmartins@gmail.com)

---

## Vue generale

`srvSMS` fonctionne sous Linux avec un modem GSM connecte a un port serie, par exemple `/dev/ttyUSB0`.

Modes principaux:

1. **Daemon C `srvSMS`**: lit la table `jobs` dans MySQL/MariaDB et envoie les SMS.
2. **Serveur TCP Python `srv/python/srvSMS.py`**: recoit des commandes ASCII via TCP et envoie les SMS par le modem serie.

---

## Prerequis

- Linux Debian, Ubuntu, Raspberry Pi OS ou compatible.
- GCC, Make et bibliotheques MySQL/MariaDB.
- Serveur MySQL ou MariaDB.
- Modem GSM sur port serie.
- Python 3 et PySerial seulement pour le serveur TCP.

```bash
sudo apt-get update
sudo apt-get install build-essential gcc make
sudo apt-get install default-mysql-server default-libmysqlclient-dev
sudo apt-get install mariadb-client libmariadb-dev
sudo apt-get install minicom netcat-openbsd
```

Pour Python:

```bash
sudo apt-get install python3 python3-pip
pip3 install pyserial
```

---

## Base de donnees

```bash
mysql -u root -p < mysql/srvSMSdb.sql
mysql -u root -p < mysql/jobs.sql
```

Etats de la table `jobs`:

| Etat | Signification |
|---:|---|
| `0` | En attente. |
| `1` | Envoye avec succes. |
| `9` | Erreur ou numero invalide. |

Exemple:

```sql
USE srvSMSdb;
INSERT INTO jobs (telefone, mensagem, status)
VALUES ('16999999999', 'Message de test', 0);
```

---

## Compilation et installation

```bash
make build
sudo make install
```

Installation complete:

```bash
sudo make all
```

---

## Configuration

Fichier principal:

```text
/etc/SMS/sms.cfg
```

Exemple:

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

Champs:

| Champ | Description |
|---|---|
| `INICIO` | Debut de la fenetre d'envoi. |
| `FIM` | Fin de la fenetre d'envoi. |
| Jours | `1` active, `0` desactive. |
| `MODO` | `A` console, `S` daemon silencieux. |
| `LOCALDB` | Hote de la base. |
| `USERDB` | Utilisateur. |
| `PASSDB` | Mot de passe. |
| `ALIASDB` | Nom de la base. |
| `SMSC` | Centre de messages SMS de l'operateur. |

Permissions recommandees:

```bash
sudo chown root:root /etc/SMS/sms.cfg
sudo chmod 640 /etc/SMS/sms.cfg
```

---

## Variables d'environnement

| Variable | Fonction |
|---|---|
| `SMS_LOCALDB` | Remplace `LOCALDB`. |
| `SMS_USERDB` | Remplace `USERDB`. |
| `SMS_PASSDB` | Remplace `PASSDB`. |
| `SMS_ALIASDB` | Remplace `ALIASDB`. |
| `SMS_SMSC` | Remplace `SMSC`. |
| `SMS_FORCE_MODO` | Force le mode `A` ou `S`. |

---

## Execution

```bash
./srvSMS -d /dev/ttyUSB0 -b 9600 -m A
```

---

## Service

```bash
sudo service srvSMS start
sudo service srvSMS status
sudo service srvSMS stop
sudo service srvSMS restart
```

Activer au demarrage:

```bash
sudo update-rc.d srvSMS defaults
```

---

## Journaux

Les logs quotidiens sont dans:

```text
/var/log/SMS/YYYY-MM-DD.log
```

Le daemon recharge automatiquement `/etc/SMS/sms.cfg` quand le fichier change.

---

## Serveur TCP Python

```bash
cd srv/python
python3 srvSMS.py
```

Protocole:

```text
SEND=<numero>#<message><CR>
```

Test:

```bash
nc 127.0.0.1 9095
```

---

## Securite

- Ne pas exposer le port TCP directement sur Internet.
- Utiliser un pare-feu, un VPN ou une liste IP autorisee.
- Proteger `/etc/SMS/sms.cfg`.
- Utiliser un utilisateur MySQL avec permissions minimales.

---

## Licence

Distribue sous **GNU General Public License v3.0**. Voir [`LICENSE`](LICENSE).
