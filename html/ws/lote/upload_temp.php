<?php
ini_set('display_errors','On'); error_reporting(E_ALL);
header('Content-Type: application/json; charset=utf-8');

// Diretório temporário seguro
define('TMP_DIR', rtrim(sys_get_temp_dir(),'/') . '/lote_uploads');

function ensure_tmp_dir() {
  if (!is_dir(TMP_DIR)) {
    if (!mkdir(TMP_DIR, 0775, true)) {
      throw new RuntimeException('Falha ao criar diretório temporário: ' . TMP_DIR);
    }
  }
  if (!is_writable(TMP_DIR)) {
    throw new RuntimeException('Diretório temporário sem permissão: ' . TMP_DIR);
  }
}

function json_fail($msg, $http=400){
  http_response_code($http);
  echo json_encode(['success'=>false,'message'=>$msg]);
  exit;
}

try {
  ensure_tmp_dir();

  // Campos obrigatórios para chunked upload
  $temp_id      = isset($_POST['temp_id']) ? preg_replace('/[^a-zA-Z0-9_-]/','', $_POST['temp_id']) : '';
  $chunk_index  = isset($_POST['chunk_index']) ? (int)$_POST['chunk_index'] : null;
  $total_chunks = isset($_POST['total_chunks']) ? (int)$_POST['total_chunks'] : null;

  if ($temp_id === '' || $chunk_index === null || $total_chunks === null) {
    json_fail('Parâmetros obrigatórios: temp_id, chunk_index, total_chunks.');
  }
  if ($total_chunks <= 0 || $chunk_index < 0 || $chunk_index >= $total_chunks) {
    json_fail('Índices de chunk inválidos.');
  }

  if (empty($_FILES) || !isset($_FILES['arquivo'])) {
    json_fail('Campo de arquivo "arquivo" não encontrado.');
  }
  $f = $_FILES['arquivo'];
  if ($f['error'] !== UPLOAD_ERR_OK) {
    json_fail('Erro de upload do chunk (código '.$f['error'].').');
  }
  if (!is_uploaded_file($f['tmp_name'])) {
    json_fail('Chunk inválido (não é uploaded_file).');
  }

  // Arquivos alvo
  $partPath = TMP_DIR . "/{$temp_id}.part";
  $finalPath = TMP_DIR . "/{$temp_id}.csv";

  // Escreve/concatena o chunk na ordem
  // Estrategia: se chunk_index == 0 => (re)cria arquivo; senão, abre em append.
  $mode = ($chunk_index === 0) ? 'wb' : 'ab';
  $fp = fopen($partPath, $mode);
  if (!$fp) json_fail('Falha ao abrir arquivo parcial.');

  // Lock exclusivo durante a escrita
  if (!flock($fp, LOCK_EX)) {
    fclose($fp);
    json_fail('Falha ao bloquear arquivo parcial.');
  }

  $chunkData = fopen($f['tmp_name'], 'rb');
  if (!$chunkData) {
    flock($fp, LOCK_UN); fclose($fp);
    json_fail('Falha ao abrir chunk temporário.');
  }

  stream_copy_to_stream($chunkData, $fp);
  fclose($chunkData);

  // Libera lock e fecha
  fflush($fp);
  flock($fp, LOCK_UN);
  fclose($fp);

  // Se este é o último chunk, renomeia para .csv (operação atômica)
  $finished = false;
  if ($chunk_index === ($total_chunks - 1)) {
    // Segurança: garante que arquivo parcial exista
    if (!file_exists($partPath)) {
      json_fail('Arquivo parcial ausente antes do rename final.');
    }
    if (!rename($partPath, $finalPath)) {
      json_fail('Falha ao finalizar arquivo.');
    }
    $finished = true;
  }

  echo json_encode([
    'success'      => true,
    'temp_id'      => $temp_id,
    'chunk_index'  => $chunk_index,
    'total_chunks' => $total_chunks,
    'finished'     => $finished,
    'path'         => $finished ? basename($finalPath) : basename($partPath)
  ]);

} catch (Throwable $e) {
  json_fail($e->getMessage(), 400);
}
