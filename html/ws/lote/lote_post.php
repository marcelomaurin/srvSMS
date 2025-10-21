<?php
ini_set('display_errors','On'); error_reporting(E_ALL);
header('Content-Type: application/json; charset=utf-8');

include "../connectdb.php";

// Transformar erros do MySQLi em exceções
mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

/**
 * Normaliza datas:
 * - "YYYY-MM-DD" -> mantém
 * - "YYYY-MM-DDTHH:MM:SS(.sss)Z" -> recorta para "YYYY-MM-DD"
 * - "dd/mm/YYYY" -> converte para "YYYY-MM-DD"
 */
function normalize_date($s){
  $s = trim((string)$s);
  if ($s === '') return '';
  if (preg_match('/^\d{4}-\d{2}-\d{2}/', $s)) {
    return substr($s, 0, 10);
  }
  if (preg_match('/^(\d{2})\/(\d{2})\/(\d{4})$/', $s, $m)) {
    return $m[3].'-'.$m[2].'-'.$m[1];
  }
  return $s;
}

try {
  if (!isset($dbhandle) || !($dbhandle instanceof mysqli)) {
    http_response_code(500);
    echo json_encode(['ok'=>false,'msg'=>'Falha de conexão com o banco.']); exit;
  }
  $dbhandle->set_charset('utf8mb4');

  // Lê JSON; se não vier, usa POST tradicional
  $raw  = file_get_contents("php://input");
  $body = json_decode($raw, true);
  if (!is_array($body) || count($body) === 0) { $body = $_POST; }

  $id          = isset($body['id']) ? (int)$body['id'] : 0;
  $descricao   = isset($body['descricao'])  ? trim((string)$body['descricao'])   : '';
  $observacao  = isset($body['observacao']) ? trim((string)$body['observacao'])  : '';
  $data_lote   = isset($body['data_lote'])  ? normalize_date($body['data_lote']) : '';

  // Validações
  if ($descricao === '' || $data_lote === '') {
    http_response_code(400);
    echo json_encode(['ok'=>false,'msg'=>'descricao e data_lote são obrigatórios']); exit;
  }
  if (mb_strlen($descricao) > 100) {
    http_response_code(400);
    echo json_encode(['ok'=>false,'msg'=>'descricao deve ter no máximo 100 caracteres']); exit;
  }
  if (!preg_match('/^\d{4}-\d{2}-\d{2}$/', $data_lote)) {
    http_response_code(400);
    echo json_encode(['ok'=>false,'msg'=>'data_lote inválida (use YYYY-MM-DD)']); exit;
  }
  list($y,$m,$d) = array_map('intval', explode('-', $data_lote));
  if (!checkdate($m,$d,$y)) {
    http_response_code(400);
    echo json_encode(['ok'=>false,'msg'=>'data_lote inválida (data inexistente)']); exit;
  }

  if ($id > 0) {
    // UPDATE
    $stmt = $dbhandle->prepare("
      UPDATE lote
         SET descricao = ?,
             observacao = ?,
             data_lote  = ?
       WHERE id = ?
    ");
    $stmt->bind_param('sssi', $descricao, $observacao, $data_lote, $id);
    $stmt->execute();
    $rows = $stmt->affected_rows; // pode ser 0 se não houve mudança
    $stmt->close();

    echo json_encode([
      'ok'   => true,
      'id'   => $id,
      'op'   => 'update',
      'rows' => $rows,
      'msg'  => ($rows > 0 ? 'Lote atualizado.' : 'Nenhuma alteração aplicada.')
    ]);
  } else {
    // INSERT
    $stmt = $dbhandle->prepare("
      INSERT INTO lote (descricao, observacao, data_lote)
      VALUES (?, ?, ?)
    ");
    $stmt->bind_param('sss', $descricao, $observacao, $data_lote);
    $stmt->execute();
    $newId = (int)$stmt->insert_id;
    $stmt->close();

    echo json_encode([
      'ok'  => true,
      'id'  => $newId,
      'op'  => 'insert',
      'msg' => 'Lote criado com sucesso.'
    ]);
  }

} catch (Throwable $e) {
  http_response_code(500);
  echo json_encode([
    'ok'  => false,
    'msg' => 'Erro ao processar lote: '.$e->getMessage()
  ]);
}
