-- Tabela principal de lotes
CREATE TABLE lote (
    id INT AUTO_INCREMENT PRIMARY KEY,
    descricao VARCHAR(100) NOT NULL,
    observacao TEXT,
    data_lote DATE NOT NULL,
    dtcad TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);