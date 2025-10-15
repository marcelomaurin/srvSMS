# 📡 Sistema de Envio de SMS – Servidor TCP + Modem GSM

Este software é **open source**, licenciado sob a **GPL v3**.  
Permite o envio de mensagens SMS através de um **modem GSM conectado via porta serial**, controlado por um **servidor TCP ASCII** ou por integração direta com o MySQL/SMSTools.

📬 Para dúvidas ou feedback, envie um e-mail para:  
👉 **marcelomaurinmartins@gmail.com**

---

## 🧰 Requisitos de Instalação

- [x] **SMSTools**  
- [x] **Compilador GCC**  
- [x] **MySQL 5** ou superior  
- [x] **Python 3.8+**  
- [x] **Biblioteca PySerial** (`pip install pyserial`)  
- [x] Ambiente **Linux Debian** ou equivalente

---

## 📦 Instalação – Componente C (`srvSMS`)

O componente `srvSMS.c` gerencia a integração direta com o banco MySQL e o SMSTools.

### Passos

1. **Instalar SMSTools e ferramentas auxiliares**
   ```bash
   sudo apt-get install build-essential libusb-1.0 libusb-1.0-0-dev
   sudo apt-get install smstools
   ```

2. **Instalar MySQL**
   ```bash
   sudo apt-get install mysql-server
   ```

3. **Criar o banco de dados**
   - Use o arquivo `.sql` fornecido:
     ```bash
     mysql -u root -p < estrutura.sql
     ```

4. **Editar `srvSMS.c`**
   - Ajuste os parâmetros de conexão com o banco (`host`, `usuario`, `senha`, `banco`).

5. **Identificar a porta do modem GSM**
   ```bash
   sudo apt-get install minicom
   minicom -D /dev/ttyS0
   ```
   - Digite `AT` e pressione **Enter**. Se responder `OK`, está correto.

6. **Compilar**
   ```bash
   make compile
   ```

7. **Instalar**
   ```bash
   sudo make install
   ```

8. ✅ Pronto!

3) Configuração – /etc/SMS/sms.cfg

Na primeira execução, o srvSMS cria o arquivo com defaults. Edite conforme seu ambiente.

Formato:

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
LOCALDB:
USERDB:
PASSDB:
ALIASDB:


Campos obrigatórios:

LOCALDB / USERDB / PASSDB / ALIASDB → conexão MySQL (host/usuário/senha/banco).

MODO:

A = Aplicação (mostra logs na tela e grava em /var/log/SMS/…)

S = Daemon (silencioso: redireciona stdout/err para o log diário)

Janela de envio: INICIO/FIM (hh:mm) + dias (0/1).

Segurança (recomendado):

sudo chown root:root /etc/SMS/sms.cfg
sudo chmod 600 /etc/SMS/sms.cfg

4) Overrides por Variáveis de Ambiente

Têm prioridade sobre o arquivo de config:

SMS_LOCALDB, SMS_USERDB, SMS_PASSDB, SMS_ALIASDB

Forçar modo: SMS_FORCE_MODO=S (ou A)

Exemplo:

sudo env SMS_LOCALDB=localhost \
         SMS_USERDB=smsuser \
         SMS_PASSDB='senha' \
         SMS_ALIASDB=SMSdb \
         SMS_FORCE_MODO=S \
         /usr/local/bin/srvSMS

5) Execução
# modo manual (mostra ajuda das opções)
./srvSMS -h   # (se implementado) ou rode com -d/-b/-m conforme abaixo

# selecionar porta e baud
./srvSMS -d /dev/ttyUSB0 -b 9600 -m A   # força MODO=A via CLI


Prioridade do MODO: CLI -m > ENV SMS_FORCE_MODO > MODO do sms.cfg.

6) Serviço (SysV init)

Instalação já cria /etc/init.d/srvSMS. Comandos típicos:

sudo service srvSMS start
sudo service srvSMS status
sudo service srvSMS stop


Ao rodar como serviço, prefira MODO: S no config (silencioso).
Os logs ficam sempre em /var/log/SMS/YYYY-MM-DD.log.

🔁 Hot-Reload de Configuração

O srvSMS monitora o mtime de /etc/SMS/sms.cfg.
Ao detectar mudança, ele:

Recarrega a config (janela/dias/DB/MODO);

Reaplica o MODO imediatamente:

se mudar para S, para de escrever no terminal e passa a redirecionar tudo ao log diário;

se mudar para A, volta a escrever no terminal e no log.

Loga mensagens como:

Config recarregada (MODO=S) e log reconfigurado.


🔁 Hot-Reload de Configuração

O srvSMS monitora o mtime de /etc/SMS/sms.cfg.
Ao detectar mudança, ele:

Recarrega a config (janela/dias/DB/MODO);

Reaplica o MODO imediatamente:

se mudar para S, para de escrever no terminal e passa a redirecionar tudo ao log diário;

se mudar para A, volta a escrever no terminal e no log.

Loga mensagens como:

Config recarregada (MODO=S) e log reconfigurado.



## 🌐 Instalação – Servidor TCP Python (`servidor_sms.py`)

Este servidor permite o envio de SMS via protocolo ASCII em rede.

### Comando de protocolo

```
SEND=<numero>#<mensagem><CR>
```

Exemplo:
```
SEND=5511999999999#Olá, teste de SMS\r
```

### Instalação

1. **Instalar dependências Python**
   ```bash
   sudo apt-get install python3 python3-pip
   pip3 install pyserial
   ```

2. **Colocar o arquivo `servidor_sms.py` em `/opt/smsserver`** (ou pasta desejada)

3. **Rodar o servidor**
   ```bash
   python3 servidor_sms.py
   ```

4. Na primeira execução será criado um arquivo `setup.cfg` com valores padrão:
   ```ini
   [CONFIG]
   serial_port = /dev/ttyS0
   baudrate = 9600
   tcp_port = 9095
   ```

5. **Editar `setup.cfg`** conforme sua porta serial e configurações locais.

6. **Testar via telnet/netcat**
   ```bash
   nc 127.0.0.1 9095
   ```
   ou
   ```bash
   telnet 127.0.0.1 9095
   ```

7. Enviar:
   ```
   SEND=5511999999999#Olá mundo\r
   ```

---

## 🛠️ Ferramentas Auxiliares

Instale pacotes adicionais necessários:

```bash
sudo apt-get install build-essential libusb-1.0 libusb-1.0-0-dev
sudo apt-get install smstools minicom
```

---

## 🔒 Segurança

- Proteja a porta TCP (padrão 9095) usando firewall se exposta.  
- `setup.cfg` não deve conter senhas.  
- Para uso externo, restrinja IPs confiáveis ou use VPN.

---

## 📝 Licença

Distribuído sob a **GNU General Public License v3 (GPL-3.0)**.  
Para mais detalhes:  
👉 [https://www.gnu.org/licenses/gpl-3.0.html](https://www.gnu.org/licenses/gpl-3.0.html)

---

## ✨ Créditos

Autor: **Marcelo Maurin Martins**  
📧 [marcelomaurinmartins@gmail.com](mailto:marcelomaurinmartins@gmail.com)
