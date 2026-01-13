/** Servidor de Envio de SMS (TEXT MODE)
** Criado por Marcelo Maurin Martins
**
** Implementações/ajustes:
** - Setup/Loop (Setup inicializa; Loop processa jobs)
** - Testa "AT" simples antes de tudo (com tentativas)
** - Sempre inicializa modem com comandos padrão (fail-fast)
** - TEXT MODE: AT+CMGF=1, charset GSM: AT+CSCS="GSM"
** - Opcional: AT+CSMP=17,167,0,0
** - WaitNetworkRegistered via AT+CREG? (,1 ou ,5)
** - EnsureSMSC robusto (detecta CSCA inválido/UCS2 e tenta setar com ",145")
** - serialport_write_all garante escrita completa
** - Logs diários + hot-reload de cfg
** - Verbose: mostra comando executado e resposta bruta do modem
** - Se falhar Setup, não inicia Loop e mostra motivo
**
** Observação:
** Se o SIM/operadora não permitir SMS (dados-only/M2M), você pode ver +CMS ERROR 302/500.
**/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <mysql/mysql.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>

/* ===== Timings ===== */
#define AT_GAP_MS             120
#define AT_AFTER_CLEAR_MS      50
#define CMGS_BEFORE_READ_MS   250
#define SMS_TEXT_GAP_MS       120
#define AFTER_CTRLZ_MS        150

/* ===== Modem constants ===== */
#define CTRL_Z 26

/* ===== Buffers / timeouts ===== */
#define READ_LINE_BUF       1024
#define READ_CMD_BUF        8192
#define PROMPT_TIMEOUT_MS  15000
#define OK_TIMEOUT_MS      20000
#define SHORT_TIMEOUT_MS    4000
#define NETREG_TIMEOUT_MS  30000
#define NETREG_POLL_MS       1000

/* ===== Paths ===== */
#define CFG_DIR  "/etc/SMS"
#define CFG_PATH "/etc/SMS/sms.cfg"
#define LOG_DIR  "/var/log/SMS"

/* ===== Limits ===== */
#define MAX_SMS_CHARS 1000

typedef struct {
  int inicio_min;   // minutos desde 00:00
  int fim_min;      // minutos desde 00:00
  int dias[7];      // 0=domingo ... 6=sábado
  char localdb[80];
  char userdb[80];
  char passdb[80];
  char aliasdb[80];
  char smsc[64];    // +551199...
  char modo;        // 'S' ou 'A'
} SMSConfig;

/* ===== Globais ===== */
static MYSQL mycon;
static SMSConfig g_cfg;

static int  fd = -1;
static char g_tmpbuf[READ_CMD_BUF];

static char localdb[80]  = "";
static char userdb[80]   = "";
static char passdb[80]   = "";
static char aliasdb[80]  = "";
static char smsc_cfg[64] = "";

static char SQLCMD[512];

static FILE *g_log_fp = NULL;
static int   g_log_yyyy = -1, g_log_mm = -1, g_log_dd = -1;
static int   g_mode_daemon = 0;
static time_t g_cfg_mtime = 0;
static char   g_cli_modo  = 0;

/* Porta serial */
static char device[64] = "/dev/ttyUSB0";

/* ===== Utils ===== */
static void msleep(int ms) {
  struct timespec ts;
  ts.tv_sec  = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}
static char *ltrim(char *s) { while (*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++; return s; }
static void rtrim_inplace(char *s) {
  int n = (int)strlen(s);
  while (n>0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n')) { s[n-1]='\0'; n--; }
}
static int parse_hhmm(const char *txt) {
  int h = 0, m = 0;
  if (sscanf(txt, "%d:%d", &h, &m) != 2) return -1;
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}
static int dir_exists(const char *path) { struct stat st; return (stat(path,&st)==0 && S_ISDIR(st.st_mode)); }
static int file_exists(const char *path){ struct stat st; return (stat(path,&st)==0); }
static time_t file_mtime(const char *path) { struct stat st; return (stat(path,&st)==0)?st.st_mtime:0; }

/* ===== Logging (dup console + file) ===== */
static int vdual_print(FILE *stream, const char *fmt, va_list ap) {
  int ret1 = 0, ret2 = 0;
  va_list ap1; va_copy(ap1, ap);
  ret1 = vfprintf(stream, fmt, ap1);
  va_end(ap1);

  if (g_log_fp) {
    int stream_fd = fileno(stream);
    int log_fd    = fileno(g_log_fp);
    if (stream_fd != log_fd) {
      va_list ap2; va_copy(ap2, ap);
      ret2 = vfprintf(g_log_fp, fmt, ap2);
      va_end(ap2);
      fflush(g_log_fp);
    }
  }
  fflush(stream);
  return (ret2!=0) ? ret2 : ret1;
}
static int my_printf(const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); int r = vdual_print(stdout, fmt, ap); va_end(ap); return r;
}
static int my_fprintf(FILE *stream, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); int r = vdual_print(stream, fmt, ap); va_end(ap); return r;
}
#define printf(...)  my_printf(__VA_ARGS__)
#define fprintf(...) my_fprintf(__VA_ARGS__)

/* ===== Protos ===== */
static int serialport_init(const char* serialport, int baud);
static int serialport_clear(int fd);
static int serialport_write_all(int fd, const char* str);
static int serialport_write_raw_all(int fd, const uint8_t* data, size_t len);
static int serialport_read_until(int fd, char* out, char until, int timeout_ms);
static int serialport_read_cmd(int fd, char* out, int timeout_ms);
static int serialport_read_token(int fd, char *out, size_t outsz, int timeout_ms);

static int write_default_config(void);
static void set_cfg_defaults(SMSConfig *cfg);
static int ensure_config(SMSConfig *cfg);
static int load_config(SMSConfig *cfg);
static void apply_db_from_cfg(const SMSConfig *cfg);
static int now_in_window(const SMSConfig *cfg);

