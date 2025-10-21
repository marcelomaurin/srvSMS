<!doctype html>
<html lang="pt-br" ng-app="LoteListApp">
<head>
  <meta charset="utf-8">
  <title>Listar Lotes</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <!-- AngularJS / Bootstrap -->
  <script src="https://ajax.googleapis.com/ajax/libs/angularjs/1.5.6/angular.min.js"></script>
  <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css">
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js"></script>
  <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js"></script>

  <style>
    .filters .form-control { margin-bottom: 10px; }
    th.sortable { cursor: pointer; }
    th.sortable .caret { margin-left: 6px; }
    .table > tbody > tr > td { vertical-align: middle; }
  </style>
</head>
<body ng-controller="ListCtrl">

<?php include('menu.php'); ?>

<div class="container">
  <div class="page-header">
    <h2>Lotes <small>listagem e filtros</small></h2>
  </div>

  <!-- Ações -->
  <div class="clearfix" style="margin-bottom:10px;">
    <div class="pull-left">
      <a href="lote_cad.php" class="btn btn-primary">
        <span class="glyphicon glyphicon-plus"></span> Novo Lote
      </a>
    </div>
    <div class="pull-right">
      <button class="btn btn-default" ng-click="reload()">
        <span class="glyphicon glyphicon-refresh"></span> Recarregar
      </button>
    </div>
  </div>

  <!-- Filtros -->
  <form class="row filters" ng-submit="applyFilters()">
    <div class="col-sm-4">
      <label>Busca (descrição/observação)</label>
      <input type="text" class="form-control" ng-model="f.q" placeholder="Digite para filtrar...">
    </div>
    <div class="col-sm-3">
      <label>Data do lote - início</label>
      <input type="date" class="form-control" ng-model="f.start">
    </div>
    <div class="col-sm-3">
      <label>Data do lote - fim</label>
      <input type="date" class="form-control" ng-model="f.end">
    </div>
    <div class="col-sm-2">
      <label>&nbsp;</label>
      <div>
        <button type="submit" class="btn btn-primary btn-block">Filtrar</button>
      </div>
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
          <th class="sortable" ng-click="sortBy('id')">
            ID
            <span ng-if="sort.col==='id'">
              <span class="caret" ng-class="{'caret': sort.dir==='asc'}"></span>
            </span>
          </th>
          <th class="sortable" ng-click="sortBy('descricao')">
            Descrição
            <span ng-if="sort.col==='descricao'">
              <span class="caret" ng-class="{'caret': sort.dir==='asc'}"></span>
            </span>
          </th>
          <th>Observação</th>
          <th class="sortable" ng-click="sortBy('data_lote')">
            Data do Lote
            <span ng-if="sort.col==='data_lote'">
              <span class="caret" ng-class="{'caret': sort.dir==='asc'}"></span>
            </span>
          </th>
          <th class="hidden-xs">Cadastro</th>
          <th style="width:220px;">Ações</th>
        </tr>
      </thead>
      <tbody>
        <tr ng-repeat="r in rows">
          <td>{{r.id}}</td>
          <td>{{r.descricao}}</td>
          <td>{{r.observacao || '-'}}</td>
          <td>{{r.data_lote | dateBR}}</td>
          <td class="hidden-xs">{{r.dtcad | dateTimeBR}}</td>
          <td>
            <div class="btn-group">
              <a class="btn btn-default btn-sm" ng-href="telefone_lote_list.php?id_lote={{r.id}}">
                <span class="glyphicon glyphicon-earphone"></span> Telefones
              </a>
              <a class="btn btn-default btn-sm" ng-href="lote_cad.php?id={{r.id}}">
                <span class="glyphicon glyphicon-edit"></span> Editar
              </a>
              <button class="btn btn-danger btn-sm" ng-click="confirmDelete(r)">
                <span class="glyphicon glyphicon-trash"></span> Excluir
              </button>
            </div>
          </td>
        </tr>
        <tr ng-if="!rows.length && !loading">
          <td colspan="6" class="text-muted">Nenhum lote encontrado.</td>
        </tr>
        <tr ng-if="loading">
          <td colspan="6" class="text-muted">Carregando...</td>
        </tr>
      </tbody>
    </table>
  </div>

  <!-- Paginação -->
  <div class="row" ng-if="total > 0">
    <div class="col-sm-6">
      <p class="text-muted" style="margin-top:7px;">
        Mostrando {{ (pager.offset + 1) }}–{{ Math.min(pager.offset + rows.length, total) }} de {{ total }}
      </p>
    </div>
    <div class="col-sm-6 text-right">
      <div class="btn-group">
        <button class="btn btn-default" ng-disabled="pager.page===1" ng-click="goPage(1)">&laquo;</button>
        <button class="btn btn-default" ng-disabled="pager.page===1" ng-click="goPage(pager.page-1)">&lsaquo;</button>
        <button class="btn btn-default disabled">Página {{pager.page}}</button>
        <button class="btn btn-default" ng-disabled="pager.page>=pager.last" ng-click="goPage(pager.page+1)">&rsaquo;</button>
        <button class="btn btn-default" ng-disabled="pager.page>=pager.last" ng-click="goPage(pager.last)">&raquo;</button>
      </div>
      <select class="btn btn-default" ng-model="pager.limit" ng-change="changeLimit()"
              ng-options="s for s in [10,20,50,100]" style="margin-left:8px;"></select>
    </div>
  </div>
</div>

<?php include('botton.php'); ?>

