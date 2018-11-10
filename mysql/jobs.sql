use srvSMSdb;

CREATE TABLE IF NOT EXISTS `jobs` (
  `idjob` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `telefone` varchar(20) NOT NULL,
  `mensagem` varchar(500) NOT NULL,
  status int default 0,
  PRIMARY KEY (`idjob`)
  ) ENGINE=MyISAM DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;
