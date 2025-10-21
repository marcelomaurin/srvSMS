<?php
ini_set('display_errors', 'On');
error_reporting(E_ALL);
header('Content-Type: application/json; charset=utf-8');

include "../connectdb.php";

try {
  mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
  if (!isset($dbhandle) || !($dbhandle instanceof mysqli)) {
    http_response_code(500);
    echo json_encode(['ok'=>false,'msg'=>'Conexão de banco indisponível']); exit;
  }
  $dbhandle->set_charset('utf8mb4');

  // --- helpers ---
  $getBody = function() {
    $raw = file_get_contents("php://input");
    $j = json_decode($raw, true);
    return is_array($j) ? $j : $_POST;
  };
  $normalizePhone = function($p) {
    $d = preg_replace('/\D+/', '', (string)$p);
    if ($d === '') return '';
    // limita ao varchar(20)
    if (strlen($d) > 20) $d = substr($d, 0, 20);
    return $d;
  };
  $splitPhones = function($val) use ($normalizePhone) {
    // aceita array ou string; separa por \n , ; ou espaço
    if (is_array($val)) {
      $arr = $val;
    } else {
      $arr = preg_split('/[\n,;\s]+/u', (string)$val, -1, PREG_SPLIT_NO_EMPTY);
    }
    $out = [];
    $seen = [];
    foreach ($arr as $p) {
      $n = $normalizePhone($p);
      if ($n !== '' && !isset($seen[$n])) {
        $seen[$n] = true;
        $out[] = $n;
      }
    }
    return $out;
  };
  $splitMessages = function($val) {
    // aceita array ou string; converte "\n" literal para quebra real e divide por linhas
    if (is_array($val)) {
      $arr = $val;
    } else {
      $txt = str_replace("\\n", "\n", (string)$val);
      $arr = preg_split('/\n+/u', $txt, -1, PREG_SPLIT_NO_EMPTY);
    }
    $out = [];
    foreach ($arr as $m) {
      $s = trim((string)$m);
      if ($s !== '') {
        if (mb_strlen($s) > 500) {
          // respeita varchar(500)
          $s = mb_substr($s, 0, 500);
        }
        $out[] = $s;
      }
    }
    return $out;
  };

  // --- input ---
  $body     = $getBody();
  $id_lote  = isset($body['id_lote'])    ? (int)$body['id_lote']    : 0;
  $id_ini   = isset($body['id_inicial']) ? (int)$body['id_inicial'] : 0;
  $id_fim   = isset($body['id_final'])   ? (int)$body['id_final']   : 0;

  // mensagens: aceita 'mensagens' (array) OU 'mensagem' (string)
  if (isset($body['mensagens'])) {
    $mensagens = $splitMessages($body['mensagens']);
  } else {
    $mensagens = $splitMessages(isset($body['mensagem']) ? $body['mensagem'] : '');
  }

  // extras início/fim: aceita array ou string
  $extrasIni = $splitPhones(isset($body['extras_inicio']) ? $body['extras_inicio'] : []);
  $extrasFim = $splitPhones(isset($body['extras_fim']) ? $body['extras_fim'] : []);

  // validações básicas
  if ($id_lote <= 0 || $id_ini <= 0 || $id_fim < $id_ini) {
    http_response_code(400);
    echo json_encode(['ok'=>false,'msg'=>'Parâmetros inválidos (id_lote/id_inicial/id_final)']); exit;
  }
  if (!count($mensagens)) {
    http_response_code(400);
    echo json_encode(['ok'=>false,'msg'=>'Informe ao menos uma mensagem']); exit;
  }

  // busca faixa dos telefones do lote
  $stmt = $dbhandle->prepare("
    SELECT id, numero_telefone
      FROM telefone_lote
     WHERE id_lote = ?
       AND id BETWEEN ? AND ?
     ORDER BY id ASC
  ");
  $stmt->bind_param('iii', $id_lote, $id_ini, $id_fim);
  $stmt->execute();
  $res = $stmt->get_result();

  $faixa = [];
  while ($row = $res->fetch_assoc()) {
    $n = preg_replace('/\D+/', '', (string)$row['numero_telefone']);
    if ($n !== '') {
      if (strlen($n) > 20) $n = substr($n, 0, 20);
      $faixa[] = $n;
    }
  }
  $stmt->close();

  // compõe lista final: extras_inicio + faixa + extras_fim (sem duplicar, mantendo ordem)
  $telefones = [];
  $seen = [];
  foreach ([$extrasIni, $faixa, $extrasFim] as $chunk) {
    foreach ($chunk as $p) {
      if ($p !== '' && !isset($seen[$p])) {
        $seen[$p] = true;
        $telefones[] = $p;
      }
    }
  }

  if (!count($telefones)) {
    http_response_code(400);
    echo json_encode(['ok'=>false,'msg'=>'Nenhum telefone válido encontrado (faixa e/ou extras).']); exit;
  }

  // transação + insert preparado
  $dbhandle->begin_transaction();

  $stmt = $dbhandle->prepare("INSERT INTO jobs (telefone, mensagem, status) VALUES (?, ?, 0)");

  $inserted = 0;
  foreach ($telefones as $tel) {
    foreach ($mensagens as $msg) {
      $stmt->bind_param('ss', $tel, $msg);
      $stmt->execute();
      $inserted++;
    }
  }
  $stmt->close();

  $dbhandle->commit();

  echo json_encode([
    'ok' => true,
    'msg' => 'Jobs criados com sucesso.',
    'inserted' => $inserted,
    'stats' => [
      'id_lote' => $id_lote,
      'ids' => ['inicial' => $id_ini, 'final' => $id_fim],
      'telefones_faixa' => count($faixa),
      'extras_inicio' => count($extrasIni),
      'extras_fim' => count($extrasFim),
      'mensagens' => count($mensagens),
      'telefones_totais_sem_dup' => count($telefones)
    ]
  ]);

} catch (Throwable $e) {
  if (isset($dbhandle) && $dbhandle instanceof mysqli) {
    try { $dbhandle->rollback(); } catch(Throwable $ignored) {}
  }
  http_response_code(500);
  echo json_encode(['ok'=>false, 'msg'=>'Erro: '.$e->getMessage()]);
}
