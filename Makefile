#HEADERS = program.h headers.h

CC= gcc
MYSQLCFLAGS= -I/usr/include/mysql -DBIG_JOINS=1 -fno-strict-aliasing -g
MYSQLLIBS= -L/usr/lib/mysql -lmysqlclient  -lpthread -lz -lm -lrt -ldl


default: compile

all: clean compile install run 

compile:	
	$(CC) main.cpp -g -o srvSMS $(MYSQLCFLAGS) $(MYSQLLIBS)

clean:
	rm -f srvSMS
	rm -f /usr/local/bin/srvSMS
	
run:
	/usr/local/bin/srvSMS

install:
	cp ./srvSMS /usr/local/bin/

