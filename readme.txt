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

Instalação Interface Web
========================
A interface web permite enviar SMS pela interface de internet.
Não temos a intenção de desenvolver um aplicativo, porem é um modelo para que outros
aprimorem e desenvolvam esta aplicação.

Requisitos da Interface Web
===========================
1) Apache 2 com Mysql
2) PHP 5 ou 7
3) Phpmyadmin - recomendado mas não necessário

Procedimentos de instalação Interface Web
=========================================
Copie os arquivos da pasta html do projeto para a pasta /var/www/html.
Dê permissão:
> chmod 555 -R /var/www/html/ws 
> chmod 555 -R /var/www/html/SMS

Edite o arquivo connectdb.php:
> vim /var/www/html/ws/connectdb.php

Inclua os parametros do banco de dados, como senha, usuario, database name.

Pronto!
Entre no browser, abra e selecione o IP do servidor apache, no meu caso:
localhost/SMS/Envia_SMS.php



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


Web Interface Installation
========================
The web interface allows you to send SMS through the internet interface.
We do not intend to develop an application, but it is a template for others to
improve and develop this application.

Web Interface Requirements
===========================
1) Apache 2 with Mysql
2) PHP 5 or 7
3) Phpmyadmin - recommended but not required

Installation Procedures Web Interface
===========================================
Copy the files from the html folder of the project to the / var / www / html folder.
Give permission:
> chmod 555 -R / var / www / html / ws
> chmod 555 -R / var / www / html / SMS

Edit the connectdb.php file:
> vim /var/www/html/ws/connectdb.php

Include the database parameters, such as password, user, database name.

Ready!
Enter the browser, open and select the IP of the apache server, in my case:
localhost / SMS / Send_SMS.php