static int  dir_ensure(const char *path, mode_t mode);
static void open_log_for_tm(const struct tm *tm, int redirect_streams);
static void init_daily_log(void);
static void maybe_rotate_daily_log(void);
static void reapply_logging_mode(void);
static int  reload_cfg_if_changed(void);

static int Mysqlcon(void);
static bool ExecQuery(const char *SQL);
static void Closecon(void);

static const char* cms_reason_pt(int code);
static const char* cms_hint_pt(int code);
static int extract_cms_from_resp(const char *resp, int *code);

static int send_at_expect_ok_verbose(const char *cmd, int timeout_ms, const char *etapa);

static int WaitNetworkRegistered(int timeout_ms);
static int InicializaModem(void);
static int EnsureSMSC(void);
static int normalize_br_e164(const char *in, char *out, size_t outsz);
static int EnviaSMS(int job_id_param, const char *telefone_e164, const char *texto);
static int BuscaJobs(void);

static void Wellcome(void);

static int Setup(int baud);
static void Loop(void);

/* ===== Config ===== */
static int write_default_config(void) {
  if (!dir_exists(CFG_DIR)) {
    if (mkdir(CFG_DIR, 0755) != 0 && errno != EEXIST) { perror("mkdir /etc/SMS"); return -1; }
  }
  FILE *f = fopen(CFG_PATH, "w");
  if (!f) { perror("fopen write cfg"); return -1; }
  fprintf(f,
    "INICIO: 07:00\n"
    "FIM: 20:00\n"
    "SEGUNDA: 1\n"
    "TERCA: 1\n"
    "QUARTA: 1\n"
    "QUINTA: 1\n"
    "SEXTA: 1\n"
    "SABADO: 1\n"
    "DOMINGO: 1\n"
    "MODO: A\n"
    "LOCALDB: \n"
    "USERDB: \n"
    "PASSDB: \n"
    "ALIASDB: \n"
    "SMSC: \n"
  );
  fclose(f);
  printf("Config padrão criado em %s\n", CFG_PATH);
  return 0;
}
static void set_cfg_defaults(SMSConfig *cfg) {
  cfg->inicio_min = 7*60; cfg->fim_min = 20*60;
  for (int i=0;i<7;i++) cfg->dias[i]=1;
  cfg->localdb[0]=cfg->userdb[0]=cfg->passdb[0]=cfg->aliasdb[0]='\0';
  cfg->smsc[0]='\0';
  cfg->modo = 'A';
}
static int load_config(SMSConfig *cfg) {
  set_cfg_defaults(cfg);
  FILE *f = fopen(CFG_PATH, "r");
  if (!f) { perror("fopen read cfg"); return -1; }
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    rtrim_inplace(line);
    char *p = strchr(line, ':'); if (!p) continue;
    *p = '\0'; char *key = ltrim(line); char *val = ltrim(p+1);
    if (strcasecmp(key,"INICIO")==0) { int m=parse_hhmm(val); if(m>=0) cfg->inicio_min=m; }
    else if (strcasecmp(key,"FIM")==0){ int m=parse_hhmm(val); if(m>=0) cfg->fim_min=m; }
    else if (strcasecmp(key,"SEGUNDA")==0){ cfg->dias[1]=atoi(val)?1:0; }
    else if (strcasecmp(key,"TERCA")==0 || strcasecmp(key,"TERÇA")==0){ cfg->dias[2]=atoi(val)?1:0; }
    else if (strcasecmp(key,"QUARTA")==0){ cfg->dias[3]=atoi(val)?1:0; }
    else if (strcasecmp(key,"QUINTA")==0){ cfg->dias[4]=atoi(val)?1:0; }
    else if (strcasecmp(key,"SEXTA")==0){ cfg->dias[5]=atoi(val)?1:0; }
    else if (strcasecmp(key,"SABADO")==0 || strcasecmp(key,"SÁBADO")==0){ cfg->dias[6]=atoi(val)?1:0; }
    else if (strcasecmp(key,"DOMINGO")==0){ cfg->dias[0]=atoi(val)?1:0; }
    else if (strcasecmp(key,"MODO")==0){ cfg->modo = (val && (*val=='S'||*val=='s')) ? 'S':'A'; }
    else if (strcasecmp(key,"LOCALDB")==0){ snprintf(cfg->localdb,sizeof(cfg->localdb),"%s",val); }
    else if (strcasecmp(key,"USERDB")==0){ snprintf(cfg->userdb,sizeof(cfg->userdb),"%s",val); }
    else if (strcasecmp(key,"PASSDB")==0){ snprintf(cfg->passdb,sizeof(cfg->passdb),"%s",val); }
    else if (strcasecmp(key,"ALIASDB")==0){ snprintf(cfg->aliasdb,sizeof(cfg->aliasdb),"%s",val); }
    else if (strcasecmp(key,"SMSC")==0){ snprintf(cfg->smsc,sizeof(cfg->smsc),"%s",val); }
  }
  fclose(f);
  if (cfg->fim_min < cfg->inicio_min) { cfg->inicio_min=0; cfg->fim_min=24*60-1; }
  return 0;
}
static int ensure_config(SMSConfig *cfg) {
  if (!file_exists(CFG_PATH)) { if (write_default_config()!=0) return -1; }
  return load_config(cfg);
}
static void apply_db_from_cfg(const SMSConfig *cfg) {
  const char *e_local  = getenv("SMS_LOCALDB");
  const char *e_user   = getenv("SMS_USERDB");
  const char *e_pass   = getenv("SMS_PASSDB");
  const char *e_alias  = getenv("SMS_ALIASDB");
  const char *e_smsc   = getenv("SMS_SMSC");

  snprintf(localdb,sizeof(localdb),"%s",(e_local&&*e_local)?e_local:cfg->localdb);
  snprintf(userdb, sizeof(userdb), "%s",(e_user &&*e_user )?e_user :cfg->userdb);
  snprintf(passdb, sizeof(passdb), "%s",(e_pass &&*e_pass )?e_pass :cfg->passdb);
  snprintf(aliasdb,sizeof(aliasdb),"%s",(e_alias&&*e_alias)?e_alias:cfg->aliasdb);
  snprintf(smsc_cfg,sizeof(smsc_cfg),"%s",(e_smsc&&*e_smsc)?e_smsc:cfg->smsc);
}
static int now_in_window(const SMSConfig *cfg) {
  time_t t=time(NULL); struct tm lt; localtime_r(&t,&lt);
  int wday=lt.tm_wday; if (wday<0||wday>6) return 0;
  if (cfg->dias[wday]==0) return 0;
  int nowm=lt.tm_hour*60+lt.tm_min;
  if (nowm<cfg->inicio_min) return 0;
  if (nowm>cfg->fim_min) return 0;
  return 1;
}

