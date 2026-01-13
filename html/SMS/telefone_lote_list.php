<!doctype html>
<html lang="pt-br" ng-app="TelefoneLoteListApp">
<head>
  <meta charset="utf-8">
  <title>Telefones por Lote</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <!-- AngularJS / Bootstrap -->
  <script src="https://ajax.googleapis.com/ajax/libs/angularjs/1.5.6/angular.min.js"></script>
  <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css">
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js"></script>
  <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js"></script>

  <style>
    .filters .form-control { margin-bottom: 10px; }
    th.sortable { cursor: pointer; }
    .table > tbody > tr > td { vertical-align: middle; }
    .nowrap { white-space: nowrap; }
    .tool-inline .form-control { display:inline-block; width:auto; vertical-align:middle; }
    .tool-inline .btn { vertical-align:middle; }
    .tool-inline .label-inline { margin-right:6px; }
  </style>
</head>
<body ng-controller="ListCtrl">

<?php include('menu.php'); ?>

<div class="container">
  <div class="page-header">
    <h2>Telefones por Lote <small>consulta, filtros e ações</small></h2>
  </div>

  <!-- Ações -->
  <div class="clearfix" style="margin-bottom:10px;">
    <div class="pull-left">
      <a class="btn btn-primary" ng-href="telefone_lote_cad.php?id_lote={{f.id_lote||''}}">
        <span class="glyphicon glyphicon-plus"></span> Incluir Telefone
      </a>
      <a class="btn btn-default" ng-href="lote_list.php">
        <span class="glyphicon glyphicon-th-list"></span> Ir para Lotes
      </a>
    </div>

    <!-- Ferramentas em linha -->
    <div class="pull-right tool-inline">
      <span class="label-inline text-muted">Prefixo:</span>
      <input type="text" class="form-control input-sm" ng-model="f.prefix" placeholder="Prefixo" style="width:110px;">
      <button class="btn btn-warning btn-sm" ng-click="bulkDeleteByPrefix()" title="Excluir todos os telefones que começam com o prefixo informado no lote selecionado">
        <span class="glyphicon glyphicon-filter"></span> Excluir prefixo
      </button>

      <span class="label-inline text-muted" style="margin-left:12px;">Lote:</span>
      <input type="text" class="form-control input-sm" ng-model="f.id_lote" style="width:90px;" placeholder="ID do lote">
      <button class="btn btn-danger btn-sm" ng-click="deleteWholeLote()" title="Excluir lote inteiro">
        <span class="glyphicon glyphicon-trash"></span> Excluir lote
      </button>

      <!-- Submeter Lote -->
      <button class="btn btn-success btn-sm" ng-click="openSubmitModal()" title="Submeter telefones do lote para a fila (jobs)" style="margin-left:8px;">
        <span class="glyphicon glyphicon-send"></span> Submeter lote
      </button>

      <button class="btn btn-default btn-sm" ng-click="reload()" style="margin-left:8px;">
        <span class="glyphicon glyphicon-refresh"></span> Recarregar
      </button>
    </div>
  </div>

  <!-- Filtros -->
  <form class="row filters" ng-submit="applyFilters()">
    <div class="col-sm-4">
      <label>Lote</label>
      <select class="form-control"
              ng-model="f.id_lote"
              ng-options="l.id as (l.id + ' - ' + l.descricao + ' (' + (l.data_lote|dateBR) + ')') for l in lotes">
        <option value="">— Todos os lotes —</option>
      </select>
    </div>
    <div class="col-sm-4">
      <label>Busca</label>
      <input type="text" class="form-control" ng-model="f.q" placeholder="Telefone ou Nº">
    </div>
    <div class="col-sm-2">
      <label>&nbsp;</label>
      <button type="submit" class="btn btn-primary btn-block">Filtrar</button>
    </div>
  </form>

  <div class="alert" ng-if="msg" ng-class="{'alert-info': msgType==='info','alert-success':msgType==='success','alert-danger':msgType==='error'}">
    {{msg}}
  </div>

  <!-- Tabela -->
  <div class="table-responsive">
    <table class="table table-striped table-hover">
      <thead>
        <tr>
          <th class="sortable" ng-click="sortBy('id')">ID</th>
          <th class="sortable" ng-click="sortBy('id_lote')">Lote</th>
          <th class="sortable" ng-click="sortBy('numero_telefone')">Telefone</th>
          <th class="sortable" ng-click="sortBy('nro')">Nº</th>
          <th class="hidden-xs">Cadastro</th>
          <th style="width:170px;">Ações</th>
        </tr>
      </thead>
      <tbody>
        <tr ng-repeat="r in pageRows = (rows | filter:searchFn | orderBy:orderExpr) | limitTo: pager.pageSize : (pager.page-1)*pager.pageSize">
          <td class="nowrap">{{r.id}}</td>
          <td class="nowrap">
            <a ng-href="lote_cad.php?id={{r.id_lote}}" title="Abrir lote">{{r.id_lote}}</a>
          </td>
          <td class="nowrap">{{r.numero_telefone}}</td>
          <td class="nowrap">{{r.nro}}</td>
          <td class="hidden-xs nowrap">{{r.dtcad | dateTimeBR}}</td>
          <td>
            <div class="btn-group">
              <a class="btn btn-default btn-sm" ng-href="telefone_lote_cad.php?id_lote={{r.id_lote}}">
                <span class="glyphicon glyphicon-plus"></span>
              </a>
              <button class="btn btn-danger btn-sm" ng-click="confirmDelete(r)">
                <span class="glyphicon glyphicon-trash"></span> Excluir
              </button>
            </div>
          </td>
        </tr>
        <tr ng-if="!rows.length">
          <td colspan="6" class="text-muted">Nenhum telefone encontrado.</td>
        </tr>
      </tbody>
    </table>
  </div>

  <!-- Paginação -->
  <div class="row" ng-if="rows.length">
    <div class="col-sm-6">
      <p class="text-muted" style="margin-top:7px;">
        Mostrando {{(pager.page-1)*pager.pageSize + (pageRows.length?1:0)}}–
        {{(pager.page-1)*pager.pageSize + pageRows.length}} de {{(rows | filter:searchFn).length}}
      </p>
    </div>
    <div class="col-sm-6 text-right">
      <div class="btn-group">
        <button class="btn btn-default" ng-disabled="pager.page===1" ng-click="pager.page=1">&laquo;</button>
        <button class="btn btn-default" ng-disabled="pager.page===1" ng-click="pager.page=pager.page-1">&lsaquo;</button>
        <button class="btn btn-default disabled">Página {{pager.page}}</button>
        <button class="btn btn-default" ng-disabled="(pager.page*pager.pageSize)>= (rows | filter:searchFn).length" ng-click="pager.page=pager.page+1">&rsaquo;</button>
      </div>
      <select class="btn btn-default" ng-model="pager.pageSize" ng-change="pager.page=1"
              ng-options="s for s in [10,20,50,100]" style="margin-left:8px;"></select>
    </div>
  </div>
