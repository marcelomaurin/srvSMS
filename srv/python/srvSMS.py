import socket
import threading
import os
import configparser
import serial  # pip install pyserial
import time

CONFIG_FILE = "setup.cfg"
serial_conn = None  # Conexão serial global


# ==========================
# Função para carregar ou criar configuração
# ==========================
def carregar_configuracao():
    config = configparser.ConfigParser()

    if not os.path.exists(CONFIG_FILE):
        print("[SETUP] Arquivo de configuração não encontrado. Criando padrão...")
        config["CONFIG"] = {
            "serial_port": "COM13",
            "baudrate": "9600",
            "tcp_port": "9095"
        }
        with open(CONFIG_FILE, "w") as cfgfile:
            config.write(cfgfile)
        print(f"[SETUP] Arquivo {CONFIG_FILE} criado com valores padrão.")

    config.read(CONFIG_FILE)

    serial_port = config.get("CONFIG", "serial_port")
    baudrate = config.getint("CONFIG", "baudrate")
    tcp_port = config.getint("CONFIG", "tcp_port")

    print(f"[CONFIG] Serial={serial_port}, Baudrate={baudrate}, TCP={tcp_port}")
    return serial_port, baudrate, tcp_port


# ==========================
# Inicialização da porta serial
# ==========================
def inicializar_serial(porta, baud):
    try:
        ser = serial.Serial(porta, baudrate=baud, timeout=1)
        print(f"[SERIAL] Porta {porta} aberta a {baud} bps")
        return ser
    except Exception as e:
        print(f"[ERRO SERIAL] Não foi possível abrir {porta}: {e}")
        return None


# ==========================
# Função para enviar SMS via modem GSM
# ==========================
def enviar_sms(numero: str, mensagem: str):
    """
    Exemplo de envio de SMS via modem GSM usando comandos AT.
    """
    global serial_conn
    print(f"[ENVIAR SMS] Para: {numero} | Mensagem: {mensagem}")

    if serial_conn is None:
        print("[ERRO] Serial não inicializada.")
        return False

    try:
        # Envia comandos AT básicos
        serial_conn.write(b'AT\r')
        time.sleep(0.5)
        serial_conn.write(b'AT+CMGF=1\r')  # modo texto
        time.sleep(0.5)
        serial_conn.write(f'AT+CMGS="{numero}"\r'.encode('ascii'))
        time.sleep(0.5)
        serial_conn.write(mensagem.encode('ascii') + b"\x1A")  # Ctrl+Z para enviar
        time.sleep(2)
        print("[SMS] Comando enviado para modem.")
        return True
    except Exception as e:
        print(f"[ERRO SMS] {e}")
        return False


# ==========================
# Tratamento do cliente TCP
# ==========================
def tratar_cliente(conn, addr):
    print(f"[NOVA CONEXÃO] {addr}")
    buffer = ""

    try:
        while True:
            data = conn.recv(1024)
            if not data:
                break

            buffer += data.decode("ascii", errors="ignore")

            # Enquanto tiver <CR> no buffer, processa
            while "\r" in buffer:
                comando, buffer = buffer.split("\r", 1)
                comando = comando.strip()

                if comando.startswith("SEND="):
                    try:
                        payload = comando[5:]
                        numero, mensagem = payload.split("#", 1)
                        sucesso = enviar_sms(numero, mensagem)
                        resposta = "OK\r\n" if sucesso else "ERRO\r\n"
                    except Exception as e:
                        print(f"[ERRO PARSE] {e}")
                        resposta = "ERRO\r\n"
                    conn.sendall(resposta.encode("ascii"))
                else:
                    conn.sendall(b"COMANDO_INVALIDO\r\n")

    except ConnectionResetError:
        print(f"[DESCONECTADO] {addr}")
    finally:
        conn.close()


# ==========================
# Servidor principal TCP
# ==========================
def iniciar_servidor_tcp(porta_tcp):
    servidor = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    servidor.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    servidor.bind(("0.0.0.0", porta_tcp))
    servidor.listen(5)
    print(f"[SERVIDOR ATIVO] Escutando em 0.0.0.0:{porta_tcp}")

    while True:
        conn, addr = servidor.accept()
        thread = threading.Thread(target=tratar_cliente, args=(conn, addr), daemon=True)
        thread.start()


# ==========================
# Programa principal
# ==========================
if __name__ == "__main__":
    serial_port, baudrate, tcp_port = carregar_configuracao()
    serial_conn = inicializar_serial(serial_port, baudrate)

    try:
        iniciar_servidor_tcp(tcp_port)
    except KeyboardInterrupt:
        print("\n[ENCERRANDO] Servidor finalizado.")
        if serial_conn:
            serial_conn.close()