/* ===== Logging ===== */
static int dir_ensure(const char *path, mode_t mode) {
  if (!dir_exists(path)) { if (mkdir(path,mode)!=0 && errno!=EEXIST) return -1; }
  return 0;
}
static void open_log_for_tm(const struct tm *tm, int redirect_streams) {
  char path[256];
  if (dir_ensure(LOG_DIR,0755)!=0) perror("mkdir /var/log/SMS");
  strftime(path,sizeof(path), LOG_DIR"/%Y-%m-%d.log", tm);
  FILE *f=fopen(path,"a");
  if (f) {
    setvbuf(f,NULL,_IOLBF,0);
    g_log_yyyy=tm->tm_year+1900; g_log_mm=tm->tm_mon+1; g_log_dd=tm->tm_mday;
    g_log_fp=f;
    if (redirect_streams) {
      dup2(fileno(f), STDOUT_FILENO);
      dup2(fileno(f), STDERR_FILENO);
      printf("=== srvSMS iniciado em %s (modo S) ===\n", path);
    } else {
      fprintf(stdout,"=== srvSMS log diário em %s (modo A) ===\n", path);
      fflush(stdout);
    }
  } else {
    perror("Falha ao abrir log diário");
  }
}
static void init_daily_log(void) {
  time_t t=time(NULL); struct tm tm_now; localtime_r(&t,&tm_now);
  int redirect = g_mode_daemon ? 1 : 0;
  open_log_for_tm(&tm_now, redirect);
}
static void maybe_rotate_daily_log(void) {
  time_t t=time(NULL); struct tm tm_now; localtime_r(&t,&tm_now);
  int y=tm_now.tm_year+1900, m=tm_now.tm_mon+1, d=tm_now.tm_mday;
  if (y!=g_log_yyyy || m!=g_log_mm || d!=g_log_dd) {
    if (g_log_fp) fflush(g_log_fp);
    int redirect = g_mode_daemon ? 1 : 0;
    open_log_for_tm(&tm_now, redirect);
  }
}
static void reapply_logging_mode(void) {
  time_t t=time(NULL); struct tm tm_now; localtime_r(&t,&tm_now);
  int redirect = g_mode_daemon ? 1 : 0;
  open_log_for_tm(&tm_now, redirect);
}
static int reload_cfg_if_changed(void) {
  time_t mt=file_mtime(CFG_PATH);
  if (mt!=0 && mt!=g_cfg_mtime) {
    SMSConfig newcfg;
    if (load_config(&newcfg)==0) {
      int old_daemon=g_mode_daemon;
      g_cfg=newcfg; g_cfg_mtime=mt;
      apply_db_from_cfg(&g_cfg);

      char env_force=0; const char *f=getenv("SMS_FORCE_MODO");
      if (f && (*f=='S'||*f=='s'||*f=='A'||*f=='a')) env_force = (*f=='s'||*f=='S') ? 'S':'A';
      char efetivo = g_cli_modo ? g_cli_modo : (env_force ? env_force : g_cfg.modo);
      g_mode_daemon = (efetivo=='S');

      if (g_mode_daemon!=old_daemon) {
        reapply_logging_mode();
        printf("Config recarregada (MODO=%c) e log reconfigurado.\n", efetivo);
      } else {
        printf("Config recarregada (MODO=%c) sem mudança de modo.\n", efetivo);
      }
      return 1;
    }
  }
  return 0;
}

/* ===== Serial ===== */
static int serialport_clear(int fd) { return tcflush(fd, TCIOFLUSH); }

static int serialport_write_all(int fd, const char* str) {
  size_t len=strlen(str);
  size_t off=0;
  while (off < len) {
    ssize_t n = write(fd, str + off, len - off);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    off += (size_t)n;
  }
  return (int)len;
}
static int serialport_write_raw_all(int fd, const uint8_t* data, size_t len) {
  size_t off=0;
  while (off < len) {
    ssize_t n = write(fd, data + off, len - off);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    off += (size_t)n;
  }
  return (int)len;
}

