<?php
    /*phpinfo();*/
	/*Registra webservice para processamento de jobs*/
	ini_set('display_errors', 'On');
	/*error_reporting(E_ALL);*/
	//echo "teste1";
	
	include "connectdb.php";
	
	$data = json_decode(file_get_contents("php://input"));
	$idjob = $dbhandle->real_escape_string($data->idjob);
	$telefone = $dbhandle->real_escape_string($data->telefone);
	$mensagem = $dbhandle->real_escape_string($data->mensagem);
   
	
	
	$query= "update jobs set telefone = '".$telefone."', mensagem= '".$mensagem."' where idJob=". $idjob;
	//echo $query ;
	$dbhandle->query($query);
	echo "OK";

?>
 