</div>

<!-- Modal Submeter Lote -->
<div id="modalSubmit" class="modal fade" tabindex="-1" role="dialog">
  <div class="modal-dialog" role="document">
    <div class="modal-content">
      <div class="modal-header bg-success text-white">
        <button type="button" class="close" data-dismiss="modal" aria-label="Fechar"><span>&times;</span></button>
        <h4 class="modal-title">Submeter Telefones do Lote</h4>
      </div>
      <div class="modal-body">
        <form onsubmit="return false;">
          <div class="form-group">
            <label>ID Inicial</label>
            <input type="number" class="form-control" ng-model="submit.id_inicial" placeholder="Ex: 100">
          </div>
          <div class="form-group">
            <label>ID Final</label>
            <input type="number" class="form-control" ng-model="submit.id_final" placeholder="Ex: 500">
          </div>

          <div class="form-group">
            <label>Telefones extras (início)</label>
            <textarea class="form-control" rows="3" ng-model="submit.extra_inicio"
              placeholder="Um por linha. Aceita dígitos com/sem máscara."></textarea>
            <p class="help-block">Serão inseridos <b>antes</b> dos telefones da faixa.</p>
          </div>

          <div class="form-group">
            <label>Telefones extras (final)</label>
            <textarea class="form-control" rows="3" ng-model="submit.extra_fim"
              placeholder="Um por linha. Aceita dígitos com/sem máscara."></textarea>
            <p class="help-block">Serão inseridos <b>depois</b> dos telefones da faixa.</p>
          </div>

          <div class="form-group">
            <label>Mensagem</label>
            <textarea class="form-control" rows="4" ng-model="submit.mensagem" maxlength="2000"
              placeholder="Digite a mensagem. Use '\n' ou pule linha para gerar múltiplos jobs (cada linha/trecho vira um registro)."></textarea>
            <p class="help-block">
              Cada parte (entre quebras) vira um job. Ex.: <code>Bom dia \n Esta é outra \n E mais uma</code> &rarr; 3 mensagens.
            </p>
          </div>
        </form>
      </div>
      <div class="modal-footer">
        <span class="pull-left text-muted" ng-if="submitCountPreview">
          Previsto: {{submitCountPreview}} inserts (telefones × mensagens)
        </span>
        <button type="button" class="btn btn-default" data-dismiss="modal">Fechar</button>
        <button type="button" class="btn btn-success" ng-click="submitLoteJobs()">Enviar</button>
      </div>
    </div>
  </div>