static int serialport_read_until(int fd, char* out, char until, int timeout_ms) {
  int i = 0; out[0] = '\0';
  struct timeval start, now; gettimeofday(&start, NULL);

  while (1) {
    char b; int n = read(fd, &b, 1);
    if (n == 1) {
      if (i >= READ_LINE_BUF - 2) {
        out[i] = '\0';
        printf("[REC] read_until: linha atingiu limite (%d)\n", i);
        return i;
      }
      out[i++] = b;
      if (b == until) break;
    } else if (n == 0 || (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
      msleep(10);
    } else if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }

    gettimeofday(&now, NULL);
    int elapsed = (int)((now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000);
    if (elapsed > timeout_ms) break;
  }
  out[i] = '\0';
  return i;
}

static int serialport_read_cmd(int fd, char* out, int timeout_ms) {
  size_t out_len = 0;
  out[0] = '\0';

  char line[READ_LINE_BUF + 2];
  struct timeval start, now; gettimeofday(&start, NULL);

  while (1) {
    int n = serialport_read_until(fd, line, '\n', 1000);
    if (n > 0) {
      if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
      line[n] = '\0';

      size_t free_cap = (READ_CMD_BUF - 1) - out_len;
      size_t to_copy  = ((size_t)n < free_cap) ? (size_t)n : free_cap;
      if (to_copy > 0) {
        memcpy(out + out_len, line, to_copy);
        out_len += to_copy;
        out[out_len] = '\0';
      } else {
        return 0;
      }

      if (strstr(out, "OK") || strstr(out, "ERROR") || strstr(out, ">")) {
        return 0;
      }
      if (out_len > (READ_CMD_BUF - 64)) {
        return 0;
      }
    }

    gettimeofday(&now, NULL);
    long elapsed_ms =
      (long)(now.tv_sec - start.tv_sec) * 1000L +
      (long)(now.tv_usec - start.tv_usec) / 1000L;

    if (elapsed_ms >= (long)timeout_ms) return -2;
    msleep(10);
  }
}

static int serialport_read_token(int fd, char *out, size_t outsz, int timeout_ms) {
  size_t out_len = 0;
  if (!out || outsz == 0) return -1;
  out[0] = '\0';

  struct timeval start, now;
  gettimeofday(&start, NULL);

  while (1) {
    char b;
    int n = read(fd, &b, 1);

    if (n == 1) {
      if (out_len < outsz - 1) {
        out[out_len++] = b;
        out[out_len] = '\0';
      }

      if (strchr(out, '>') != NULL) return 0;
      if (strstr(out, "ERROR") != NULL) return 0;
      if (strstr(out, "+CMS ERROR:") != NULL) return 0;
      if (strstr(out, "+ACMS ERROR:") != NULL) return 0;

    } else if (n == 0 || (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
      msleep(10);
    } else if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }

    gettimeofday(&now, NULL);
    int elapsed = (int)((now.tv_sec - start.tv_sec) * 1000 +
                        (now.tv_usec - start.tv_usec) / 1000);
    if (elapsed >= timeout_ms) return -2;
  }
}

/* ===== Serial init ===== */
static int serialport_init(const char* serialport, int baud) {
  struct termios toptions;
  int fd_local=open(serialport, O_RDWR|O_NOCTTY|O_NONBLOCK);
  if (fd_local<0) { perror("open serial"); return -1; }
  if (tcgetattr(fd_local,&toptions)<0) { perror("tcgetattr"); close(fd_local); return -1; }

  speed_t brate=B9600;
  switch (baud) {
    case 4800: brate=B4800; break; case 9600: brate=B9600; break;
    case 19200: brate=B19200; break; case 38400: brate=B38400; break;
    case 57600: brate=B57600; break; case 115200: brate=B115200; break;
  }
  cfsetispeed(&toptions, brate); cfsetospeed(&toptions, brate);

  toptions.c_cflag &= ~PARENB;
  toptions.c_cflag &= ~CSTOPB;
  toptions.c_cflag &= ~CSIZE;
  toptions.c_cflag |= CS8;

  toptions.c_cflag &= ~CRTSCTS;
  toptions.c_cflag |= CREAD | CLOCAL;

  toptions.c_iflag &= ~(IXON|IXOFF|IXANY);
  toptions.c_lflag &= ~(ICANON|ECHO|ECHOE|ISIG);
  toptions.c_oflag &= ~OPOST;

  toptions.c_cc[VMIN]=0;
  toptions.c_cc[VTIME]=10;

  if (tcsetattr(fd_local,TCSANOW,&toptions)<0) { perror("tcsetattr"); close(fd_local); return -1; }

  int flags=fcntl(fd_local,F_GETFL,0);
  fcntl(fd_local,F_SETFL, flags & ~O_NONBLOCK);

  int mctl; ioctl(fd_local,TIOCMGET,&mctl);
  mctl |= TIOCM_DTR|TIOCM_RTS;
  ioctl(fd_local,TIOCMSET,&mctl);

  return fd_local;
}

/* ===== MySQL ===== */
static int Mysqlcon(void) {
  mysql_init(&mycon);
  if (mysql_real_connect(&mycon, localdb, userdb, passdb, aliasdb, 0, NULL, 0) == NULL) {
    fprintf(stderr, "Erro ao conectar no banco: %s\n", mysql_error(&mycon));
    return -1;
  }
  printf("Conectou no banco.\n");
  return 0;
}
static bool ExecQuery(const char *SQL) {
  if (mysql_query(&mycon, SQL)!=0) {
    fprintf(stderr, "Erro ao Executar SQL: %s | %s\n", SQL, mysql_error(&mycon));
    return false;
  }
  return true;
}
static void Closecon(void) { mysql_close(&mycon); }

/* ===== GSM helpers (PT-BR) ===== */
static const char* cms_reason_pt(int code) {
  switch (code) {
    case 300: return "Falha no aparelho (ME)";
    case 301: return "Serviço de SMS do aparelho reservado";
    case 302: return "Operação não permitida";
    case 303: return "Operação não suportada";
    case 304: return "Parâmetro inválido no modo PDU";
    case 305: return "Parâmetro inválido no modo texto";
    case 310: return "SIM não inserido";
    case 311: return "PIN do SIM exigido";
    case 312: return "PUK do SIM exigido";
    case 313: return "Falha no SIM";
    case 314: return "SIM ocupado";
    case 315: return "SIM incorreto";
    case 316: return "PUK2 exigido";
    case 317: return "PIN2 exigido";
    case 320: return "Falha de memória";
    case 321: return "Índice de memória inválido";
    case 322: return "Memória cheia";
    case 330: return "Endereço do SMSC desconhecido";
    case 331: return "Sem serviço de rede";
    case 332: return "Tempo de rede esgotado";
    case 500: return "Erro desconhecido";
    default:  return "Erro não especificado";
  }
}
static const char* cms_hint_pt(int code) {
  switch (code) {
    case 302: return "Operadora/linha pode bloquear SMS. Verifique plano/crédito/M2M.";
    case 330: return "Configure SMSC (AT+CSCA=\"+55...\",145).";
    case 331: return "Sem rede: verifique sinal/antena, tente 2G/3G.";
    case 332: return "Rede lenta: tente novamente, confirme registro (CREG).";
    case 500: return "Erro genérico: confira registro, SMSC, bloqueio do SIM.";
    default:  return "Verifique CPIN/CREG/CSQ e configurações do modem.";
  }
}
static int extract_cms_from_resp(const char *resp, int *code) {
  if (!resp) return 0;
  const char *p = strstr(resp, "+CMS ERROR:");
  if (!p) p = strstr(resp, "+ACMS ERROR:");
  if (!p) return 0;
  p = strchr(p, ':');
  if (!p) return 0;
  p++;
  while (*p==' ' || *p=='\t') p++;
  int n = atoi(p);
  if (code) *code = n;
  return 1;
}

/* ===== AT send (verbose) ===== */
static int send_at_expect_ok_verbose(const char *cmd, int timeout_ms, const char *etapa) {
  char resp[READ_CMD_BUF];
  resp[0] = '\0';

  printf("\n[Setup][%s] >>> %s", etapa, cmd);
  fflush(stdout);

  serialport_clear(fd);
  msleep(AT_AFTER_CLEAR_MS);

  if (serialport_write_all(fd, cmd) < 0) {
    fprintf(stderr, "[Setup][%s] ERRO escrevendo na serial (errno=%d: %s)\n",
            etapa, errno, strerror(errno));
    return -1;
  }

  msleep(AT_GAP_MS);

  int rr = serialport_read_cmd(fd, resp, timeout_ms);
  if (rr < 0) {
    fprintf(stderr, "[Setup][%s] TIMEOUT/ERRO lendo resposta (rr=%d). Cmd=%s\n",
            etapa, rr, cmd);
    return -1;
  }

  printf("[Setup][%s] <<< RESPOSTA DO MODEM (bruta):\n%s\n", etapa, resp);

  if (strstr(resp, "OK")) {
    printf("[Setup][%s] OK\n", etapa);
    return 0;
  }

  int code = -1;
  if (extract_cms_from_resp(resp, &code)) {
    fprintf(stderr,
            "[Setup][%s] FALHA: +CMS ERROR: %d (%s)\n"
            "[Setup][%s] Dica: %s\n",
            etapa, code, cms_reason_pt(code),
            etapa, cms_hint_pt(code));
  } else if (strstr(resp, "ERROR")) {
    fprintf(stderr, "[Setup][%s] FALHA: Modem respondeu ERROR (sem +CMS). Veja dump acima.\n", etapa);
  } else {
    fprintf(stderr, "[Setup][%s] FALHA: Resposta inesperada (sem OK/ERROR). Veja dump acima.\n", etapa);
  }
  return -1;
}

/* ===== Network registration ===== */
static int WaitNetworkRegistered(int timeout_ms) {
  int elapsed = 0;
  char resp[READ_CMD_BUF];

  while (elapsed <= timeout_ms) {
    if (send_at_expect_ok_verbose("AT+CREG?\r", SHORT_TIMEOUT_MS, "CREG?") != 0) {
      fprintf(stderr, "[NET] Falha consultando CREG.\n");
      return -1;
    }

    /* A resposta bruta já foi mostrada; vamos reler rapidinho para checar */
    /* (para evitar depender do print anterior, usamos resp com novo comando) */
    serialport_clear(fd);
    serialport_write_all(fd, "AT+CREG?\r");
    serialport_read_cmd(fd, resp, SHORT_TIMEOUT_MS);

    if (strstr(resp, ",1") || strstr(resp, ",5")) {
      printf("[NET] Registrado na rede (CREG = 1 ou 5)\n");
      return 0;
    }

    printf("[NET] Aguardando registro na rede... (%dms)\n", elapsed);
    msleep(NETREG_POLL_MS);
    elapsed += NETREG_POLL_MS;
  }

  fprintf(stderr, "[NET] Timeout: não registrou na rede em %dms\n", timeout_ms);
  return -1;
}

/* ===== Inicialização Modem ===== */
static int InicializaModem(void) {
  printf("\n[Setup] Iniciando modem: teste AT simples (até 3 tentativas)\n");

  int ok = -1;
  for (int i=1;i<=3;i++) {
    char etapa[32];
    snprintf(etapa, sizeof(etapa), "AT try %d/3", i);
    if (send_at_expect_ok_verbose("AT\r", SHORT_TIMEOUT_MS, etapa) == 0) { ok = 0; break; }
    msleep(300);
  }
  if (ok != 0) {
    fprintf(stderr, "[Setup] Falha: modem não respondeu OK ao AT.\n");
    return -1;
  }

  printf("\n[Setup] Executando sequência padrão de inicialização (fail-fast)\n");

  if (send_at_expect_ok_verbose("ATE0\r",            SHORT_TIMEOUT_MS, "ATE0") != 0) return -1;
  if (send_at_expect_ok_verbose("AT+CMEE=2\r",       SHORT_TIMEOUT_MS, "CMEE=2") != 0) return -1;

  if (send_at_expect_ok_verbose("AT+CMGF=1\r",       SHORT_TIMEOUT_MS, "CMGF=1 (TEXT)") != 0) return -1;
  if (send_at_expect_ok_verbose("AT+CSCS=\"GSM\"\r", SHORT_TIMEOUT_MS, "CSCS=\"GSM\"") != 0) return -1;

  /* Opcional recomendado */
  (void)send_at_expect_ok_verbose("AT+CSMP=17,167,0,0\r", SHORT_TIMEOUT_MS, "CSMP (opcional)");

  if (send_at_expect_ok_verbose("ATI\r",             SHORT_TIMEOUT_MS, "ATI") != 0) return -1;

  /* Espera rede */
  if (WaitNetworkRegistered(NETREG_TIMEOUT_MS) != 0) return -1;

  /* Debug status */
  (void)send_at_expect_ok_verbose("AT+CPIN?\r", SHORT_TIMEOUT_MS, "CPIN?");
  (void)send_at_expect_ok_verbose("AT+CSQ\r",   SHORT_TIMEOUT_MS, "CSQ");
  (void)send_at_expect_ok_verbose("AT+CREG?\r", SHORT_TIMEOUT_MS, "CREG? (final)");

  printf("\n[Setup] ✅ Modem inicializado com sucesso (TEXT MODE pronto)\n");
  return 0;
}

/* ===== Normaliza telefone Brasil E.164 ===== */
static int normalize_br_e164(const char *in, char *out, size_t outsz) {
  char digits[32]; size_t j = 0;
  for (const char *p = in; *p && j < sizeof(digits)-1; ++p) {
    if (*p >= '0' && *p <= '9') digits[j++] = *p;
  }
  digits[j] = '\0';

  if (strlen(digits) == 11) {
    if (snprintf(out, outsz, "+55%s", digits) >= (int)outsz) return -1;
    return 0;
  }
  if (strlen(digits) == 13 && strncmp(digits, "55", 2) == 0) {
    if (snprintf(out, outsz, "+%s", digits) >= (int)outsz) return -1;
    return 0;
  }
  return -1;
}

/* ===== SMSC ===== */
static int is_smsc_text_ok(const char *smsc) {
  if (!smsc || !*smsc) return 0;
  if (smsc[0] != '+') return 0;
  for (const char *p = smsc+1; *p; ++p) {
    if (*p < '0' || *p > '9') return 0;
  }
  return 1;
}

static int EnsureSMSC(void) {
  char resp[READ_CMD_BUF];
  resp[0]='\0';

  printf("\n[Setup] Verificando SMSC (CSCA)\n");

  serialport_clear(fd);
  serialport_write_all(fd, "AT+CSCA?\r");
  if (serialport_read_cmd(fd, resp, SHORT_TIMEOUT_MS) < 0) {
    fprintf(stderr, "[SMSC] Falha lendo CSCA?\n");
    return -1;
  }

  printf("[SMSC] CSCA? resp bruta:\n%s\n", resp);

  char current[96] = {0};
  const char *p = strstr(resp, "+CSCA:");
  if (p) {
    const char *q1 = strchr(p, '\"');
    const char *q2 = q1 ? strchr(q1+1, '\"') : NULL;
    if (q1 && q2 && q2 > q1+1) {
      size_t n = (size_t)(q2 - (q1+1));
      if (n >= sizeof(current)) n = sizeof(current)-1;
      memcpy(current, q1+1, n);
      current[n] = '\0';
    }
  }

  if (is_smsc_text_ok(current)) {
    printf("[SMSC] Já configurado (texto): %s\n", current);
    return 0;
  }

  printf("[SMSC] CSCA atual inválido (possível UCS2/hex ou vazio). Tentando setar.\n");

  if (!*smsc_cfg) {
    fprintf(stderr, "[SMSC] CSCA inválido/ausente e nenhum SMSC fornecido no cfg/ENV.\n");
    return -1;
  }
  if (!is_smsc_text_ok(smsc_cfg)) {
    fprintf(stderr, "[SMSC] SMSC do cfg/ENV inválido (esperado +55...): '%s'\n", smsc_cfg);
    return -1;
  }

  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+CSCA=\"%s\",145\r", smsc_cfg);

  if (send_at_expect_ok_verbose(cmd, SHORT_TIMEOUT_MS, "CSCA(set)") != 0) {
    fprintf(stderr, "[SMSC] Falha ao setar CSCA\n");
    return -1;
  }

  serialport_clear(fd);
  serialport_write_all(fd, "AT+CSCA?\r");
  if (serialport_read_cmd(fd, resp, SHORT_TIMEOUT_MS) >= 0) {
    printf("[SMSC] CSCA? pós-set:\n%s\n", resp);
  }
  return 0;
}

