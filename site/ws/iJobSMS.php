<?php
    /*phpinfo();*/
	/*Registra webservice para processamento de jobs*/
	ini_set('display_errors', 'On');
	/*error_reporting(E_ALL);*/
	
	include "connectdb.php";
	
	$data = json_decode(file_get_contents("php://input"));
	
	$telefone = $dbhandle->real_escape_string($data->telefone);
	$mensagem = $dbhandle->real_escape_string($data->mensagem);
	$query= "INSERT into jobs (telefone, mensagem, status) values ( '". $telefone. "', '".$mensagem."', 0)";
	$dbhandle->query($query);
?>
 