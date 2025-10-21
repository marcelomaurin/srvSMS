<!-- menu.php -->
<div class="container-fluid top-bar" style="padding-top:10px;">
  <div class="row">
    <div class="col-sm-12">
      <div class="btn-group">
        <button type="button" class="btn btn-default" onclick="window.location.href='index.php'">Menu Principal</button>
        <button type="button" class="btn btn-primary" onclick="window.location.href='Envia_SMS.php'">Envia SMS</button>
        <button type="button" class="btn btn-default" onclick="window.location.href='Relatorios.php'">Relatórios</button>

        <!-- Lotes -->
        <div class="btn-group">
          <button type="button" class="btn btn-default dropdown-toggle" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
            Lotes <span class="caret"></span>
          </button>
          <ul class="dropdown-menu">
            <li><a href="lote_cad.php">Cadastrar Lote</a></li>
            <li><a href="lote_list.php">Listar Lotes</a></li>
          </ul>
        </div>

        <!-- Telefones por Lote -->
        <div class="btn-group">
          <button type="button" class="btn btn-default dropdown-toggle" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
            Telefones / Lote <span class="caret"></span>
          </button>
          <ul class="dropdown-menu">
            <li><a href="telefone_lote_list.php">Listar Telefones por Lote</a></li>
            <li role="separator" class="divider"></li>
           
          </ul>
        </div>

        <button type="button" class="btn btn-default" onclick="alert('Módulo em desenvolvimento 💻')">Configurações (em breve)</button>
      </div>
    </div>
  </div>
  <hr>
</div>
