<?php
ini_set('display_errors','On'); error_reporting(E_ALL);
header('Content-Type: text/plain; charset=utf-8');

include "../connectdb.php";

/**
 * Importa um CSV de telefones e cadastra:
 *   1. Um novo registro na tabela `lote`
 *   2. Telefones vinculados em `telefone_lote`
 *
 * CSV esperado (delimitador ';'):
 *   0 NUMPAC        -> nro_hygia (int)
 *   1 Nome          -> ignorar
 *   2 US_CODUNIDSAU -> ignorar
 *   3 FONE_DDD      -> parte 1 do telefone
 *   4 FONE          -> parte 2 do telefone
 */

$descricao  = isset($_POST['descricao'])  ? trim($_POST['descricao'])  : '';
$data_lote  = isset($_POST['data_lote'])  ? trim($_POST['data_lote'])  : '';
$observacao = isset($_POST['observacao']) ? trim($_POST['observacao']) : '';
$ignoraCabecalho = isset($_POST['ignora_cabecalho']);

if (!isset($_FILES['arquivo']) || $_FILES['arquivo']['error'] !== UPLOAD_ERR_OK) {
  http_response_code(400);
  exit("Erro no upload do arquivo CSV.\n");
}

if ($descricao === '' || $data_lote === '') {
  http_response_code(400);
  exit("Descrição e data do lote são obrigatórias.\n");
}

$csvPath = $_FILES['arquivo']['tmp_name'];
if (!is_readable($csvPath)) {
  http_response_code(500);
  exit("Não foi possível ler o arquivo CSV.\n");
}

$csv = fopen($csvPath, 'r');
if (!$csv) {
  http_response_code(500);
  exit("Erro ao abrir arquivo CSV.\n");
}

$linha = 0;
$rows = [];
$delim = ';';

// Lê todas as linhas válidas do CSV
while (($dados = fgetcsv($csv, 2000, $delim)) !== false) {
  $linha++;
  if ($linha === 1 && $ignoraCabecalho) continue;
  if (count($dados) < 5) continue;

  $numpac   = trim($dados[0]);
  $foneDDD  = trim($dados[3]);
  $fone     = trim($dados[4]);

  if ($numpac === '' || $fone === '') continue;

  $nro_hygia = (int)preg_replace('/\D+/', '', $numpac);
  $numero_telefone = preg_replace('/\D+/', '', $foneDDD . $fone);

  if ($nro_hygia <= 0 || $numero_telefone === '') continue;
  if (strlen($numero_telefone) > 20) $numero_telefone = substr($numero_telefone, 0, 20);

  $rows[] = [$nro_hygia, $numero_telefone];
}
fclose($csv);

// Nenhuma linha válida → não cria lote
if (count($rows) === 0) {
  http_response_code(400);
  exit("Nenhuma linha válida encontrada. Lote não criado.\n");
}

$dbhandle->begin_transaction();

try {
  // Cria o lote primeiro
  $desc_esc = $dbhandle->real_escape_string($descricao);
  $obs_esc  = $dbhandle->real_escape_string($observacao);
  $data_esc = $dbhandle->real_escape_string($data_lote);

  $sqlLote = "INSERT INTO lote (descricao, observacao, data_lote)
              VALUES ('$desc_esc', '$obs_esc', '$data_esc')";
  if (!$dbhandle->query($sqlLote)) {
    throw new Exception("Falha ao criar lote: ".$dbhandle->error);
  }

  $id_lote = (int)$dbhandle->insert_id;
  echo "✅ Lote criado com sucesso. ID: $id_lote\n";

  // Agora insere os telefones vinculados
  $stmt = $dbhandle->prepare("
    INSERT INTO telefone_lote (id_lote, numero_telefone, nro_hygia)
    VALUES (?, ?, ?)
  ");
  if (!$stmt) {
    throw new Exception("Falha ao preparar statement: ".$dbhandle->error);
  }

  $importados = 0;
  foreach ($rows as [$nro_hygia, $numero_telefone]) {
    $stmt->bind_param('isi', $id_lote, $numero_telefone, $nro_hygia);
    if ($stmt->execute()) $importados++;
  }
  $stmt->close();

  if ($importados === 0) {
    throw new Exception("Nenhum telefone pôde ser inserido. Lote revertido.");
  }

  $dbhandle->commit();
  echo "📦 Telefones importados: $importados\n";
  echo "Linhas inválidas/ignoradas: ".(count($rows) - $importados)."\n";
  echo "Importação concluída com sucesso.\n";

} catch (Exception $e) {
  $dbhandle->rollback();
  http_response_code(400);
  echo "❌ Erro: ".$e->getMessage()."\n";
}
