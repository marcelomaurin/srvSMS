<?php
ini_set('display_errors','On'); error_reporting(E_ALL);
header('Content-Type: application/json; charset=utf-8');

include "../connectdb.php"; // ajuste o caminho se necessário

$q      = isset($_GET['q'])      ? $dbhandle->real_escape_string($_GET['q']) : '';
$id     = isset($_GET['id'])     ? (int)$_GET['id'] : 0;
$start  = isset($_GET['start'])  ? $dbhandle->real_escape_string($_GET['start']) : '';
$end    = isset($_GET['end'])    ? $dbhandle->real_escape_string($_GET['end'])   : '';

$sql = "SELECT id, descricao, observacao, data_lote, dtcad
        FROM lote WHERE 1=1";

if ($id > 0) {
  $sql .= " AND id = ".$id;
}
if ($q !== '') {
  $like = "%$q%";
  $sql .= " AND (descricao LIKE '".$like."' OR observacao LIKE '".$like."')";
}
if ($start !== '') {
  $sql .= " AND data_lote >= '".$start."'";
}
if ($end !== '') {
  $sql .= " AND data_lote <= '".$end."'";
}
$sql .= " ORDER BY data_lote DESC, id DESC";

$rs = $dbhandle->query($sql);
$out = [];
if ($rs) {
  while ($row = $rs->fetch_assoc()) { $out[] = $row; }
}
echo json_encode($out);
