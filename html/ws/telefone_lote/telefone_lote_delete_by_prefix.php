<?php
ini_set('display_errors', 'On'); 
error_reporting(E_ALL);
header('Content-Type: application/json; charset=utf-8');

include "../connectdb.php";

// Transformar erros do MySQLi em exceções para tratamento uniforme
mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

try {
  if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    echo json_encode(['ok'=>false,'msg'=>'Método não permitido. Use POST.']);
    exit;
  }

  if (!isset($dbhandle) || !($dbhandle instanceof mysqli)) {
    http_response_code(500);
    echo json_encode(['ok'=>false,'msg'=>'Falha de conexão com o banco.']);
    exit;
  }
  $dbhandle->set_charset('utf8mb4');

  // Lê JSON; se não vier, usa POST tradicional
  $raw  = file_get_contents('php://input');
  $body = json_decode($raw, true);
  if (!is_array($body) || !count($body)) { $body = $_POST; }

  $id_lote = isset($body['id_lote']) ? (int)$body['id_lote'] : 0;
  $prefix  = isset($body['prefix'])  ? trim((string)$body['prefix'])  : '';

  if ($id_lote <= 0 || $prefix === '') {
    http_response_code(400);
    echo json_encode(['ok'=>false,'msg'=>'id_lote e prefix são obrigatórios']); 
    exit;
  }

  // Tratar o prefixo como texto literal (sem permitir curingas do LIKE vindos do usuário)
  // Escapa barra invertida primeiro, depois % e _, e adiciona % ao fim para "começa com"
  $escaped = str_replace(['\\', '%', '_'], ['\\\\', '\%', '\_'], $prefix) . '%';

  // DELETE com prepared statement + ESCAPE para tratar o backslash como escape literal
  $sql = "DELETE FROM telefone_lote 
          WHERE id_lote = ? 
            AND numero_telefone LIKE ? ESCAPE '\\\\'";

  $stmt = $dbhandle->prepare($sql);
  $stmt->bind_param('is', $id_lote, $escaped);
  $stmt->execute();
  $deleted = $stmt->affected_rows;
  $stmt->close();

  echo json_encode([
    'ok'       => true,
    'id_lote'  => $id_lote,
    'prefix'   => $prefix,
    'deleted'  => (int)$deleted,
    'msg'      => 'Exclusão concluída.'
  ]);

} catch (Throwable $e) {
  http_response_code(500);
  echo json_encode([
    'ok'  => false,
    'msg' => 'Erro ao excluir por prefixo: '.$e->getMessage()
  ]);
}
