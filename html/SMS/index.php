<!doctype html>
<html lang="pt-br" ng-app="MenuSMS">
<head>
  <meta charset="utf-8">
  <title>Menu Principal - SMS</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <!-- AngularJS -->
  <script src="https://ajax.googleapis.com/ajax/libs/angularjs/1.5.6/angular.min.js"></script>

  <!-- Bootstrap 3 -->
  <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css">
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js"></script>
  <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js"></script>

  <style>
    body {
      background-color: #f8f8f8;
    }
    .explicacao {
      background: #fff;
      border: 1px solid #ddd;
      border-radius: 6px;
      box-shadow: 0 2px 6px rgba(0,0,0,0.1);
      padding: 25px;
      margin: 40px auto;
      max-width: 900px;
    }
    .explicacao h2 {
      margin-top: 0;
      color: #2c3e50;
      font-weight: 600;
    }
    .explicacao p {
      font-size: 16px;
      color: #333;
      line-height: 1.6;
      text-align: justify;
    }
  </style>
</head>
<body ng-controller="MenuController">

  <!-- Menu Superior -->
  <?php include('menu.php'); ?>

  <!-- Área branca central com explicação -->
  <div class="container">
    <div class="explicacao">
      <h2>Projeto SMS - Sistema de Envio de Mensagens</h2>
      <p>
        Este sistema é um <strong>projeto open source</strong> desenvolvido para o 
        <strong>envio automatizado de mensagens SMS</strong> por meio de modem GSM, 
        APIs web e integração com bancos de dados.
      </p>
      <p>
        O objetivo é permitir que organizações públicas e privadas realizem comunicações 
        automatizadas com seus usuários de forma prática, segura e monitorável, 
        com relatórios e histórico de envio.
      </p>
      <p>
        O projeto faz parte da iniciativa <strong>Maurinsoft</strong>, criada para oferecer 
        soluções tecnológicas livres, eficientes e acessíveis, promovendo inovação e automação 
        em ambientes corporativos e públicos.
      </p>
      <hr>
      <p class="text-center">
        Utilize o menu superior para navegar entre os módulos: 
        <strong>Envio de SMS</strong>, <strong>Relatórios</strong> e 
        <strong>Configurações</strong>.
      </p>
    </div>
  </div>

  <?php include('botton.php'); ?>

  <!-- Controller -->
  <script>
    (function(){
      'use strict';
      var app = angular.module('MenuSMS', []);
      app.controller('MenuController', function($scope, $window){
        $scope.irEnvio = function(){ $window.location.href = 'Envia_SMS.html'; };
        $scope.irRelatorios = function(){ $window.location.href = 'Relatorios.php'; };
        $scope.emConstrucao = function(){ alert('Módulo em desenvolvimento 💻'); };
      });
    })();
  </script>

</body>
</html>
