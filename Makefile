# ====== Build ======
CC        := gcc
SRC       := srvSMS.c
BIN       := srvSMS

# Auto-descoberta dos flags de MySQL/MariaDB (preferir mysql_config; senão mariadb_config; senão fallback)
MYSQL_CONFIG    := $(shell command -v mysql_config 2>/dev/null)
MARIADB_CONFIG  := $(shell command -v mariadb_config 2>/dev/null)

ifeq ($(MYSQL_CONFIG),)
  ifeq ($(MARIADB_CONFIG),)
    $(warning mysql_config/mariadb_config não encontrados; usando defaults)
    MYSQLCFLAGS ?= -I/usr/include/mysql -DBIG_JOINS=1 -fno-strict-aliasing
    MYSQLLIBS   ?= -lmysqlclient
  else
    MYSQLCFLAGS := $(shell $(MARIADB_CONFIG) --cflags)
    MYSQLLIBS   := $(shell $(MARIADB_CONFIG) --libs)
  endif
else
  MYSQLCFLAGS := $(shell $(MYSQL_CONFIG) --cflags)
  MYSQLLIBS   := $(shell $(MYSQL_CONFIG) --libs)
endif

CFLAGS   += -O2 -g -Wall -Wextra $(MYSQLCFLAGS)
LDLIBS   += $(MYSQLLIBS) -lpthread -lz -lm -ldl

# ====== Instalação ======
PREFIX    ?= /usr/local
BINDIR    ?= $(PREFIX)/bin

# Config (/etc/SMS)
CFG_DIR   ?= /etc/SMS
CFG_FILE  ?= $(CFG_DIR)/sms.cfg
CFG_SRC   ?= config/sms.cfg     # se existir no repo, será usado; senão gera default

# Logs (/var/log/SMS) + logrotate
LOG_DIR         ?= /var/log/SMS
LOGROTATE_FILE  ?= /etc/logrotate.d/srvSMS

# ====== Config do Banco / Site (opcional) ======
HOST      ?= maurinsoft.com.br
USER      ?= usuario
PASSWORD  ?= senha

SITE      ?= /var/www/html
PASTA     ?= SMS

MYSQL_CMD = MYSQL_PWD='$(PASSWORD)' mysql -u $(USER) -h $(HOST)

# ====== Alvos ======
.PHONY: all default build install install-config etcdir \
        install-logdir install-logrotate uninstall-log \
        service-enable service-disable clean uninstall \
        mysql create drop tables newapache_dir apache2 run show-config

default: first
all: clean build install install-config install-logdir install-logrotate service-enable apache2

first: build install install-config install-logdir install-logrotate service-enable create tables newapache_dir apache2 run

build: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

install: $(BIN)
	install -Dm755 $(BIN) $(BINDIR)/$(BIN)
	# Instala script SysV (ajuste se usar systemd)
	install -Dm755 srvSMS_service /etc/init.d/srvSMS || true

# ====== /etc/SMS e sms.cfg ======
etcdir:
	install -d -m755 $(CFG_DIR)

install-config: etcdir
	@if [ ! -f "$(CFG_FILE)" ]; then \
	  echo ">> Criando $(CFG_FILE)"; \
	  if [ -f "$(CFG_SRC)" ]; then \
	    install -m640 -o root -g root "$(CFG_SRC)" "$(CFG_FILE)"; \
	  else \
	    umask 037; \
	    { \
	      echo "INICIO: 07:00"; \
	      echo "FIM: 20:00"; \
	      echo "SEGUNDA: 1"; \
	      echo "TERCA: 1"; \
	      echo "QUARTA: 1"; \
	      echo "QUINTA: 1"; \
	      echo "SEXTA: 1"; \
	      echo "SABADO: 1"; \
	      echo "DOMINGO: 1"; \
	      echo "LOCALDB: localhost"; \
	      echo "USERDB: root"; \
	      echo "PASSDB: 226468"; \
	      echo "ALIASDB: SMSdb"; \
	    } > "$(CFG_FILE)"; \
	    chown root:root "$(CFG_FILE)"; \
	    chmod 0640 "$(CFG_FILE)"; \
	  fi; \
	else \
	  echo ">> Mantido $(CFG_FILE) existente (não sobrescrito)"; \
	fi

show-config:
	@echo "==> $(CFG_FILE)"; \
	if [ -f "$(CFG_FILE)" ]; then cat "$(CFG_FILE)"; else echo "(arquivo não existe)"; fi

# ====== LOGS /var/log/SMS e logrotate ======
install-logdir:
	install -d -m755 $(LOG_DIR)
	# Garante dono padrão (root:root); ajuste se rodar com outro usuário
	chown root:root $(LOG_DIR) || true

install-logrotate:
	@echo ">> Instalando $(LOGROTATE_FILE)"
	@umask 022; \
	{ \
	  echo "$(LOG_DIR)/*.log {"; \
	  echo "    daily"; \
	  echo "    rotate 30"; \
	  echo "    compress"; \
	  echo "    missingok"; \
	  echo "    notifempty"; \
	  echo "    create 0640 root root"; \
	  echo "}"; \
	} > "$(LOGROTATE_FILE)"

uninstall-log:
	-@rm -f "$(LOGROTATE_FILE)"
	@echo ">> $(LOGROTATE_FILE) removido. Logs em $(LOG_DIR) preservados."

# ====== Serviço SysV ======
service-enable:
	update-rc.d srvSMS defaults || true

service-disable:
	-update-rc.d srvSMS remove || true

# ====== Limpeza / Remoção ======
clean:
	-$(MAKE) service-disable
	-rm -f $(BIN)

uninstall: clean uninstall-log
	-rm -f $(BINDIR)/$(BIN)
	@echo "Config em $(CFG_FILE) e logs em $(LOG_DIR) preservados."

# ====== Banco (scripts em mysql/) ======
mysql: drop create tables

create:
	$(MYSQL_CMD) < mysql/srvSMSdb.sql

drop:
	$(MYSQL_CMD) < mysql/dropdb.sql

tables:
	$(MYSQL_CMD) < mysql/jobs.sql

# ====== Site (Apache) ======
newapache_dir:
	mkdir -p "$(SITE)/$(PASTA)"

apache2: newapache_dir
	cp -R site/. "$(SITE)/$(PASTA)/"

# ====== Execução ======
run:
	$(BINDIR)/$(BIN) || ./$(BIN)
