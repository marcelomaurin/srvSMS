
-- Tabela de telefones vinculados a um lote
CREATE TABLE telefone_lote (
    id INT AUTO_INCREMENT PRIMARY KEY,
    id_lote INT NOT NULL,
    numero_telefone VARCHAR(20) NOT NULL,
    nro_hygia INT(8) NOT NULL,
    dtcad TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (id_lote) REFERENCES lote(id)
        ON DELETE CASCADE
        ON UPDATE CASCADE
);