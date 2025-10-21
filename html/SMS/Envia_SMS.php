<!doctype html>
<html lang="pt-br" ng-app="Envia_SMS">
<head>
  <meta charset="utf-8">
  <title>Envio de SMS Gratuito</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <!-- AngularJS 1.5.6 -->
  <script src="https://ajax.googleapis.com/ajax/libs/angularjs/1.5.6/angular.min.js"></script>

  <!-- Bootstrap 3 -->
  <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css">

  <!-- jQuery (necessário pro Bootstrap JS) -->
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js"></script>
  <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js"></script>

  <style>
    body { padding-top: 10px; }
    .section-title { margin: 20px 0 10px; }
    .table td { vertical-align: middle !important; }
    .modal-dialog { max-width: 800px; width: 95%; }
  </style>
</head>
<body ng-controller="cntrl" ng-init="displayJob('')">

  <!-- Menu Superior -->
<?php include('./menu.php'); ?>


  <div class="container">

    <!-- Cabeçalho -->
    <div class="jumbotron">
      <h1>CRUD de Envio de SMS</h1>
      <p>Apresentação de conhecimento utilizando AngularJS e PHP</p>
      <p>Contato: <a href="mailto:marcelomaurinmartins@gmail.com">marcelomaurinmartins@gmail.com</a></p>
    </div>

    <!-- Filtro e ações -->
    <form class="form-horizontal" ng-submit="displayJob(ptelefone)">
      <div class="row">
        <div class="col-sm-1"><label class="control-label">Telefone:</label></div>
        <div class="col-sm-4">
          <input class="form-control" placeholder="Telefone (opcional)" type="text" ng-model="ptelefone" name="ptelefone">
        </div>
        <div class="col-sm-2">
          <button type="submit" class="btn btn-primary btn-block">Pesquisar</button>
        </div>
        <div class="col-sm-2">
          <button type="button" class="btn btn-success btn-block" ng-click="abrirModalInsert()">Novo Item</button>
        </div>
      </div>
    </form>

    <!-- Alertas -->
    <div class="section-title"></div>
    <div ng-if="msg" class="alert" ng-class="{'alert-info': msgType==='info', 'alert-success': msgType==='success', 'alert-danger': msgType==='error'}">
      {{msg}}
    </div>

    <!-- Resultado -->
    <div class="panel panel-default">
      <div class="panel-heading"><strong>Resultados</strong></div>
      <div class="table-responsive">
        <table class="table table-striped">
          <thead>
          <tr>
            <th>Id</th>
            <th>Telefone</th>
            <th>Mensagem</th>
            <th>Status</th>
            <th style="width: 220px;">Ações</th>
          </tr>
          </thead>
          <tbody>
          <tr ng-repeat="sms in data track by sms.idjob">
            <td>{{sms.idjob}}</td>
            <td>{{sms.telefone}}</td>
            <td>{{sms.mensagem}}</td>
            <td>{{sms.status}}</td>
            <td>
              <div class="btn-group">
                <button class="btn btn-danger" ng-click="deleteJob(sms.idjob)">Excluir</button>
                <button class="btn btn-primary" ng-click="abrirModalEdit(sms)">Editar</button>
              </div>
            </td>
          </tr>
          <tr ng-if="!data || data.length===0">
            <td colspan="5" class="text-muted">Nenhum registro encontrado.</td>
          </tr>
          </tbody>
        </table>
      </div>
    </div>

  </div> <!-- /container -->

  <!-- ===================== MODAL: INSERIR REGISTRO ===================== -->
  <div id="modalInsert" class="modal fade" tabindex="-1" role="dialog" aria-labelledby="tituloInsert">
    <div class="modal-dialog" role="document">
      <div class="modal-content">

        <form class="form-horizontal" ng-submit="insertJob()">

          <div class="modal-header">
            <button type="button" class="close" ng-click="fecharModal('modalInsert')">&times;</button>
            <h4 class="modal-title" id="tituloInsert">Operação: Inserir registro</h4>
          </div>

          <div class="modal-body">
            <div class="form-group">
              <label class="col-sm-3 control-label">Telefone</label>
              <div class="col-sm-7">
                <input class="form-control" placeholder="Telefone que será enviado" type="text" ng-model="telefone" name="telefone" required>
              </div>
            </div>

            <div class="form-group">
              <label class="col-sm-3 control-label">Mensagem</label>
              <div class="col-sm-9">
                <input class="form-control" placeholder="Mensagem a enviar" type="text" ng-model="mensagem" name="mensagem" required>
              </div>
            </div>

            <!-- Agora envia 0/1/2 -->
            <div class="form-group">
              <label class="col-sm-3 control-label">Status</label>
              <div class="col-sm-4">
                <select class="form-control" ng-model="status" ng-init="status=0">
                  <option value="0">Pendente</option>
                  <option value="1">Enviado</option>
                  <option value="2">Falhou</option>
                </select>
              </div>
            </div>
          </div> <!-- /modal-body -->

          <div class="modal-footer">
            <button type="button" class="btn btn-default" ng-click="fecharModal('modalInsert')">Cancelar</button>
            <button type="submit" class="btn btn-primary">Gravar</button>
          </div>

        </form>

      </div>
    </div>
  </div>

  <!-- ===================== MODAL: EDIÇÃO ===================== -->
  <div id="modalEdit" class="modal fade" tabindex="-1" role="dialog" aria-labelledby="tituloEdit">
    <div class="modal-dialog" role="document">
      <div class="modal-content">

        <form class="form-horizontal" ng-submit="updateJob(edidjob, edtelefone, edmensagem)">

          <div class="modal-header">
            <button type="button" class="close" ng-click="fecharModal('modalEdit')">&times;</button>
            <h4 class="modal-title" id="tituloEdit">Operação: Edição</h4>
          </div>

          <div class="modal-body">
            <div class="form-group">
              <label class="col-sm-3 control-label">Id</label>
              <div class="col-sm-3">
                <p class="form-control-static">{{edidjob}}</p>
              </div>
            </div>

            <div class="form-group">
              <label class="col-sm-3 control-label">Telefone</label>
              <div class="col-sm-7">
                <input class="form-control" type="text" ng-model="edtelefone" name="edtelefone" required>
              </div>
            </div>

            <div class="form-group">
              <label class="col-sm-3 control-label">Mensagem</label>
              <div class="col-sm-9">
                <input class="form-control" type="text" ng-model="edmensagem" name="edmensagem" required>
              </div>
            </div>
            <!-- Se quiser editar status também, adicione aqui outro select e ajuste o WS uJobSMS.php -->
          </div> <!-- /modal-body -->

          <div class="modal-footer">
            <button type="button" class="btn btn-default" ng-click="fecharModal('modalEdit')">Cancelar</button>
            <button type="submit" class="btn btn-primary">Atualizar</button>
          </div>

        </form>

      </div>
    </div>
  </div>
  <?php include('botton.php'); ?>

  <!-- Controller -->
  <script>
    (function() {
      'use strict';

      var app = angular.module('Envia_SMS', []);

      app.controller('cntrl', function($scope, $http, $window, $httpParamSerializerJQLike) {

        // Navegação do menu
        $scope.irMenu = function() { $window.location.href = 'index.html'; };
        $scope.irEnvio = function() { $window.location.href = 'Envia_SMS.html'; };
        $scope.irRelatorios = function() { $window.location.href = 'Relatorios.php'; };
        $scope.emConstrucao = function() { alert('Módulo em desenvolvimento 💻'); };

        // Mensagens
        $scope.msg = '';
        $scope.msgType = 'info';
        function setMsg(text, type) {
          $scope.msg = text || '';
          $scope.msgType = type || 'info';
        }

        // Helper POST (x-www-form-urlencoded)
        function postForm(url, dataObj) {
          return $http({
            method: 'POST',
            url: url,
            data: $httpParamSerializerJQLike(dataObj || {}),
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
          });
        }

        // ================== MODAIS ==================
        $scope.abrirModalInsert = function() {
          $scope.telefone = '';
          $scope.mensagem = '';
          $scope.status = 0; // numérico
          $('#modalInsert').modal('show');
        };

        $scope.abrirModalEdit = function(sms) {
          if (!sms) return;
          $scope.edidjob = sms.idjob;
          $scope.edtelefone = sms.telefone;
          $scope.edmensagem = sms.mensagem;
          $('#modalEdit').modal('show');
        };

        $scope.fecharModal = function(modalId) {
          $('#' + modalId).modal('hide');
        };
        // ============================================

        // INSERT
        $scope.insertJob = function() {
          if (!$scope.telefone || !$scope.mensagem) {
            setMsg('Informe telefone e mensagem.', 'error');
            return;
          }
          var payload = {
            telefone: ($scope.telefone || '').trim(),
            mensagem: ($scope.mensagem || '').trim(),
            status: parseInt($scope.status, 10) || 0 // 0/1/2
          };

          postForm('/ws/iJobSMS.php', payload)
            .then(function(resp) {
              setMsg('SMS programado com sucesso.', 'success');
              $scope.fecharModal('modalInsert');
              $scope.displayJob($scope.ptelefone);
            }, function() {
              setMsg('Falha ao inserir registro.', 'error');
            });
        };

        // SELECT
        $scope.displayJob = function(pTelefone) {
          var params = { telefone: pTelefone || '' };
          $http.get('/ws/sJobSMS.php', { params: params })
            .then(function(resp) {
              $scope.data = resp.data;
              setMsg('Lista atualizada.', 'info');
            }, function() {
              $scope.data = [];
              setMsg('Pesquisa retornou vazia.', 'error');
            });
        };

        // DELETE
        $scope.deleteJob = function(idjob) {
          if (!idjob) return;
          if (!confirm('Confirma a exclusão do registro ' + idjob + '?')) return;

          postForm('/ws/dJobSMS.php', { id: idjob })
            .then(function() {
              setMsg('Registro excluído!', 'success');
              $scope.displayJob($scope.ptelefone);
            }, function() {
              setMsg('Falha ao excluir registro.', 'error');
            });
        };

        // UPDATE
        $scope.updateJob = function(edidjob, edtelefone, edmensagem) {
          if (!edidjob || !edtelefone || !edmensagem) {
            setMsg('Id, telefone e mensagem são obrigatórios.', 'error');
            return;
          }
          var payload = {
            idjob: edidjob,
            telefone: (edtelefone || '').trim(),
            mensagem: (edmensagem || '').trim()
          };

          postForm('/ws/uJobSMS.php', payload)
            .then(function() {
              setMsg('Registro atualizado!', 'success');
              $scope.fecharModal('modalEdit');
              $scope.displayJob($scope.ptelefone);
            }, function() {
              setMsg('Falha ao atualizar registro.', 'error');
            });
        };

      });

      // ❌ Não declarar factory/serviço com o mesmo nome de $httpParamSerializerJQLike
    })();
  </script>
</body>
</html>
