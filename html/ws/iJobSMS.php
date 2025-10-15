<?php
    ini_set('display_errors', 'On');
    /*error_reporting(E_ALL);*/

    include "connectdb.php";

    // 1) Tenta formulário
    $telefone = isset($_POST['telefone']) ? $_POST['telefone'] : null;
    $mensagem = isset($_POST['mensagem']) ? $_POST['mensagem'] : null;
    $status   = isset($_POST['status'])   ? $_POST['status']   : null;

    // 2) Se não vier por POST, tenta JSON
    if ($telefone === null && $mensagem === null) {
        $raw = file_get_contents("php://input");
        if ($raw) {
            $data = json_decode($raw);
            if (is_object($data)) {
                $telefone = isset($data->telefone) ? $data->telefone : null;
                $mensagem = isset($data->mensagem) ? $data->mensagem : null;
                $status   = isset($data->status)   ? $data->status   : null;
            }
        }
    }

    // Trim e validação
    $telefone = trim((string)$telefone);
    $mensagem = trim((string)$mensagem);

    if ($telefone === '' || $mensagem === '') {
        header('Content-Type: application/json; charset=utf-8', true, 400);
        echo json_encode(array('error' => 'Telefone e mensagem não podem ficar em branco.'));
        exit;
    }

    if (strlen($telefone) < 8 || strlen($telefone) > 20) {
        header('Content-Type: application/json; charset=utf-8', true, 400);
        echo json_encode(array('error' => 'Telefone deve ter entre 8 e 20 caracteres.'));
        exit;
    }

    if (strlen($mensagem) > 500) {
        header('Content-Type: application/json; charset=utf-8', true, 400);
        echo json_encode(array('error' => 'Mensagem deve ter no máximo 500 caracteres.'));
        exit;
    }

    $status = ($status === null || $status === '') ? 0 : (int)$status;

    // Escapa para o SQL
    $telefone = $dbhandle->real_escape_string($telefone);
    $mensagem = $dbhandle->real_escape_string($mensagem);

    $query = "INSERT INTO jobs (telefone, mensagem, status) VALUES ('".$telefone."', '".$mensagem."', ".$status.")";
    $ok = $dbhandle->query($query);

    header('Content-Type: application/json; charset=utf-8');
    if ($ok) {
        echo json_encode(array('ok' => true));
    } else {
        http_response_code(500);
        echo json_encode(array('error' => 'Falha ao inserir.'));
    }
?>
