<?php
	define("HOSTNAME","localhost"); //HOSTNAME mysql or ip
	define("USERNAME","<user>"); //USERNAME
 	define("PASSWORD","<passwrd>"); //Senha PASSWORD
	define("DATABASE","<database>"); //Database name

	$dbhandle = new mysqli(HOSTNAME, USERNAME, PASSWORD, DATABASE) or die("Erro ao conectar no banco de dados");

?>
