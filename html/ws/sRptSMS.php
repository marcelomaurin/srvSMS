<?php
    /* sRptSMS.php
     * Relatório de SMS enviados por dia/semana/mês/ano.
     * Sucesso => imprime SOMENTE array JSON se houver linhas (padrão sJobSMS.php).
     * GET:
     *   group     = day|week|month|year (default: day)
     *   start     = YYYY-MM-DD          (default: hoje-30)
     *   end       = YYYY-MM-DD          (default: hoje)
     *   status    = int                 (default: 1)
     *   datefield = nome_coluna_data    (default: dthrcad)
     *   debug     = 0|1                 (opcional; 1 habilita headers/logs)
     */

    ini_set('display_errors', 'On');
    error_reporting(E_ALL);

    include "connectdb.php";

    // ======================= DEBUG HELPERS =======================
    $DEBUG = (isset($_GET['debug']) && $_GET['debug'] == '1');

    function dbg_log($msg) {
        global $DEBUG;
        if ($DEBUG) { error_log("[sRptSMS] " . $msg); }
    }
    function dbg_header($name, $value) {
        global $DEBUG;
        if ($DEBUG) { @header("X-SRPT-" . $name . ": " . $value); }
    }
    function dbg_params($params) {
        dbg_log("params=" . json_encode($params));
        foreach ($params as $k => $v) {
            dbg_header("param-" . $k, is_scalar($v) ? $v : json_encode($v));
        }
    }
    function json_error($message, $http_status = 400) {
        @header("Content-Type: application/json; charset=utf-8", true, $http_status);
        echo json_encode(array("error" => $message));
        exit;
    }
    // ============================================================

    // Parâmetros
    $group    = isset($_GET['group']) ? strtolower($dbhandle->real_escape_string($_GET['group'])) : 'day';
    if (!in_array($group, array('day','week','month','year'))) { $group = 'day'; }

    $start    = isset($_GET['start']) ? $dbhandle->real_escape_string($_GET['start']) : '';
    $end      = isset($_GET['end'])   ? $dbhandle->real_escape_string($_GET['end'])   : '';
    $status   = isset($_GET['status']) ? (int)$_GET['status'] : 1; // ajuste se necessário
    $datefld  = isset($_GET['datefield']) ? $dbhandle->real_escape_string($_GET['datefield']) : 'dthrcad';

    dbg_params(array(
        "group"=>$group,
        "start"=>$start,
        "end"=>$end,
        "status"=>$status,
        "datefield"=>$datefld
    ));

    // Datas padrão (últimos 30 dias)
    $today    = new DateTime('today');
    $defEnd   = $today->format('Y-m-d');
    $defStart = $today->modify('-30 days')->format('Y-m-d');

    if ($start === '') { $start = $defStart; }
    if ($end   === '') { $end   = $defEnd; }

    $startFull = $start . " 00:00:00";
    $endFull   = $end   . " 23:59:59";

    dbg_log("Período resolvido: {$startFull} a {$endFull}");
    dbg_header("period-start", $startFull);
    dbg_header("period-end", $endFull);

    // Valida se a coluna de data existe
    function getCurrentDbName($db) {
        $res = $db->query("SELECT DATABASE() AS db");
        if ($res && ($row = $res->fetch_assoc())) return $row['db'];
        return null;
    }
    function columnExists($db, $schema, $table, $column) {
        $sql = "SELECT 1 FROM information_schema.COLUMNS
                WHERE TABLE_SCHEMA = '".$db->real_escape_string($schema)."'
                  AND TABLE_NAME   = '".$db->real_escape_string($table)."'
                  AND COLUMN_NAME  = '".$db->real_escape_string($column)."'";
        if ($rs = $db->query($sql)) return (bool)$rs->num_rows;
        return false;
    }

    $schema = getCurrentDbName($dbhandle);
    if (!$schema || !columnExists($dbhandle, $schema, "jobs", $datefld)) {
        dbg_log("Coluna de data inexistente: ".$datefld);
        dbg_header("mysql-error", "unknown-date-column");
        json_error("Coluna '".$datefld."' não existe na tabela jobs.", 400);
    }

    // Expressões de label (vamos AGRUPAR por 'label' para evitar ONLY_FULL_GROUP_BY)
    switch ($group) {
        case 'day':
            $labelExpr = "DATE_FORMAT(".$datefld.", '%Y-%m-%d')";
            break;
        case 'week': // ISO (segunda a domingo)
            $labelExpr = "CONCAT(YEAR(".$datefld."), '-W', LPAD(WEEK(".$datefld.",3),2,'0'))";
            break;
        case 'month':
            $labelExpr = "DATE_FORMAT(".$datefld.", '%Y-%m')";
            break;
        case 'year':
            $labelExpr = "YEAR(".$datefld.")";
            break;
        default:
            $labelExpr = "DATE_FORMAT(".$datefld.", '%Y-%m-%d')";
    }

    // Para ordenar cronologicamente sem violar ONLY_FULL_GROUP_BY, usamos MIN($datefld)
    $orderExpr = "MIN(".$datefld.")";

    // ============ SQL compatível com ONLY_FULL_GROUP_BY ============
    // - Seleciona label, COUNT(*) e MIN(date) (agregada)
    // - GROUP BY label (a mesma expressão do SELECT)
    // - ORDER BY MIN(date) para ordem cronológica
    $query  = "select ".$labelExpr." as label, count(*) as total, ".$orderExpr." as first_dt ";
    $query .= "from jobs ";
    $query .= "where (status = ".$status.") ";
    $query .= "and (".$datefld." between '".$startFull."' and '".$endFull."') ";
    $query .= "group by label ";
    $query .= "order by first_dt;";
    // echo $query;

    dbg_log("SQL=" . $query);
    dbg_header("sql", $query);

    $rs = $dbhandle->query($query);

    if ($rs === false) {
        dbg_log("MySQL error: " . $dbhandle->error);
        dbg_header("mysql-error", $dbhandle->error);
        json_error("Erro na consulta SQL.", 500);
    }

    $cont = 0;
    $data = array();

    while ($row = $rs->fetch_assoc())
    {
        $cont ++;
        // Ignora 'first_dt' na saída para manter o contrato (só label/total)
        $data[] = array(
            "label" => $row["label"],
            "total" => (int)$row["total"]
        );
    }

    dbg_log("Linhas retornadas: " . $cont);
    dbg_header("rows", (string)$cont);

    if ($cont>0)
    {
        @header("Content-Type: application/json; charset=utf-8");
        print json_encode($data);
    }
?>
