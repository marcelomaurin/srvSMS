<!doctype html>
<html lang="pt-br" ng-app="RelatoriosSMS">
<head>
  <meta charset="utf-8">
  <title>Relatórios - SMS Enviados</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <!-- AngularJS 1.5.6 -->
  <script src="https://ajax.googleapis.com/ajax/libs/angularjs/1.5.6/angular.min.js"></script>

  <!-- Bootstrap 3 -->
  <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css">

  <!-- jQuery (necessário pro Bootstrap JS) -->
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js"></script>
  <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js"></script>

  <!-- Chart.js (para o gráfico) -->
  <script src="https://cdn.jsdelivr.net/npm/chart.js@2.9.4/dist/Chart.min.js"></script>

  <style>
    body { padding-top: 10px; }
    .section-title { margin: 15px 0 10px; }
    .table td { vertical-align: middle !important; }

    /* Layout full-width para priorizar o gráfico */
    .top-bar { padding-top:10px; }
    .filters { margin-bottom: 10px; }

    /* Área do gráfico: ocupa grande parte da tela */
    .chart-shell {
      position: relative;
      width: 100%;
      height: 70vh; /* gráfico grande por padrão */
      background: #fff;
      border: 1px solid #eee;
      border-radius: 4px;
      padding: 8px;
    }
    .chart-shell.fullscreen {
      position: fixed !important;
      top: 0; left: 0;
      width: 100vw !important;
      height: 100vh !important;
      z-index: 9999;
      border-radius: 0;
      border: none;
      padding: 10px;
      background: #fff;
    }
    .chart-actions {
      margin-bottom: 8px;
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      justify-content: flex-end;
    }
    .chart-wrap, .chart-wrap canvas {
      width: 100%;
      height: 100%;
    }
  </style>
</head>
<body ng-controller="RelCtrl" ng-init="init()">