/* ===== Envio SMS (TEXT MODE) ===== */
static int EnviaSMS(int job_id_param, const char *telefone_e164, const char *texto) {
  int msg_len = (int)strlen(texto);
  printf("\n[SMS-%d] Iniciando envio...\n", job_id_param);
  printf("[SMS-%d] Telefone: %s\n", job_id_param, telefone_e164);
  printf("[SMS-%d] Mensagem (len=%d)\n", job_id_param, msg_len);

  if (msg_len > MAX_SMS_CHARS) {
    fprintf(stderr, "[SMS-%d] Mensagem excede %d caracteres, abortando.\n", job_id_param, MAX_SMS_CHARS);
    return -1;
  }

  if (WaitNetworkRegistered(15000) != 0) {
    fprintf(stderr, "[SMS-%d] Sem registro de rede antes do envio.\n", job_id_param);
    return -1;
  }

  if (send_at_expect_ok_verbose("AT+CMGF=1\r",       SHORT_TIMEOUT_MS, "CMGF(TEXT) before send") != 0) return -1;
  if (send_at_expect_ok_verbose("AT+CSCS=\"GSM\"\r", SHORT_TIMEOUT_MS, "CSCS(GSM) before send") != 0) return -1;

  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"\r", telefone_e164);
  printf("[SMS-%d] CMGS: %s\n", job_id_param, cmd);

  serialport_clear(fd);
  if (serialport_write_all(fd, cmd) < 0) {
    fprintf(stderr, "[SMS-%d] Falha escrevendo CMGS\n", job_id_param);
    return -1;
  }

  printf("[SMS-%d] Aguardando prompt '>'...\n", job_id_param);

  char tok[READ_CMD_BUF];
  tok[0] = '\0';

  msleep(CMGS_BEFORE_READ_MS);
  int pr = serialport_read_token(fd, tok, sizeof(tok), PROMPT_TIMEOUT_MS);

  if (pr != 0 || strchr(tok, '>') == NULL) {
    int code=-1;
    fprintf(stderr, "[SMS-%d] Falhou esperando '>' (pr=%d). Resp bruta:\n%s\n",
            job_id_param, pr, tok);

    if (extract_cms_from_resp(tok, &code)) {
      fprintf(stderr, "[SMS-%d] +CMS ERROR: %d (%s)\n", job_id_param, code, cms_reason_pt(code));
      fprintf(stderr, "[SMS-%d] Dica: %s\n", job_id_param, cms_hint_pt(code));
    } else if (strstr(tok, "ERROR")) {
      fprintf(stderr, "[SMS-%d] ERROR sem +CMS (provável rejeição do comando ou estado do modem)\n", job_id_param);
    }
    return -1;
  }

  printf("[SMS-%d] Prompt '>' recebido. Enviando texto + CTRL+Z...\n", job_id_param);

  if (serialport_write_all(fd, texto) < 0) {
    fprintf(stderr, "[SMS-%d] Falha escrevendo texto\n", job_id_param);
    return -1;
  }
  msleep(SMS_TEXT_GAP_MS);
  uint8_t z = CTRL_Z;
  if (serialport_write_raw_all(fd, &z, 1) < 0) {
    fprintf(stderr, "[SMS-%d] Falha enviando CTRL+Z\n", job_id_param);
    return -1;
  }
  msleep(AFTER_CTRLZ_MS);

  char resp[READ_CMD_BUF];
  resp[0] = '\0';
  int r = serialport_read_cmd(fd, resp, OK_TIMEOUT_MS);
  if (r < 0) {
    fprintf(stderr, "[SMS-%d] Timeout aguardando confirmação do envio\n", job_id_param);
    return -1;
  }

  printf("[SMS-%d] Confirmação bruta:\n%s\n", job_id_param, resp);

  if (strstr(resp, "ERROR")) {
    int code = -1;
    if (extract_cms_from_resp(resp, &code)) {
      fprintf(stderr, "[SMS-%d] +CMS ERROR: %d (%s)\n", job_id_param, code, cms_reason_pt(code));
      fprintf(stderr, "[SMS-%d] Dica: %s\n", job_id_param, cms_hint_pt(code));
    } else {
      fprintf(stderr, "[SMS-%d] ERROR sem código explícito.\n", job_id_param);
    }
    return -1;
  }

  if (!strstr(resp, "+CMGS:") || !strstr(resp, "OK")) {
    int code=-1;
    if (extract_cms_from_resp(resp, &code)) {
      fprintf(stderr, "[SMS-%d] +CMS ERROR: %d (%s)\n", job_id_param, code, cms_reason_pt(code));
      fprintf(stderr, "[SMS-%d] Dica: %s\n", job_id_param, cms_hint_pt(code));
    } else {
      fprintf(stderr, "[SMS-%d] Confirmação incompleta (sem +CMGS/OK).\n", job_id_param);
    }
    return -1;
  }

  printf("[SMS-%d] ✅ SMS enviado com sucesso!\n", job_id_param);
  return 0;
}

