<?php
    /*phpinfo();*/
	/*Registra webservice para processamento de jobs*/
	ini_set('display_errors', 'On');
	/*error_reporting(E_ALL);*/
	
	include "connectdb.php";
	
	$data = json_decode(file_get_contents("php://input"));
	//echo String($data);
	
	$id = $data->id;
	//echo $data;
	
	$query= "delete from jobs where idjob = ". $id;
	echo $query;
	$dbhandle->query($query);
	
?>
 