<script>
  var app = angular.module('LoteListApp', []);

  // Filtros de data
  app.filter('dateBR', function(){
    return function(iso){
      if(!iso) return '';
      // aceita 'YYYY-MM-DD' ou ISO datetime
      var d = new Date(iso);
      if(isNaN(d.getTime())){
        // tenta manual para 'YYYY-MM-DD'
        var p = (''+iso).split('-');
        if(p.length===3) return p[2]+'/'+p[1]+'/'+p[0];
        return iso;
      }
      var dd = ('0'+d.getUTCDate()).slice(-2);
      var mm = ('0'+(d.getUTCMonth()+1)).slice(-2);
      var yy = d.getUTCFullYear();
      return dd+'/'+mm+'/'+yy;
    };
  });

  app.filter('dateTimeBR', function(){
    return function(iso){
      if(!iso) return '';
      var d = new Date(iso);
      if(isNaN(d.getTime())) return iso;
      var dd = ('0'+d.getUTCDate()).slice(-2);
      var mm = ('0'+(d.getUTCMonth()+1)).slice(-2);
      var yy = d.getUTCFullYear();
      var hh = ('0'+d.getUTCHours()).slice(-2);
      var mi = ('0'+d.getUTCMinutes()).slice(-2);
      return dd+'/'+mm+'/'+yy+' '+hh+':'+mi;
    };
  });

  app.controller('ListCtrl', function($scope, $http){
    $scope.rows   = [];
    $scope.total  = 0;
    $scope.loading = false;

    $scope.msg = '';
    $scope.msgType = 'info';

    $scope.f = { q:'', start:null, end:null };

    $scope.sort = { col: 'data_lote', dir: 'desc' };

    $scope.pager = {
      page: 1,
      limit: 20,
      offset: 0,
      last: 1
    };

    function setMsg(type, text){
      $scope.msgType = type;
      $scope.msg = text;
    }

    function toYMD(v){
      if(!v) return '';
      if(Object.prototype.toString.call(v)==='[object Date]'){
        return v.toISOString().slice(0,10);
      }
      var s = String(v);
      if(/^\d{4}-\d{2}-\d{2}/.test(s)) return s.slice(0,10);
      var m = s.match(/^(\d{2})\/(\d{2})\/(\d{4})$/);
      if(m) return m[3]+'-'+m[2]+'-'+m[1];
      return s;
    }

    function buildParams(){
      return {
        q: $scope.f.q || '',
        start: toYMD($scope.f.start) || '',
        end: toYMD($scope.f.end) || '',
        order: $scope.sort.col || 'data_lote',
        dir: $scope.sort.dir || 'desc',
        limit: $scope.pager.limit,
        offset: $scope.pager.offset
      };
    }

    $scope.fetch = function(){
      $scope.loading = true;
      setMsg('info','Carregando...');
      $http.get('/ws/lote/lote_get.php', { params: buildParams() })
        .then(function(resp){
          var data = resp.data;
          // Formato preferido: { ok, rows, total }
          if (data && data.rows && angular.isArray(data.rows)) {
            $scope.rows = data.rows;
            $scope.total = parseInt(data.total||data.rows.length,10)||0;
          } else if (angular.isArray(data)) {
            // Fallback: array puro
            $scope.rows = data.slice(0, $scope.pager.limit);
            $scope.total = data.length;
          } else {
            $scope.rows = [];
            $scope.total = 0;
          }
          // calcula última página
          $scope.pager.last = Math.max(1, Math.ceil($scope.total / $scope.pager.limit));
          setMsg('success','Ok!');
        }, function(err){
          $scope.rows = []; $scope.total = 0;
          setMsg('error', (err && err.data && (err.data.msg||err.data.message)) || 'Falha ao carregar lotes.');
        })
        .finally(function(){
          $scope.loading = false;
        });
    };

    $scope.applyFilters = function(){
      $scope.pager.page = 1;
      $scope.pager.offset = 0;
      $scope.fetch();
    };

    $scope.sortBy = function(col){
      if($scope.sort.col === col){
        $scope.sort.dir = ($scope.sort.dir === 'asc' ? 'desc' : 'asc');
      } else {
        $scope.sort.col = col;
        $scope.sort.dir = 'asc';
      }
      $scope.fetch();
    };

    $scope.changeLimit = function(){
      $scope.pager.page = 1;
      $scope.pager.offset = 0;
      $scope.fetch();
    };

    $scope.goPage = function(p){
      if(p < 1) p = 1;
      if(p > $scope.pager.last) p = $scope.pager.last;
      $scope.pager.page = p;
      $scope.pager.offset = (p-1) * $scope.pager.limit;
      $scope.fetch();
    };

    $scope.reload = function(){
      $scope.fetch();
    };

    $scope.confirmDelete = function(row){
      if(!row || !row.id) return;
      if(!window.confirm('Excluir o lote #'+row.id+'? Esta ação é irreversível.')) return;

      $http.post('/ws/lote/lote_delete.php', { id: row.id }, { headers: { 'Content-Type':'application/json' } })
        .then(function(resp){
          if (resp.data && resp.data.ok){
            setMsg('success','Lote excluído com sucesso.');
            $scope.fetch();
          } else {
            setMsg('error', (resp.data && (resp.data.msg||resp.data.message)) || 'Falha ao excluir o lote.');
          }
        }, function(err){
          setMsg('error', (err && err.data && (err.data.msg||err.data.message)) || 'Falha ao excluir o lote.');
        });
    };

    // Inicial
    $scope.fetch();
  });
</script>

</body>
</html>
