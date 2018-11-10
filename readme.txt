Portuguese
==========
Este software é open source, GPL 3.

Por favor, envie um feedback para marcelomaurinmartins@gmail.com

Requerimentos de Instalação:
1) SMSTools
2) Compilador GCC
3) Mysql 5 ou maior

Ambiente Linux Debian ou equivalente.

Instalação
Siga o procedimento abaixo:
1) Instale SMSTools e Ferramentas auxiliares, ver topico.
2) Instale o Mysql
3) Crie um banco de dados, adicione a tabela conforme arquivo .sql
4) Modifique o arquivo srvSMS.c com os parametros de conexao do banco.
5) Como o minicom, ache o device que esta o SMS, dando o comanto at, para ver se responde, o meu varia /dev/ttyS0.
6) Make compile, para compilar a aplicacao
7) Make install, para instalar a aplicacao.
8) Tudo ok!

Ferramentas Auxiliares
apt-get install build-essential libusb-1.0 libusb-1.0-0-dev
sudo apt-get install smstools

English
=======
This software is open source, GPL 3.

Please send feedback to marcelomaurinmartins@gmail.com

Installation Requirements:
1) SMSTools
2) GCC Compiler
3) Mysql 5 or greater

Debian Linux environment or equivalent.

Installation
Follow the procedure below:
1) Install SMSTools and Auxiliary Tools, see topic.
2) Install Mysql
3) Create a database, add the table as .sql file
4) Modify the srvSMS.c file with the database connection parameters.
5) Like minicom, find the device that is the SMS, giving the comanto at, to see if it responds, my varia / dev / ttyS0.
6) Make compile, to compile the application
7) Make install, to install the application.
8) Everything okay!

Auxiliary Tools
> apt-get install build-essential libusb-1.0 libusb-1.0-0-dev
> sudo apt-get install smstools
