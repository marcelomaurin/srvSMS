CREATE TABLE IF NOT EXISTS sms_log (
  idlog       BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  idjob       INT UNSIGNED NULL,
  telefone    VARCHAR(20) NOT NULL,
  mensagem    VARCHAR(500) NOT NULL,
  resultado   ENUM('SUCESSO','ERRO','DESCARTE') NOT NULL,
  codigo      INT NULL,                 -- ex: 302
  resposta    TEXT NULL,                -- resposta bruta do modem
  observacao  VARCHAR(255) NULL,        -- motivo curto (ex: Operação não permitida)
  dthr        TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (idlog),
  KEY idx_job (idjob),
  KEY idx_tel_dthr (telefone, dthr)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
