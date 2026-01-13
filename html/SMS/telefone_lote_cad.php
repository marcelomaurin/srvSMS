<!doctype html>
<html lang="pt-br">
<head>
  <meta charset="utf-8">
  <title>Importar Telefones (CSV) para Lote</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css">
  <style>
    .help-text{font-size:12px;color:#777}
    .card{border:1px solid #ddd;border-radius:6px;padding:15px;margin-bottom:20px}
    .muted{color:#777}
  </style>
</head>
<body>

<?php include('menu.php'); ?>

<div class="container">
  <h2>Importar Telefones em Lote (CSV)</h2>
  <p class="help-text">
    Envie o CSV com cabeçalho <code>NUMPAC;Nome;US_CODUNIDSAU;FONE_DDD;FONE</code> (Nome e US_CODUNIDSAU são ignorados).
    O lote <strong>só será salvo</strong> se houver pelo menos 1 telefone válido no CSV.
  </p>

  <!-- ÚNICO ACTION: SALVAR (faz criar/usar lote + import CSV) -->
  <form class="form-horizontal card" method="post" action="/ws/lote/lote_import.php" enctype="multipart/form-data" id="frmImport">

    <!-- Lote existente OU criar novo -->
    <div class="form-group">
      <label class="col-sm-2 control-label">Lote</label>
      <div class="col-sm-10">
        <div class="row">
          <div class="col-sm-6">
            <div class="panel panel-default">
              <div class="panel-heading"><strong>Usar lote existente</strong></div>
              <div class="panel-body">
                <input type="number" class="form-control" name="id_lote" min="1" placeholder="ID do lote existente (opcional)">
                <p class="help-text">Se informado (&gt;0), a importação usará este lote.</p>
              </div>
            </div>
          </div>
          <div class="col-sm-6">
            <div class="panel panel-default">
              <div class="panel-heading"><strong>Ou criar novo lote</strong></div>
              <div class="panel-body">
                <div class="form-group">
                  <label class="control-label">Descrição</label>
                  <input type="text" class="form-control" name="descricao" maxlength="120" placeholder="Ex.: Lote SMS Outubro">
                </div>
                <div class="form-group">
                  <label class="control-label">Data do Lote</label>
                  <input type="date" class="form-control" name="data_lote" placeholder="YYYY-MM-DD">
                </div>
                <div class="form-group">
                  <label class="control-label">Observação</label>
                  <textarea class="form-control" name="observacao" rows="2" placeholder="Observações (opcional)"></textarea>
                </div>
                <p class="help-text">
                  Se <code>id_lote</code> não for informado, <strong>descricao</strong> e <strong>data_lote</strong> serão obrigatórios (validados no backend).
                </p>
              </div>
            </div>
          </div>
        </div><!-- row -->
      </div>
    </div>

    <!-- CSV -->
    <div class="form-group">
      <label class="col-sm-2 control-label">Arquivo CSV</label>
      <div class="col-sm-10">
        <input type="file" name="arquivo" id="arquivo" class="form-control" accept=".csv,text/csv" required>
        <p class="help-text">
          Delimitador: <code>;</code><br>
          Mapeamento: <code>NUMPAC → nro</code>, <code>FONE_DDD + FONE → numero_telefone</code>.
        </p>
        <div class="checkbox">
          <label>
            <input type="checkbox" name="ignora_cabecalho" id="ignora_cabecalho" checked>
            Ignorar a primeira linha (cabeçalho)
          </label>
        </div>
      </div>
    </div>

    <!-- Ações: só SALVAR -->
    <div class="form-group">
      <div class="col-sm-offset-2 col-sm-10">
        <button type="submit" id="btnSalvar" class="btn btn-primary">Salvar</button>
        <a href="lote_cad.php" class="btn btn-default">Voltar</a>
        <p class="muted" style="margin-top:8px">
          O botão <strong>Salvar</strong> envia lote e CSV juntos para <code>/ws/lote/lote_import.php</code>.
        </p>
      </div>
    </div>
  </form>

  <div class="alert alert-info">
    <strong>Destino:</strong> <code>telefone_lote(id, id_lote, numero_telefone, nro, dtcad)</code>.<br>
    <strong>Regra:</strong> lote só é salvo se o CSV tiver ao menos 1 linha válida (garantido no backend).
  </div>
</div>

<?php include('botton.php'); ?>

<script>
  // UX: desabilita "Salvar" até ter arquivo selecionado
  (function(){
    var arquivo = document.getElementById('arquivo');
    var btnSalvar = document.getElementById('btnSalvar');
    if (arquivo && btnSalvar){
      var toggle = function(){ btnSalvar.disabled = (arquivo.files.length === 0); };
      toggle();
      arquivo.addEventListener('change', toggle);
    }
  })();
</script>

</body>
</html>