/* ===== Busca e processamento de jobs ===== */
static int BuscaJobs(void) {
  snprintf(SQLCMD,sizeof(SQLCMD),
           "SELECT idjob, telefone, mensagem, status FROM jobs WHERE status = 0;");
  if (mysql_query(&mycon, SQLCMD)!=0) {
    fprintf(stderr, "Erro ao executar SQL: %s\n", mysql_error(&mycon));
    return -1;
  }
  MYSQL_RES *result = mysql_store_result(&mycon);
  if (!result) { fprintf(stderr,"Resultado nulo em SQL: %s\n", SQLCMD); return -1; }

  MYSQL_ROW row;
  unsigned long *lengths;
  int count = 0;

  while ((row = mysql_fetch_row(result)) != NULL) {
    lengths = mysql_fetch_lengths(result);

    int this_idjob = row[0] ? atoi(row[0]) : 0;

    char job_telefone[64];
    char job_mensagem[MAX_SMS_CHARS + 1];

    snprintf(job_telefone, sizeof(job_telefone), "%s", row[1] ? row[1] : "");
    char e164[32];
    if (normalize_br_e164(job_telefone, e164, sizeof(e164)) != 0) {
      fprintf(stderr, "[JOB %d] Telefone inválido E.164 (in: %s) — status=9\n", this_idjob, job_telefone);
      snprintf(SQLCMD,sizeof(SQLCMD), "UPDATE jobs SET status = 9 WHERE idjob = %d;", this_idjob);
      ExecQuery(SQLCMD);
      msleep(200);
      continue;
    }

    size_t src_len = (row[2] ? lengths[2] : 0);
    size_t max_len = sizeof(job_mensagem) - 1;
    if (src_len > max_len) src_len = max_len;
    if (row[2] && src_len > 0) {
      memcpy(job_mensagem, row[2], src_len);
      job_mensagem[src_len] = '\0';
    } else {
      job_mensagem[0] = '\0';
    }

    for (char *p = job_mensagem; *p; ++p) {
      if (*p == '\r' || *p == '\n') *p = ' ';
    }

    printf("[JOB] id=%d telefone=%s len_msg=%zu\n", this_idjob, e164, strlen(job_mensagem));

    int ret = EnviaSMS(this_idjob, e164, job_mensagem);
    if (ret != 0) {
      fprintf(stderr, "Job %d: 1a tentativa falhou. Tentando novamente...\n", this_idjob);
      serialport_clear(fd);
      msleep(1500);
      ret = EnviaSMS(this_idjob, e164, job_mensagem);
    }

    if (ret == 0) {
      snprintf(SQLCMD,sizeof(SQLCMD), "UPDATE jobs SET status = 1 WHERE idjob = %d;", this_idjob);
      ExecQuery(SQLCMD);
      printf("Job %d: status=1 (sucesso)\n", this_idjob);
    } else {
      snprintf(SQLCMD,sizeof(SQLCMD), "UPDATE jobs SET status = 9 WHERE idjob = %d;", this_idjob);
      ExecQuery(SQLCMD);
      fprintf(stderr, "Job %d: erro após 2 tentativas. status=9.\n", this_idjob);
    }

    count++;
    msleep(200);
  }

  mysql_free_result(result);
  return count;
}

