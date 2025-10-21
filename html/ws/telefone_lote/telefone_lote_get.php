<?php
ini_set('display_errors','On'); error_reporting(E_ALL);
header('Content-Type: application/json; charset=utf-8');

include "../connectdb.php";

$id       = isset($_GET['id'])       ? (int)$_GET['id'] : 0;
$id_lote  = isset($_GET['id_lote'])  ? (int)$_GET['id_lote'] : 0;
$q        = isset($_GET['q'])        ? $dbhandle->real_escape_string($_GET['q']) : '';

$sql = "SELECT id, id_lote, numero_telefone, nro_hygia, dtcad
        FROM telefone_lote WHERE 1=1";

if ($id > 0)      { $sql .= " AND id = ".$id; }
if ($id_lote > 0) { $sql .= " AND id_lote = ".$id_lote; }
if ($q !== '') {
  $like = "%$q%";
  $sql .= " AND (numero_telefone LIKE '".$like."' OR CAST(nro_hygia AS CHAR) LIKE '".$like."')";
}
$sql .= " ORDER BY id DESC";

$rs = $dbhandle->query($sql);
$out = [];
if ($rs) {
  while ($row = $rs->fetch_assoc()) { $out[] = $row; }
}
echo json_encode($out);
