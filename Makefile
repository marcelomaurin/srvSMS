#HEADERS = program.h headers.h

CC= gcc
MYSQLCFLAGS= -I/usr/include/mysql -DBIG_JOINS=1 -fno-strict-aliasing -g
MYSQLLIBS= -L/usr/lib/mysql -lmysqlclient  -lpthread -lz -lm -lrt -ldl

# Inclua aqui o usuario e senha do banco de dados #
HOST="maurinsoft.com.br"
USER="mmm"
PASSWORD="226468"

#Inclua aqui o caminho da pasta apache2
SITE="/var/www/html/"
#Pasta que será criada no site apache
PASTA="SMS" 

default: first 

first:  compile install registra create tables newapache_dir apache2 run

all: clean compile install registra mysql apache2 run

compile:
	$(CC) srvSMS.c -g -o srvSMS  $(MYSQLCFLAGS) $(MYSQLLIBS)

	

install:
	#copia para pasta de binarios
	cp srvSMS /usr/local/bin/
	cp srvSMS.sh /etc/init.d/
	#copia script de servico
	cp srvSMS_service /etc/init.d/srvSMS
	#atribui direitos de servico
	chmod +x /etc/init.d/srvSMS

registra:
	#registra o servico
	update-rc.d srvSMS defaults	

clean:
	#Retira registro do servico
	update-rc.d srvSMS remove
	rm -f srvSMS
	rm -f /usr/local/bin/srvSMS

mysql:  drop create tables
	
create:
	mysql -u $(USER) -p$(PASSWORD) -h$(HOST) < mysql/srvSMSdb.sql
	
drop:
	mysql -u $(USER) -p$(PASSWORD) -h$(HOST) < mysql/dropdb.sql
tables:
	mysql -u $(USER) -p$(PASSWORD) -h$(HOST) < mysql/jobs.sql
	
	
procedures:

samples:

newapache_dir:
	mkdir site/*.* $SITE$PASTA 

apache2:
	cp -R site/*.* $SITE$PASTA 

run:
	srvSMS