/* ===== UI ===== */
static void Wellcome(void) {
  printf("srvSMS - Servidor de SMS (TEXT MODE)\n");
  printf("Criado por Marcelo Maurin Martins\n");
  printf("Maurinsoft - http://maurinsoft.com.br\n\n");
}

/* ===== Setup/Loop ===== */
static int Setup(int baud) {
  printf("[Setup] Carregando config...\n");
  if (ensure_config(&g_cfg)!=0) {
    fprintf(stderr,"[Setup] Falha ao preparar/carregar %s\n", CFG_PATH);
    return -1;
  }
  g_cfg_mtime = file_mtime(CFG_PATH);

  apply_db_from_cfg(&g_cfg);

  if (!*localdb || !*userdb || !*aliasdb) {
    fprintf(stderr,
      "[Setup] Config de banco incompleta. Defina LOCALDB/USERDB/ALIASDB em %s ou via env SMS_*\n",
      CFG_PATH
    );
    return -1;
  }

  printf("[Setup] Abrindo log diário...\n");
  init_daily_log();
  Wellcome();

  printf("[Setup] Abrindo conexao com Banco de dados...\n");
  if (Mysqlcon()==-1) {
    fprintf(stderr,"[Setup] Nao é possivel conectar ao banco de dados\n");
    return -1;
  }

  ExecQuery("SET NAMES utf8mb4");
  ExecQuery("SET CHARACTER SET utf8mb4");

  printf("[Setup] Inicializando Device %s @ %d...\n", device, baud);
  fd = serialport_init(device, baud);
  if (fd==-1) {
    fprintf(stderr,"[Setup] Erro ao abrir porta serial\n");
    return -1;
  }

  printf("[Setup] Inicializando Modem (TEXT MODE)...\n");
  if (InicializaModem()!=0) {
    fprintf(stderr,"[Setup] ❌ Falha ao iniciar modem. Motivo acima. Loop não será iniciado.\n");
    return -1;
  }

  printf("[Setup] Verificando SMSC...\n");
  if (EnsureSMSC() != 0) {
    fprintf(stderr, "[Setup] Aviso: SMSC não configurado/indisponível; envio pode falhar (+CMS 302/500).\n");
  }

  printf("\n[Setup] ✅ Setup concluído. Entrando no Loop.\n");
  return 0;
}