<?php include('./menu.php'); ?>


  <!-- Área principal em tela cheia (container-fluid) -->
  <div class="container-fluid">

    <!-- Cabeçalho -->
    <div class="jumbotron">
      <h1>Relatórios de SMS Enviados</h1>
      <p>Quantidade de SMS com status <strong>Enviado</strong> por Dia, Semana (ISO), Mês ou Ano</p>
    </div>

    <!-- Filtros -->
    <form class="form-horizontal filters" ng-submit="carregar()">
      <div class="row">
        <div class="col-sm-2">
          <label>Agregação</label>
          <select class="form-control" ng-model="f.group">
            <option value="day">Dia</option>
            <option value="week">Semana</option>
            <option value="month">Mês</option>
            <option value="year">Ano</option>
          </select>
        </div>
        <div class="col-sm-2">
          <label>Início</label>
          <input type="date" class="form-control"
                 ng-model="f.start"
                 ng-model-options="{ timezone: 'UTC' }">
        </div>
        <div class="col-sm-2">
          <label>Fim</label>
          <input type="date" class="form-control"
                 ng-model="f.end"
                 ng-model-options="{ timezone: 'UTC' }">
        </div>
        <div class="col-sm-2">
          <label>&nbsp;</label>
          <button type="submit" class="btn btn-primary btn-block">Verificar</button>
        </div>
        <div class="col-sm-2">
          <label>&nbsp;</label>
          <button type="button" class="btn btn-default btn-block" ng-click="ultimosDias(30)">Últimos 30 dias</button>
        </div>
        <div class="col-sm-2">
          <label>&nbsp;</label>
          <button type="button" class="btn btn-default btn-block" ng-click="esteAno()">Este ano</button>
        </div>
      </div>
    </form>

    <!-- Alertas -->
    <div ng-if="msg" class="alert" ng-class="{'alert-info': msgType==='info', 'alert-success': msgType==='success', 'alert-danger': msgType==='error'}">
      {{msg}}
    </div>

    <!-- Ações do gráfico -->
    <div class="chart-actions">
      <button type="button" class="btn btn-default" ng-click="toggleFullscreen()">
        <span class="glyphicon" ng-class="{'glyphicon-resize-small': fullscreen, 'glyphicon-fullscreen': !fullscreen}"></span>
        {{ fullscreen ? 'Sair da tela cheia' : 'Tela cheia' }}
      </button>
      <button type="button" class="btn btn-info" ng-click="abrirModalDados()" ng-disabled="!rows || rows.length===0">
        <span class="glyphicon glyphicon-list-alt"></span> Ver dados (tabela)
      </button>
    </div>

    <!-- Gráfico em destaque (grande / tela cheia quando habilitado) -->
    <div class="chart-shell" ng-class="{'fullscreen': fullscreen}">
      <div class="chart-wrap">
        <canvas id="chartSMS"></canvas>
      </div>
    </div>

  </div> <!-- /container-fluid -->

  <!-- ===================== MODAL: DADOS (TABELA) ===================== -->
  <div id="modalDados" class="modal fade" tabindex="-1" role="dialog" aria-labelledby="tituloDados">
    <div class="modal-dialog modal-lg" role="document" style="width:95%; max-width:1100px;">
      <div class="modal-content">

        <div class="modal-header">
          <button type="button" class="close" ng-click="fecharModalDados()">&times;</button>
          <h4 class="modal-title" id="tituloDados">Dados do período</h4>
        </div>

        <div class="modal-body">
          <div class="table-responsive">
            <table class="table table-striped">
              <thead>
              <tr>
                <th style="width:50%;">Período</th>
                <th style="width:50%;">Total enviados</th>
              </tr>
              </thead>
              <tbody>
              <tr ng-repeat="r in rows">
                <td>{{r.label}}</td>
                <td>{{r.total}}</td>
              </tr>
              <tr ng-if="!rows || rows.length===0">
                <td colspan="2" class="text-muted">Nenhum dado no período selecionado.</td>
              </tr>
              </tbody>
            </table>
          </div>
        </div>

        <div class="modal-footer">
          <button type="button" class="btn btn-default" ng-click="fecharModalDados()">Fechar</button>
        </div>

      </div>
    </div>
  </div>
  <?php include('botton.php'); ?>

  <script>
    (function(){
      'use strict';

      var app = angular.module('RelatoriosSMS', []);

      app.controller('RelCtrl', function($scope, $http, $window) {

        var chart = null;

        $scope.f = {
          group: 'day',
          start: null, // Date object
          end:   null  // Date object
        };

        $scope.msg = '';
        $scope.msgType = 'info';
        $scope.rows = [];
        $scope.fullscreen = false;

        $scope.go = function(href){ $window.location.href = href; };
        $scope.emConstrucao = function(){ alert('Módulo em desenvolvimento 💻'); };

        $scope.init = function() {
          // padrão: últimos 30 dias por dia
          $scope.ultimosDias(30);
        };

        // ---- Helpers de data (UTC) ----
        function toYMD(d){
          if (!(d instanceof Date)) return '';
          var year  = d.getUTCFullYear();
          var month = (d.getUTCMonth()+1).toString().padStart(2,'0');
          var day   = d.getUTCDate().toString().padStart(2,'0');
          return year + '-' + month + '-' + day;
        }
        function todayUTC(){
          var now = new Date();
          return new Date(Date.UTC(now.getUTCFullYear(), now.getUTCMonth(), now.getUTCDate()));
        }
        function addDaysUTC(d, delta){
          var base = new Date(Date.UTC(d.getUTCFullYear(), d.getUTCMonth(), d.getUTCDate()));
          base.setUTCDate(base.getUTCDate() + delta);
          return base;
        }

        // Atalhos de período
        $scope.ultimosDias = function(n) {
          var end = todayUTC();
          var start = addDaysUTC(end, -(n-1));
          $scope.f.group = 'day';
          $scope.f.start = start;   // Date
          $scope.f.end   = end;     // Date
          $scope.carregar();
        };

        $scope.esteAno = function() {
          var now = todayUTC();
          var start = new Date(Date.UTC(now.getUTCFullYear(), 0, 1));
          var end   = new Date(Date.UTC(now.getUTCFullYear(), 11, 31));
          $scope.f.group = 'month';
          $scope.f.start = start;   // Date
          $scope.f.end   = end;     // Date
          $scope.carregar();
        };

        // Modal de dados (tabela)
        $scope.abrirModalDados = function() {
          $('#modalDados').modal('show');
        };
        $scope.fecharModalDados = function() {
          $('#modalDados').modal('hide');
        };

        // Tela cheia (apenas muda classe CSS e força resize do gráfico)
        $scope.toggleFullscreen = function() {
          $scope.fullscreen = !$scope.fullscreen;
          setTimeout(resizeChart, 50);
        };

        // Chamada WS (converte Date -> 'YYYY-MM-DD')
        $scope.carregar = function() {
          var params = {
            group: $scope.f.group || 'day',
            start: toYMD($scope.f.start),
            end:   toYMD($scope.f.end)
          };
          $http.get('/ws/sRptSMS.php', { params: params })
            .then(function(resp){
              var arr = Array.isArray(resp.data) ? resp.data : [];
              $scope.rows = arr;
              setMsg(
                'Período: ' + (params.start||'') + ' a ' + (params.end||'') +
                ' · Agrupamento: ' + labelGroup($scope.f.group||'day'), 'info'
              );
              var labels = arr.map(function(r){ return r.label; });
              var values = arr.map(function(r){ return r.total; });
              renderChart(labels, values);
            }, function(err){
              setMsg('Erro ao buscar dados.', 'error');
              $scope.rows = [];
              renderChart([], []);
            });
        };

        function setMsg(t, k){ $scope.msg=t||''; $scope.msgType=k||'info'; }
        function labelGroup(g){
          return {day:'Dia', week:'Semana', month:'Mês', year:'Ano'}[g] || g;
        }

        // Gráfico (Chart.js 2.x)
        function renderChart(labels, values) {
          var ctx = document.getElementById('chartSMS').getContext('2d');
          if (chart) { chart.destroy(); }
          chart = new Chart(ctx, {
            type: 'line',
            data: {
              labels: labels,
              datasets: [{
                label: 'SMS enviados',
                data: values,
                fill: false
              }]
            },
            options: {
              responsive: true,
              maintainAspectRatio: false, // permite ocupar toda a altura do contêiner
              animation: { duration: 200 },
              scales: {
                xAxes: [{ display: true }],
                yAxes: [{ display: true, ticks: { beginAtZero: true, precision: 0 } }]
              },
              tooltips: { mode: 'index', intersect: false }
            }
          });
          // Em alguns casos, é útil forçar o resize após renderizar
          setTimeout(resizeChart, 50);
        }

        function resizeChart() {
          if (chart) { chart.resize(); }
        }

        // Recalibra o gráfico ao redimensionar janela
        window.addEventListener('resize', resizeChart);

      });
    })();
  </script>

</body>
</html>
