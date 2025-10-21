<?php
ini_set('display_errors','On'); error_reporting(E_ALL);
header('Content-Type: application/json; charset=utf-8');

include "../connectdb.php";

// Use o MESMO TMP_DIR do upload_temp.php
define('TMP_DIR', rtrim(sys_get_temp_dir(),'/') . '/lote_uploads');

function json_fail($msg, $http=400){
  http_response_code($http);
  echo json_encode(['success'=>false,'message'=>$msg]);
  exit;
}

try {
  if (!isset($dbhandle) || !($dbhandle instanceof mysqli)) {
    json_fail('Falha de conexão com o banco.', 500);
  }
  $dbhandle->set_charset('utf8mb4');

  $id_lote    = isset($_POST['id_lote']) ? (int)$_POST['id_lote'] : 0;
  $descricao  = isset($_POST['descricao'])  ? trim((string)$_POST['descricao'])  : '';
  $data_lote  = isset($_POST['data_lote'])  ? trim((string)$_POST['data_lote'])  : '';
  $observacao = isset($_POST['observacao']) ? trim((string)$_POST['observacao']) : '';
  $ignoraCab  = isset($_POST['ignora_cabecalho']);
  $temp_id_in = isset($_POST['temp_id']) ? $_POST['temp_id'] : '';

  // sanitize temp_id (apenas [a-z0-9_-])
  $temp_id = preg_replace('/[^a-zA-Z0-9_-]/','',$temp_id_in);
  if ($temp_id === '') json_fail('temp_id ausente ou inválido.');

  if (!is_dir(TMP_DIR)) json_fail('Diretório temporário inexistente: '.TMP_DIR, 500);

  $csvPath  = TMP_DIR . "/{$temp_id}.csv";
  $partPath = TMP_DIR . "/{$temp_id}.part";

  // Se .csv não existe mas .part existe, tenta promover agora
  if (!file_exists($csvPath) && file_exists($partPath)) {
    if (!rename($partPath, $csvPath)) {
      json_fail('Arquivo temporário não finalizado (aguarde término do upload).');
    }
  }

  if (!is_readable($csvPath)) {
    json_fail('Arquivo temporário não encontrado');
  }

  // Lê CSV e coleta linhas válidas
  $delim = ';';
  $rows  = [];
  if (($csv = fopen($csvPath, 'r')) === false) {
    json_fail('Erro ao abrir CSV temporário.', 500);
  }
  $linha = 0;
  while (($dados = fgetcsv($csv, 2000, $delim)) !== false) {
    $linha++;
    if ($linha === 1 && $ignoraCab) continue;
    if (count($dados) < 5) continue;

    $numpac = trim($dados[0]);      // NUMPAC
    $ddd    = trim($dados[3]);      // FONE_DDD
    $fone   = trim($dados[4]);      // FONE

    if ($numpac === '' || $fone === '') continue;

    $nro_hygia = (int)preg_replace('/\D+/', '', $numpac);
    $numero    = preg_replace('/\D+/', '', $ddd.$fone);
    if ($nro_hygia <= 0 || $numero === '') continue;
    if (strlen($numero) > 20) $numero = substr($numero, 0, 20);

    $rows[] = [$nro_hygia, $numero];
  }
  fclose($csv);

  if (!count($rows)) {
    // limpa arquivo e encerra
    @unlink($csvPath);
    json_fail('CSV sem linhas válidas (lote não alterado).');
  }

  $dbhandle->begin_transaction();
  try {
    $created = false;

    if ($id_lote <= 0) {
      // criar novo lote (modo opcional)
      if ($descricao === '' || $data_lote === '') {
        throw new Exception("Descricao e data_lote são obrigatórias para criar novo lote.");
      }
      $stmtL = $dbhandle->prepare("INSERT INTO lote (descricao, observacao, data_lote) VALUES (?,?,?)");
      $stmtL->bind_param('sss', $descricao, $observacao, $data_lote);
      $stmtL->execute();
      $id_lote = (int)$stmtL->insert_id;
      $stmtL->close();
      $created = true;
    } else {
      // validar existência
      $res = $dbhandle->query("SELECT id FROM lote WHERE id = ".(int)$id_lote." LIMIT 1");
      if (!$res || !$res->num_rows) throw new Exception("Lote id=$id_lote não encontrado.");
      $res->free();
    }

    $stmt = $dbhandle->prepare("INSERT INTO telefone_lote (id_lote, numero_telefone, nro_hygia) VALUES (?,?,?)");
    if (!$stmt) throw new Exception("Prepare falhou: ".$dbhandle->error);

    $ok = 0;
    foreach ($rows as [$h,$num]) {
      $stmt->bind_param('isi', $id_lote, $num, $h);
      if ($stmt->execute()) $ok++;
    }
    $stmt->close();

    if ($ok === 0) {
      throw new Exception("Nenhuma linha inserida em telefone_lote.");
    }

    $dbhandle->commit();
    @unlink($csvPath); // apaga temporário final

    echo json_encode([
      'success'      => true,
      'id_lote'      => $id_lote,
      'created_lote' => $created,
      'importados'   => $ok,
      'ignorados'    => count($rows) - $ok
    ]);

  } catch (Exception $e) {
    $dbhandle->rollback();
    json_fail($e->getMessage());
  }

} catch (Throwable $e) {
  json_fail($e->getMessage(), 500);
}