static void Loop(void) {
  while (1) {
    reload_cfg_if_changed();
    maybe_rotate_daily_log();

    if (!now_in_window(&g_cfg)) { msleep(10000); continue; }

    int n = BuscaJobs();
    if (n <= 0) msleep(1000); else msleep(500);

    serialport_clear(fd);
  }
}

/* ===== MAIN ===== */
int main(int argc, char *argv[]) {
  int opt;
  int baud = 9600;

  while ((opt=getopt(argc,argv,"d:b:m:"))!=-1) {
    switch(opt) {
      case 'd': snprintf(device,sizeof(device), "%s", optarg); break;
      case 'b': baud=atoi(optarg); break;
      case 'm':
        if (optarg && (optarg[0]=='S'||optarg[0]=='s'||optarg[0]=='A'||optarg[0]=='a')) {
          g_cli_modo = (optarg[0]=='S'||optarg[0]=='s') ? 'S' : 'A';
        }
        break;
      default: break;
    }
  }

  /* Carrega cfg cedo só para definir modo e log corretamente */
  if (ensure_config(&g_cfg) != 0) {
    fprintf(stderr,"Falha ao preparar/carregar %s\n", CFG_PATH);
    return EXIT_FAILURE;
  }

  char env_force=0; const char *f=getenv("SMS_FORCE_MODO");
  if (f && (*f=='S'||*f=='s'||*f=='A'||*f=='a')) env_force = (*f=='s'||*f=='S') ? 'S':'A';
  char efetivo = g_cli_modo ? g_cli_modo : (env_force ? env_force : g_cfg.modo);
  g_mode_daemon = (efetivo=='S');

  printf("[MAIN] Iniciando srvSMS (MODO=%c) - device=%s baud=%d\n", efetivo, device, baud);
  if (g_mode_daemon) printf("[MAIN] Rodando silencioso (log em %s)\n", LOG_DIR);

  /* Setup completo (fail-fast) */
  if (Setup(baud) != 0) {
    fprintf(stderr, "[MAIN] Encerrando: Setup falhou (não inicia Loop).\n");
    if (fd >= 0) close(fd);
    Closecon();
    return EXIT_FAILURE;
  }

  /* Loop de atendimento */
  Loop();

  /* nunca chega aqui */
  Closecon();
  if (fd>=0) close(fd);
  return EXIT_SUCCESS;
}
