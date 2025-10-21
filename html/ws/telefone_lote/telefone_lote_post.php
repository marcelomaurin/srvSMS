<?php
ini_set('display_errors','On'); error_reporting(E_ALL);
header('Content-Type: application/json; charset=utf-8');

include "../connectdb.php";

$raw = file_get_contents("php://input");
$body = json_decode($raw, true);
if (!is_array($body)) { $body = $_POST; }

$id              = isset($body['id']) ? (int)$body['id'] : 0;
$id_lote         = isset($body['id_lote']) ? (int)$body['id_lote'] : 0;
$numero_telefone = isset($body['numero_telefone']) ? $dbhandle->real_escape_string(trim($body['numero_telefone'])) : '';
$nro_hygia       = isset($body['nro_hygia']) ? (int)$body['nro_hygia'] : null;

if ($id_lote <= 0 || $numero_telefone === '' || $nro_hygia === null) {
  http_response_code(400);
  echo json_encode(['ok'=>false,'msg'=>'id_lote, numero_telefone e nro_hygia são obrigatórios']);
  exit;
}
if ($nro_hygia < 0 || $nro_hygia > 99999999) {
  http_response_code(400);
  echo json_encode(['ok'=>false,'msg'=>'nro_hygia inválido (0..99999999)']);
  exit;
}

if ($id > 0) {
  // UPDATE
  $sql = "UPDATE telefone_lote
          SET id_lote=$id_lote,
              numero_telefone='$numero_telefone',
              nro_hygia=$nro_hygia
          WHERE id=$id";
  $ok = $dbhandle->query($sql);
  echo json_encode(['ok'=>$ok?true:false,'id'=>$id,'op'=>'update']);
} else {
  // INSERT
  $sql = "INSERT INTO telefone_lote (id_lote, numero_telefone, nro_hygia)
          VALUES ($id_lote,'$numero_telefone',$nro_hygia)";
  $ok = $dbhandle->query($sql);
  echo json_encode(['ok'=>$ok?true:false,'id'=>$dbhandle->insert_id,'op'=>'insert']);
}
