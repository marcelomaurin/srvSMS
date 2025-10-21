<!doctype html>
<html lang="pt-br" ng-app="LoteApp">
<head>
  <meta charset="utf-8">
  <title>Cadastrar Lote + Importar CSV</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <script src="https://ajax.googleapis.com/ajax/libs/angularjs/1.5.6/angular.min.js"></script>
  <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css">
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js"></script>
  <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js"></script>

  <style>
    .help-text{font-size:12px;color:#777}
    .card{border:1px solid #ddd;border-radius:6px;padding:15px;margin-bottom:20px}
    .alert{margin-top:12px}
    .progress{margin-top:8px;height:18px}
    .progress-bar{line-height:18px}
  </style>
</head>
<body ng-controller="LoteController as vm">

<?php include('menu.php'); ?>

<div class="container">
  <h2>Cadastrar Lote + Importar Telefones (CSV)</h2>
  <p class="help-text">
    <strong>Salvar</strong> executa: criar o lote → enviar CSV temporário (em partes) → processar e gravar em <code>telefone_lote</code>.
    O lote só é salvo se existir ao menos 1 linha válida no CSV.
  </p>

  <!-- Angular orquestra as 3 etapas -->
  <form class="form-horizontal card" id="frmLoteCsv" onsubmit="return false;">

    <!-- Dados do lote -->
    <div class="form-group">
      <label class="col-sm-2 control-label">Descrição</label>
      <div class="col-sm-8">
        <input type="text" class="form-control" ng-model="vm.lote.descricao" required placeholder="Ex.: Lote SMS Outubro">
        <p class="help-text">Obrigatório para criar um novo lote (máx. 100 caracteres).</p>
      </div>
    </div>

    <div class="form-group">
      <label class="col-sm-2 control-label">Data do Lote</label>
      <div class="col-sm-4">
        <input type="date" class="form-control" ng-model="vm.lote.data_lote" required>
        <p class="help-text">Formato YYYY-MM-DD.</p>
      </div>
    </div>

    <div class="form-group">
      <label class="col-sm-2 control-label">Observação</label>
      <div class="col-sm-8">
        <textarea class="form-control" ng-model="vm.lote.observacao" rows="3" placeholder="Observações (opcional)"></textarea>
      </div>
    </div>

    <hr>

    <!-- CSV -->
    <div class="form-group">
      <label class="col-sm-2 control-label">Arquivo CSV</label>
      <div class="col-sm-8">
        <input type="file" class="form-control" id="arquivo" accept=".csv,text/csv" required>
        <p class="help-text">
          Delimitador: <code>;</code> | Cabeçalho: <code>NUMPAC;Nome;US_CODUNIDSAU;FONE_DDD;FONE</code><br>
          Mapeamento: <code>NUMPAC → nro_hygia</code> | <code>FONE_DDD + FONE → numero_telefone</code>. (Nome e US_CODUNIDSAU são ignorados)
        </p>
        <div class="checkbox">
          <label>
            <input type="checkbox" id="ignora_cabecalho" checked>
            Ignorar a primeira linha (cabeçalho)
          </label>
        </div>

        <!-- Progresso do upload em chunks -->
        <div class="progress" ng-show="vm.uploadTotal>0">
          <div class="progress-bar progress-bar-info" role="progressbar"
               aria-valuenow="{{vm.uploadPercent}}" aria-valuemin="0" aria-valuemax="100"
               style="width: {{vm.uploadPercent}}%;">
            {{vm.uploadPercent}}%
          </div>
        </div>
        <div class="help-text" ng-show="vm.uploadTotal>0">
          Enviando parte {{vm.uploadIndex}} de {{vm.uploadTotal}}...
        </div>
      </div>
    </div>

    <!-- Ações -->
    <div class="form-group">
      <div class="col-sm-offset-2 col-sm-8">
        <button type="button" id="btnSalvar" class="btn btn-primary" ng-disabled="vm.saving" ng-click="vm.salvar()">
          {{ vm.saving ? 'Processando...' : 'Salvar' }}
        </button>
        <button type="reset" class="btn btn-default" ng-disabled="vm.saving" ng-click="vm.limpar()">Limpar</button>
      </div>
    </div>
  </form>

  <div class="alert" ng-if="vm.msg" ng-class="{'alert-success': vm.ok, 'alert-info': vm.info, 'alert-danger': vm.err}">
    {{vm.msg}}
  </div>
</div>

<?php include('botton.php'); ?>

<script>
(function(){
  'use strict';
  angular.module('LoteApp', [])
  .controller('LoteController', function($http, $q, $timeout){
    var vm = this;
    vm.lote = { descricao:'', data_lote:'', observacao:'' };
    vm.msg = ''; vm.ok=false; vm.err=false; vm.info=false; vm.saving=false;

    // Estado do upload chunked
    vm.uploadIndex = 0;
    vm.uploadTotal = 0;
    vm.uploadPercent = 0;

    function setMsg(kind, text){
      vm.ok = vm.err = vm.info = false;
      vm[kind] = true; vm.msg = text;
    }

    function getFile(){
      var el = document.getElementById('arquivo');
      return el && el.files && el.files[0] ? el.files[0] : null;
    }

    function postJSON(url, obj){
      return $http.post(url, obj, { headers: { 'Content-Type':'application/json' }});
    }

    function postFormData(url, fd){
      // NÃO setar Content-Type manualmente (deixa o browser cuidar do boundary)
      return $http.post(url, fd, {
        transformRequest: angular.identity,
        headers: { 'Content-Type': undefined }
      });
    }

    function toYMD(d){
      if (!d) return '';
      if (Object.prototype.toString.call(d) === '[object Date]') {
        return d.toISOString().slice(0,10); // YYYY-MM-DD
      }
      var s = String(d);
      if (/^\d{4}-\d{2}-\d{2}/.test(s)) return s.slice(0,10);
      var m = s.match(/^(\d{2})\/(\d{2})\/(\d{4})$/);
      if (m) return m[3] + '-' + m[2] + '-' + m[1];
      return s;
    }

    // Gera um temp_id random para identificar o arquivo no servidor
    function makeTempId() {
      var a = new Uint8Array(16);
      (window.crypto || window.msCrypto).getRandomValues(a);
      var s = '';
      for (var i=0;i<a.length;i++){ s += ('0' + a[i].toString(16)).slice(-2); }
      return s;
    }

    // 1) Cria lote
    function stepCreateLote(){
      setMsg('info', '1/3 Criando lote...');
      var payload = {
        descricao:  vm.lote.descricao,
        data_lote:  toYMD(vm.lote.data_lote),
        observacao: vm.lote.observacao || ''
      };
      return postJSON('/ws/lote/lote_post.php', payload)
        .then(function(resp){
          if (!resp.data || !resp.data.id) throw new Error((resp.data && resp.data.msg) || 'Resposta inválida ao criar lote.');
          return resp.data.id;
        }, function(err){
          var msg = (err && err.data && (err.data.msg || err.data.message)) || 'Falha ao criar lote.';
          throw new Error(msg);
        });
    }

    // 2) Upload temporário em CHUNKS (2MB)
    function stepUploadTemp(){
      setMsg('info', '2/3 Enviando CSV temporário...');
      var file = getFile();
      if (!file) return $q.reject(new Error('Selecione o arquivo CSV.'));

      var CHUNK_SIZE = 2 * 1024 * 1024; // 2MB
      var total = Math.ceil(file.size / CHUNK_SIZE);
      var tempId = makeTempId();

      vm.uploadIndex = 0;
      vm.uploadTotal = total;
      vm.uploadPercent = 0;

      // Envia sequencialmente
      var sendChunk = function(index){
        var start = index * CHUNK_SIZE;
        var end   = Math.min(start + CHUNK_SIZE, file.size);
        var blob  = file.slice(start, end);

        var fd = new FormData();
        fd.append('arquivo', blob, 'chunk-' + index + '.part');
        fd.append('temp_id', tempId);
        fd.append('chunk_index', index);
        fd.append('total_chunks', total);

        return postFormData('/ws/lote/upload_temp.php', fd).then(function(resp){
          if (!resp.data || !resp.data.success) {
            throw new Error((resp.data && (resp.data.message || resp.data.msg)) || ('Falha no upload do chunk '+index));
          }
          vm.uploadIndex = index + 1;
          vm.uploadPercent = Math.floor((vm.uploadIndex / total) * 100);

          if (index + 1 < total) {
            setMsg('info', '2/3 Enviando CSV... ('+ vm.uploadIndex +'/'+ total +')');
            return sendChunk(index + 1);
          } else {
            // terminou
            return tempId;
          }
        }, function(err){
          var msg = (err && err.data && (err.data.message || err.data.msg)) || ('Erro no upload do chunk '+index);
          throw new Error(msg);
        });
      };

      return sendChunk(0);
    }

    // 3) Processa temporário + apaga
    function stepProcessTemp(id_lote, temp_id){
      setMsg('info', '3/3 Processando CSV e gravando telefone_lote...');
      var fd2 = new FormData();
      fd2.append('id_lote', id_lote);
      fd2.append('temp_id', temp_id);
      var ignora = document.getElementById('ignora_cabecalho').checked ? '1' : '';
      fd2.append('ignora_cabecalho', ignora);
      return postFormData('/ws/lote/process_temp.php', fd2)
        .then(function(resp){
          if (!resp.data || !resp.data.success) {
            throw new Error((resp.data && (resp.data.message || resp.data.msg)) || 'Falha ao processar CSV.');
          }
          return resp.data; // {success, id_lote, created_lote, importados, ignorados}
        }, function(err){
          var msg = (err && err.data && (err.data.message || err.data.msg)) || 'Falha ao processar CSV.';
          throw new Error(msg);
        });
    }

    vm.salvar = function(){
      if (vm.saving) return;
      vm.saving = true; setMsg('info','Validando dados...');
      if (!vm.lote.descricao || !vm.lote.data_lote) {
        setMsg('err','Descrição e Data do lote são obrigatórias.');
        vm.saving = false; return;
      }
      if (!getFile()){
        setMsg('err','Selecione o arquivo CSV.');
        vm.saving = false; return;
      }

      var idCriado = null, tempId = null;
      stepCreateLote()
        .then(function(id){ idCriado = id; return stepUploadTemp(); })
        .then(function(tid){ tempId = tid; return stepProcessTemp(idCriado, tempId); })
        .then(function(result){
          setMsg('ok', 'Importação concluída! Lote '+ result.id_lote +
                      ' — importados: ' + result.importados +
                      ', ignorados: ' + result.ignorados + '.');
          $timeout(vm.limpar, 500);
        })
        .catch(function(err){
          setMsg('err', (err && err.message) ? err.message : 'Erro inesperado durante o processo.');
        })
        .finally(function(){ vm.saving = false; vm.uploadIndex=0; vm.uploadTotal=0; vm.uploadPercent=0; });
    };

    vm.limpar = function(){
      vm.lote = { descricao:'', data_lote:'', observacao:'' };
      vm.ok = vm.err = vm.info = false; vm.msg = '';
      var arq = document.getElementById('arquivo'); if (arq) arq.value = '';
      var cb = document.getElementById('ignora_cabecalho'); if (cb) cb.checked = true;
      vm.uploadIndex=0; vm.uploadTotal=0; vm.uploadPercent=0;
    };
  });
})();
</script>

</body>
</html>
