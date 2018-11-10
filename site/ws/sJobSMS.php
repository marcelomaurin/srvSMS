<?php
    /*phpinfo();*/
	/*Registra webservice para processamento de jobs*/
	ini_set('display_errors', 'On');
	error_reporting(E_ALL);
	
	include "connectdb.php";
	
	//header('Cache-Control: no-cache, must-revalidate');
	//$data = var_dump(json_decode(file_get_contents("php://input")));
	
	$telefone = $dbhandle->real_escape_string($_GET['telefone']);	
	
	$query = "select idjob, telefone, mensagem, status from jobs where (status = 0) and (telefone like '%".$telefone."%');";
	//echo $query;
	
	$rs = $dbhandle->query($query);

	$cont = 0;
	
	while($row=$rs->fetch_assoc())
	{
		$cont ++;
		$data[]=$row;			
	}
	
	if ($cont>0)
	{
		print json_encode($data);
	}	
?>