</div>

<?php include('botton.php'); ?>

<script>
  var app = angular.module('TelefoneLoteListApp', []);

  // Filtros de data/tempo padrão BR
  app.filter('dateBR', function(){
    return function(iso){
      if(!iso) return '';
      var d = new Date(iso); if(isNaN(d)) {
        var p = (''+iso).split('-'); if(p.length===3) return p[2]+'/'+p[1]+'/'+p[0];
        return iso;
      }
      var dd=('0'+d.getUTCDate()).slice(-2), mm=('0'+(d.getUTCMonth()+1)).slice(-2), yy=d.getUTCFullYear();
      return dd+'/'+mm+'/'+yy;
    };
  });
  app.filter('dateTimeBR', function(){
    return function(iso){
      if(!iso) return '';
      var d = new Date(iso); if(isNaN(d)) return iso;
      var dd=('0'+d.getUTCDate()).slice(-2), mm=('0'+(d.getUTCMonth()+1)).slice(-2), yy=d.getUTCFullYear();
      var hh=('0'+d.getUTCHours()).slice(-2), mi=('0'+d.getUTCMinutes()).slice(-2);
      return dd+'/'+mm+'/'+yy+' '+hh+':'+mi;
    };
  });

  app.controller('ListCtrl', function($scope, $http, $window){
    $scope.rows = [];
    $scope.lotes = [];
    $scope.msg = '';
    $scope.msgType = 'info';

    // prefixo default = 163 e id_lote vazio
    $scope.f = { id_lote: null, q: '', prefix: '163' };

    $scope.sort = { col: 'id', dir: 'desc' };
    $scope.orderExpr = function(item){
      var c = $scope.sort.col || 'id';
      var val = item[c];
      if(c==='dtcad'){ return new Date(val).getTime(); }
      if(typeof val === 'number'){ return val; }
      return (val||'').toString().toLowerCase();
    };
    $scope.sortBy = function(col){
      if($scope.sort.col === col){
        $scope.sort.dir = ($scope.sort.dir==='asc' ? 'desc' : 'asc');
      } else {
        $scope.sort.col = col; $scope.sort.dir = 'asc';
      }
      var expr = $scope.orderExpr;
      $scope.orderExpr = ($scope.sort.dir==='asc') ? expr : function(item){ return -1 * expr(item); };
    };

    $scope.pager = { page: 1, pageSize: 20 };

    function getParam(name){
      var m = new RegExp('[?&]' + name + '=([^&]*)').exec($window.location.search);
      return m && decodeURIComponent(m[1].replace(/\+/g, ' '));
    }

    $scope.applyFilters = function(){ $scope.pager.page = 1; };

    $scope.searchFn = function(row){
      if($scope.f.id_lote && row.id_lote != $scope.f.id_lote) return false;
      var q = ($scope.f.q||'').toLowerCase();
      if(q){
        var hit = (row.numero_telefone||'').toLowerCase().indexOf(q) !== -1 ||
                  (''+(row.nro||'')).toLowerCase().indexOf(q) !== -1;
        if(!hit) return false;
      }
      return true;
    };

    $scope.reload = function(){
      $scope.msg = 'Carregando...'; $scope.msgType = 'info';
      $http.get('/ws/lote/lote_get.php').then(function(resp){
        $scope.lotes = Array.isArray(resp.data) ? resp.data : [];
        var idQS = parseInt(getParam('id_lote')||'', 10);
        if(!isNaN(idQS)) $scope.f.id_lote = idQS;
      });

      $http.get('/ws/telefone_lote/telefone_lote_get.php').then(function(resp){
        $scope.rows = Array.isArray(resp.data) ? resp.data : [];
        $scope.msg = 'Registros: ' + $scope.rows.length; $scope.msgType='success';
        $scope.pager.page = 1;
      }, function(){
        $scope.rows = [];
        $scope.msg = 'Erro ao carregar registros.'; $scope.msgType='error';
      });
    };

    $scope.confirmDelete = function(row){
      if(!row || !row.id) return;
      if(confirm('Excluir este telefone do lote?\nTelefone: ' + (row.numero_telefone||'') + '\nNro: ' + (row.nro||'') )){
        $http.post('/ws/telefone_lote/telefone_lote_delete.php', { id: row.id }, { headers: {'Content-Type':'application/json'} })
          .then(function(){
            $scope.rows = $scope.rows.filter(function(x){ return x.id !== row.id; });
            $scope.msg = 'Registro excluído.'; $scope.msgType='success';
          }, function(){
            $scope.msg = 'Erro ao excluir registro.'; $scope.msgType='error';
          });
      }
    };

    $scope.bulkDeleteByPrefix = function(){
      var prefix = ($scope.f.prefix||'').trim();
      var idLote = $scope.f.id_lote;
      if(!idLote){ $scope.msg = 'Selecione um lote para excluir por prefixo.'; $scope.msgType='error'; return; }
      if(!prefix){ $scope.msg = 'Informe um prefixo (ex.: 163).'; $scope.msgType='error'; return; }
      if(!confirm('Excluir TODOS os telefones do lote '+idLote+' que começam com "'+prefix+'"?')){ return; }

      $http.post('/ws/telefone_lote/telefone_lote_delete_by_prefix.php',
        { id_lote: idLote, prefix: prefix }, { headers: { 'Content-Type':'application/json' }})
        .then(function(resp){
          var deleted = (resp.data && (resp.data.deleted || resp.data.count || 0)) || 0;
          $scope.rows = $scope.rows.filter(function(x){
            return !(x.id_lote == idLote && String(x.numero_telefone||'').indexOf(prefix) === 0);
          });
          $scope.msg = 'Exclusão por prefixo concluída. Removidos: '+deleted+'.'; $scope.msgType='success';
        }, function(err){
          var m = (err && err.data && (err.data.msg || err.data.message)) || 'Falha na exclusão por prefixo.';
          $scope.msg = m; $scope.msgType='error';
        });
    };

    $scope.deleteWholeLote = function(){
      var idLote = $scope.f.id_lote;
      if(!idLote){ $scope.msg = 'Informe/seleciona um ID de lote para excluir.'; $scope.msgType='error'; return; }
      if(!confirm('Excluir o LOTE '+idLote+' e todos os seus telefones? Esta ação é irreversível.')){ return; }

      $http.post('/ws/lote/lote_delete.php', { id: idLote }, { headers: { 'Content-Type':'application/json' }})
        .then(function(resp){
          if(resp.data && resp.data.ok){
            $scope.rows = $scope.rows.filter(function(x){ return x.id_lote != idLote; });
            $scope.lotes = $scope.lotes.filter(function(l){ return l.id != idLote; });
            $scope.f.id_lote = null;
            $scope.msg = 'Lote '+idLote+' excluído com sucesso.'; $scope.msgType='success';
          } else {
            $scope.msg = (resp.data && (resp.data.msg||resp.data.message)) || 'Falha ao excluir o lote.'; $scope.msgType='error';
          }
        }, function(err){
          var m = (err && err.data && (err.data.msg || err.data.message)) || 'Falha ao excluir o lote.';
          $scope.msg = m; $scope.msgType='error';
        });
    };

    // ---- SUBMETER LOTE (com extras e múltiplas mensagens) ----
    $scope.openSubmitModal = function(){
      if(!$scope.f.id_lote){
        $scope.msg = 'Selecione um lote antes de submeter.';
        $scope.msgType='error';
        return;
      }
      $scope.submit = { id_inicial:'', id_final:'', mensagem:'', extra_inicio:'', extra_fim:'' };
      $scope.submitCountPreview = 0;
      $('#modalSubmit').modal('show');
    };

    function splitMessages(raw){
      if(!raw) return [];
      // converte "\n" literal em quebra real, depois quebra por linhas (uma ou mais)
      var txt = String(raw).replace(/\\n/g, '\n');
      return txt.split(/\n+/).map(function(s){ return s.trim(); }).filter(function(s){ return s.length>0; });
    }

    function parsePhones(block){
      // separa por quebra de linha, vírgula, ponto e vírgula ou espaço
      var arr = String(block||'').split(/[\n,;\s]+/);
      arr = arr.map(function(p){ return (p||'').replace(/\D+/g,''); }).filter(function(p){ return p.length>0; });
      // remove duplicados mantendo ordem
      var seen = {}; var out = [];
      arr.forEach(function(p){ if(!seen[p]){ seen[p]=1; out.push(p); }});
      return out;
    }

    $scope.submitLoteJobs = function(){
      var idLote = $scope.f.id_lote;
      var ini = parseInt($scope.submit.id_inicial||0, 10);
      var fim = parseInt($scope.submit.id_final||0, 10);
      var parts = splitMessages($scope.submit.mensagem);
      var extrasIni = parsePhones($scope.submit.extra_inicio);
      var extrasFim = parsePhones($scope.submit.extra_fim);

      if(!idLote || !ini || !fim || parts.length===0){
        alert('Informe ID inicial, ID final e pelo menos 1 mensagem.'); return;
      }
      if(fim < ini){ alert('O ID final deve ser maior ou igual ao inicial.'); return; }
      // valida tamanho de cada mensagem
      for(var i=0;i<parts.length;i++){
        if(parts[i].length > 500){ alert('Mensagem '+(i+1)+' ultrapassa 500 caracteres.'); return; }
      }

      if(!confirm('Submeter telefones do lote '+idLote+' (IDs '+ini+' até '+fim+') com '
        + parts.length + ' mensagem(ns), extras início: '+extrasIni.length+', extras fim: '+extrasFim.length+'?')){
        return;
      }

      var payload = {
        id_lote: idLote,
        id_inicial: ini,
        id_final: fim,
        mensagens: parts,
        extras_inicio: extrasIni,
        extras_fim: extrasFim
      };

      $http.post('/ws/jobs/jobs_insert_from_lote.php', payload, { headers:{'Content-Type':'application/json'} })
        .then(function(resp){
          var inserted = (resp.data && (resp.data.inserted || resp.data.count)) ? (resp.data.inserted || resp.data.count) : 0;
          $scope.msg = 'Submissão concluída. '+inserted+' jobs adicionados.';
          $scope.msgType = 'success';
          $('#modalSubmit').modal('hide');
        }, function(err){
          var m = (err && err.data && (err.data.msg||err.data.message)) || 'Erro ao submeter lote.';
          $scope.msg = m; $scope.msgType='error';
        });
    };
    // -----------------------------------------------------------

    // init
    $scope.reload();
  });
</script>

</body>
</html>
