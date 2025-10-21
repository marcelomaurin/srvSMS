<?php
ini_set('display_errors','On'); error_reporting(E_ALL);
header('Content-Type: application/json; charset=utf-8');

include "../connectdb.php";

$raw = file_get_contents("php://input");
$body = json_decode($raw, true);
if (!is_array($body)) { $body = $_POST; }
$id = 0;

if (isset($body['id'])) $id = (int)$body['id'];
if (!$id && isset($_GET['id'])) $id = (int)$_GET['id'];

if ($id <= 0) {
  http_response_code(400);
  echo json_encode(['ok'=>false,'msg'=>'id inválido']);
  exit;
}

$sql = "DELETE FROM telefone_lote WHERE id=$id";
$ok = $dbhandle->query($sql);
echo json_encode(['ok'=>$ok?true:false,'id'=>$id]);
