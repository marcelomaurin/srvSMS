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

---

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
