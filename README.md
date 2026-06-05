# srvSMS

Servidor open source para envio de SMS usando modem GSM, comandos AT, fila em MySQL/MariaDB e, opcionalmente, um servidor TCP em Python para integração simples por rede.

O projeto possui duas formas principais de uso:

1. **Daemon C `srvSMS`**: lê a tabela `jobs` no MySQL/MariaDB e envia as mensagens pelo modem GSM conectado à porta serial.
2. **Servidor TCP Python `srv/python/srvSMS.py`**: recebe comandos ASCII via TCP e envia SMS diretamente pelo modem serial.

Autor: **Marcelo Maurin Martins**  
Site: [http://maurinsoft.com.br](http://maurinsoft.com.br)  
Contato: [marcelomaurinmartins@gmail.com](mailto:marcelomaurinmartins@gmail.com)

---

## Sumário

- [Visão geral](#visão-geral)
- [Estrutura do projeto](#estrutura-do-projeto)
- [Requisitos](#requisitos)
- [Banco de dados](#banco-de-dados)
- [Compilação e instalação do daemon C](#compilação-e-instalação-do-daemon-c)
- [Configuração do daemon](#configuração-do-daemon)
- [Execução manual](#execução-manual)
- [Serviço SysV init](#serviço-sysv-init)
- [Logs e logrotate](#logs-e-logrotate)
- [Funcionamento da fila de SMS](#funcionamento-da-fila-de-sms)
- [Servidor TCP Python](#servidor-tcp-python)
- [Testes com modem GSM](#testes-com-modem-gsm)
- [Solução de problemas](#solução-de-problemas)
- [Segurança](#segurança)
- [Licença](#licença)

---

## Visão geral

O `srvSMS` foi criado para ambientes Linux onde um modem GSM fica conectado a uma porta serial, normalmente `/dev/ttyUSB0`, e o sistema precisa enviar SMS a partir de uma fila de banco de dados.

O daemon em C executa o seguinte fluxo:

1. Carrega `/etc/SMS/sms.cfg`.
2. Abre o log diário em `/var/log/SMS`.
3. Conecta ao MySQL/MariaDB.
4. Abre a porta serial do modem.
5. Inicializa o modem com comandos AT.
6. Verifica registro na rede GSM.
7. Verifica/configura SMSC quando necessário.
8. Consulta a tabela `jobs`.
9. Envia mensagens pendentes.
10. Atualiza o status de cada job.

O programa trabalha em **TEXT MODE**, usando comandos como:

```text
AT
ATE0
AT+CMEE=2
AT+CMGF=1
AT+CSCS="GSM"
AT+CREG?
AT+CPIN?
AT+CSQ
AT+CSCA?
AT+CMGS
```

---

## Estrutura do projeto

```text
.
├── README.md
├── LICENSE
├── Makefile
├── srvSMS.c
├── srvSMS_service
├── mysql/
│   ├── srvSMSdb.sql
│   ├── dropdb.sql
│   └── jobs.sql
├── srv/
│   └── python/
│       └── srvSMS.py
└── site/ ou html/
    └── arquivos auxiliares web, quando presentes no checkout
```

Arquivos principais:

| Arquivo | Função |
|---|---|
| `srvSMS.c` | Daemon principal em C. Lê jobs do banco e envia SMS pelo modem GSM. |
| `Makefile` | Compila, instala binário, configura serviço, banco, site, logs e logrotate. |
| `srvSMS_service` | Script SysV init para iniciar/parar o daemon. |
| `mysql/srvSMSdb.sql` | Criação do banco `srvSMSdb`. |
| `mysql/jobs.sql` | Criação da tabela `jobs`. |
| `srv/python/srvSMS.py` | Servidor TCP simples para envio direto via protocolo ASCII. |

---

## Requisitos

### Sistema operacional

- Linux Debian, Ubuntu, Raspberry Pi OS ou distribuição compatível.
- Acesso root ou sudo para instalação em `/usr/local/bin`, `/etc/SMS`, `/etc/init.d` e `/var/log/SMS`.

### Pacotes Linux

```bash
sudo apt-get update
sudo apt-get install build-essential gcc make
sudo apt-get install default-mysql-server default-libmysqlclient-dev
sudo apt-get install mariadb-client libmariadb-dev
sudo apt-get install minicom netcat-openbsd
```

Em alguns ambientes, os nomes dos pacotes podem variar. O `Makefile` tenta detectar automaticamente `mysql_config` ou `mariadb_config` para obter os flags corretos de compilação.

### Python, apenas se usar o servidor TCP

```bash
sudo apt-get install python3 python3-pip
pip3 install pyserial
```

---

## Banco de dados

O projeto usa MySQL/MariaDB.

### Criar banco e tabela manualmente

```bash
mysql -u root -p < mysql/srvSMSdb.sql
mysql -u root -p < mysql/jobs.sql
```

A tabela principal é `jobs`:

```sql
CREATE TABLE IF NOT EXISTS `jobs` (
  `idjob` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `telefone` varchar(20) NOT NULL,
  `mensagem` varchar(500) NOT NULL,
  status int default 0,
  PRIMARY KEY (`idjob`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 AUTO_INCREMENT=1;
```

### Status da tabela `jobs`

| Status | Significado |
|---:|---|
| `0` | Pendente para envio. |
| `1` | Enviado com sucesso. |
| `9` | Erro de envio ou telefone inválido. |

### Inserir SMS na fila

```sql
USE srvSMSdb;

INSERT INTO jobs (telefone, mensagem, status)
VALUES ('16999999999', 'Mensagem de teste', 0);
```

O daemon normaliza telefones brasileiros para E.164 quando recebe:

- `16999999999` → `+5516999999999`
- `5516999999999` → `+5516999999999`

Telefones em formatos inválidos são marcados com `status = 9`.

---

## Compilação e instalação do daemon C

### Compilar

```bash
make build
```

ou:

```bash
make
```

O binário gerado será:

```text
./srvSMS
```

### Instalar binário e serviço

```bash
sudo make install
```

Esse comando instala:

```text
/usr/local/bin/srvSMS
/etc/init.d/srvSMS
```

### Instalação completa pelo Makefile

O alvo `all` executa limpeza, build, instalação, configuração, logrotate, serviço, banco e site:

```bash
sudo make all
```

O alvo `first` também existe e executa o fluxo completo usado pelo projeto:

```bash
sudo make first
```

### Alvos úteis do Makefile

| Alvo | Função |
|---|---|
| `make build` | Compila `srvSMS.c`. |
| `sudo make install` | Instala o binário e o script SysV. |
| `sudo make install-config` | Cria `/etc/SMS/sms.cfg` se não existir. |
| `sudo make install-logdir` | Cria `/var/log/SMS`. |
| `sudo make install-logrotate` | Instala `/etc/logrotate.d/srvSMS`. |
| `sudo make service-enable` | Habilita serviço com `update-rc.d`. |
| `sudo make service-disable` | Remove serviço da inicialização. |
| `make clean` | Remove binário local. |
| `sudo make uninstall` | Remove binário/serviço, preservando config e logs. |
| `make show-config` | Mostra o conteúdo de `/etc/SMS/sms.cfg`. |

---

## Configuração do daemon

Arquivo padrão:

```text
/etc/SMS/sms.cfg
```

Se o arquivo não existir, o programa tenta criá-lo automaticamente na primeira execução.

Exemplo recomendado:

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
PASSDB: senha_do_banco
ALIASDB: srvSMSdb
SMSC: +5516999999999
```

### Campos do arquivo

| Campo | Descrição |
|---|---|
| `INICIO` | Hora inicial da janela de envio, formato `hh:mm`. |
| `FIM` | Hora final da janela de envio, formato `hh:mm`. |
| `SEGUNDA` a `DOMINGO` | Define se envia naquele dia: `1` habilita, `0` bloqueia. |
| `MODO` | `A` aplicação/console; `S` daemon silencioso. |
| `LOCALDB` | Host do MySQL/MariaDB. |
| `USERDB` | Usuário do banco. |
| `PASSDB` | Senha do banco. |
| `ALIASDB` | Nome do banco. Normalmente `srvSMSdb`. |
| `SMSC` | Centro de mensagens da operadora, no formato `+55...`, quando o modem não retornar SMSC válido. |

### Permissões recomendadas

```bash
sudo chown root:root /etc/SMS/sms.cfg
sudo chmod 640 /etc/SMS/sms.cfg
```

Use `chmod 600` se quiser restringir ainda mais o acesso, principalmente quando houver senha do banco no arquivo.

---

## Variáveis de ambiente

As variáveis de ambiente têm prioridade sobre o arquivo `/etc/SMS/sms.cfg`.

| Variável | Função |
|---|---|
| `SMS_LOCALDB` | Sobrescreve `LOCALDB`. |
| `SMS_USERDB` | Sobrescreve `USERDB`. |
| `SMS_PASSDB` | Sobrescreve `PASSDB`. |
| `SMS_ALIASDB` | Sobrescreve `ALIASDB`. |
| `SMS_SMSC` | Sobrescreve `SMSC`. |
| `SMS_FORCE_MODO` | Força modo `A` ou `S`. |

Exemplo:

```bash
sudo env SMS_LOCALDB=localhost \
         SMS_USERDB=smsuser \
         SMS_PASSDB='senha' \
         SMS_ALIASDB=srvSMSdb \
         SMS_SMSC='+5516999999999' \
         SMS_FORCE_MODO=A \
         /usr/local/bin/srvSMS -d /dev/ttyUSB0 -b 9600
```

Prioridade efetiva do modo:

```text
CLI -m > SMS_FORCE_MODO > MODO do sms.cfg
```

---

## Execução manual

Sintaxe suportada pelo binário:

```bash
./srvSMS -d <porta_serial> -b <baudrate> -m <modo>
```

Exemplos:

```bash
./srvSMS -d /dev/ttyUSB0 -b 9600 -m A
```

```bash
sudo /usr/local/bin/srvSMS -d /dev/ttyUSB0 -b 115200 -m S
```

Parâmetros:

| Parâmetro | Descrição |
|---|---|
| `-d` | Porta serial do modem. Padrão no código: `/dev/ttyUSB0`. |
| `-b` | Baudrate. Padrão: `9600`. |
| `-m` | Modo de execução: `A` ou `S`. |

Baudrates tratados no código:

```text
4800, 9600, 19200, 38400, 57600, 115200
```

---

## Serviço SysV init

O script de serviço é:

```text
srvSMS_service
```

Na instalação, ele é copiado para:

```text
/etc/init.d/srvSMS
```

Comandos:

```bash
sudo service srvSMS start
sudo service srvSMS status
sudo service srvSMS stop
sudo service srvSMS restart
```

Habilitar inicialização automática:

```bash
sudo update-rc.d srvSMS defaults
```

Remover da inicialização:

```bash
sudo update-rc.d srvSMS remove
```

Se precisar fixar porta e baudrate no serviço, edite o arquivo `/etc/init.d/srvSMS` e ajuste:

```bash
DAEMON_OPTS="-d /dev/ttyUSB0 -b 9600 -m S"
```

---

## Logs e logrotate

O daemon grava logs diários em:

```text
/var/log/SMS/YYYY-MM-DD.log
```

O `Makefile` também instala uma configuração de logrotate em:

```text
/etc/logrotate.d/srvSMS
```

Política padrão:

```text
daily
rotate 30
compress
missingok
notifempty
create 0640 root root
```

Modo de log:

| Modo | Comportamento |
|---|---|
| `A` | Mostra logs no terminal e também grava no log diário. |
| `S` | Redireciona saída para log, indicado para execução como daemon. |

---

## Hot-reload de configuração

O `srvSMS` monitora o `mtime` de:

```text
/etc/SMS/sms.cfg
```

Quando o arquivo é alterado, o daemon recarrega:

- janela de envio;
- dias habilitados;
- dados de banco;
- modo `A`/`S`;
- SMSC.

Se o modo mudar, o log é reconfigurado sem reiniciar o processo.

Mensagens esperadas:

```text
Config recarregada (MODO=S) e log reconfigurado.
Config recarregada (MODO=A) sem mudança de modo.
```

---

## Funcionamento da fila de SMS

A função principal de processamento busca:

```sql
SELECT idjob, telefone, mensagem, status FROM jobs WHERE status = 0;
```

Para cada registro:

1. Lê `idjob`, `telefone` e `mensagem`.
2. Normaliza o telefone para formato brasileiro E.164.
3. Remove quebras de linha da mensagem.
4. Envia o SMS pelo modem.
5. Em caso de falha, tenta uma segunda vez.
6. Atualiza o status:
   - `1` para sucesso;
   - `9` para erro.

O loop respeita a janela definida em `INICIO`, `FIM` e dias da semana.

---

## Servidor TCP Python

Arquivo:

```text
srv/python/srvSMS.py
```

Esse servidor abre uma porta TCP e permite enviar SMS por um protocolo ASCII simples.

### Configuração

Na primeira execução, ele cria:

```text
setup.cfg
```

Exemplo:

```ini
[CONFIG]
serial_port = COM13
baudrate = 9600
tcp_port = 9095
```

Em Linux, ajuste a porta serial, por exemplo:

```ini
[CONFIG]
serial_port = /dev/ttyUSB0
baudrate = 9600
tcp_port = 9095
```

### Executar

```bash
cd srv/python
python3 srvSMS.py
```

### Protocolo TCP

Formato:

```text
SEND=<numero>#<mensagem><CR>
```

Exemplo:

```text
SEND=5516999999999#Ola mundo\r
```

Respostas possíveis:

| Resposta | Significado |
|---|---|
| `OK` | Comando aceito e enviado ao modem. |
| `ERRO` | Falha no envio ou erro no parse. |
| `COMANDO_INVALIDO` | Comando não reconhecido. |

### Teste com netcat

```bash
nc 127.0.0.1 9095
```

Depois envie:

```text
SEND=5516999999999#Teste de SMS
```

### Observação importante

O servidor Python é simples e envia a mensagem diretamente via comandos AT. Ele não usa a tabela `jobs` e não implementa a mesma robustez do daemon C, como fila, janela de envio, hot-reload, log diário e tratamento detalhado de `+CMS ERROR`.

---

## Testes com modem GSM

Instale o `minicom`:

```bash
sudo apt-get install minicom
```

Abra a porta do modem:

```bash
sudo minicom -D /dev/ttyUSB0 -b 9600
```

Teste comandos AT:

```text
AT
AT+CPIN?
AT+CSQ
AT+CREG?
AT+CMGF=1
AT+CSCA?
```

Respostas esperadas:

```text
OK
+CPIN: READY
+CREG: 0,1
+CREG: 0,5
```

`CREG = 1` indica registrado na rede local.  
`CREG = 5` indica registrado em roaming.

---

## Solução de problemas

### Erro ao compilar por falta de `mysql_config` ou `mariadb_config`

Instale os pacotes de desenvolvimento:

```bash
sudo apt-get install default-libmysqlclient-dev
```

ou:

```bash
sudo apt-get install libmariadb-dev
```

### Erro ao abrir porta serial

Verifique se o dispositivo existe:

```bash
ls -l /dev/ttyUSB*
ls -l /dev/ttyACM*
```

Verifique permissões:

```bash
sudo usermod -aG dialout $USER
```

Faça logout/login após adicionar o usuário ao grupo.

### Modem não responde `AT`

Teste no `minicom`:

```bash
sudo minicom -D /dev/ttyUSB0 -b 9600
```

Tente outros baudrates:

```text
9600
19200
38400
57600
115200
```

### `+CMS ERROR: 302`

Possíveis causas:

- operadora bloqueia envio de SMS;
- chip M2M/dados não permite SMS;
- falta de crédito ou plano sem SMS;
- SIM não registrado corretamente;
- SMSC ausente ou inválido.

Verifique:

```text
AT+CPIN?
AT+CSQ
AT+CREG?
AT+CSCA?
```

### `+CMS ERROR: 330`

Indica problema com SMSC.

Configure no `/etc/SMS/sms.cfg`:

```text
SMSC: +5516999999999
```

ou use variável de ambiente:

```bash
SMS_SMSC='+5516999999999'
```

### Banco não conecta

Confira:

```text
LOCALDB
USERDB
PASSDB
ALIASDB
```

Teste manualmente:

```bash
mysql -h localhost -u smsuser -p srvSMSdb
```

### Jobs ficam pendentes

Verifique:

```sql
SELECT * FROM jobs WHERE status = 0;
```

Confira se o horário atual está dentro da janela configurada em `INICIO` e `FIM`, e se o dia da semana está habilitado.

---

## Segurança

Recomendações:

- Não exponha a porta TCP Python diretamente na internet.
- Use firewall, VPN ou restrição por IP.
- Proteja `/etc/SMS/sms.cfg`, pois ele pode conter senha do banco.
- Crie um usuário MySQL específico para o `srvSMS`, evitando usar `root` em produção.
- Restrinja permissões da porta serial e do serviço.
- Revise logs antes de compartilhar, pois podem conter números de telefone e mensagens.

Exemplo de usuário MySQL com permissões mínimas:

```sql
CREATE USER 'smsuser'@'localhost' IDENTIFIED BY 'senha_forte';
GRANT SELECT, INSERT, UPDATE ON srvSMSdb.jobs TO 'smsuser'@'localhost';
FLUSH PRIVILEGES;
```

---

## Limitações conhecidas

- O daemon C foi projetado para Linux/POSIX.
- O foco é modem GSM via porta serial.
- O telefone é normalizado para padrão brasileiro E.164.
- A tabela `jobs` usa MyISAM no script atual.
- O servidor Python é funcional, mas mais simples que o daemon C.
- O serviço fornecido usa SysV init; em distribuições modernas pode ser desejável criar uma unit `systemd`.

---

## Roadmap sugerido

Melhorias futuras recomendadas:

- Criar unit `systemd` oficial.
- Migrar tabela `jobs` para InnoDB.
- Adicionar campo de data/hora de envio e mensagem de erro.
- Registrar quantidade de tentativas por job.
- Criar autenticação no servidor TCP Python.
- Separar configurações sensíveis em arquivo com permissões restritas.
- Adicionar testes automatizados de parsing e normalização de telefone.

---

## Licença

Distribuído sob a licença **GNU General Public License v3.0**.

Consulte o arquivo [`LICENSE`](LICENSE) para mais detalhes.

---

## Créditos

Desenvolvido por **Marcelo Maurin Martins**.

Projeto Maurinsoft: [http://maurinsoft.com.br](http://maurinsoft.com.br)
