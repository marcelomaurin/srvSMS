/** Servidor de Envio de SMS
** Criado por Marcelo Maurin Martins
** Ajustes:
** - Config /etc/SMS/sms.cfg (auto-criação; sem credenciais hardcoded)
** - Lê credenciais do cfg com override por ENV (SMS_LOCALDB, SMS_USERDB, SMS_PASSDB, SMS_ALIASDB, SMS_SMSC)
** - Valida configuração sem vazar segredos nos logs
** - Janela de envio por dia/horário
** - Inicialização do modem: exige OK em cada comando (fail-fast)
** - Log diário em /var/log/SMS/YYYY-MM-DD.log com rotação à meia-noite
** - MODO: S (daemon silencioso) / A (aplicação verbosa). Ambos mantêm log diário.
** - Hot-reload da configuração (incl. MODO)
** - Buffers e leitura serial robustos (sem overflow)
** - Mensagens até 1000 chars (com '\0' garantido)
** - Telefone normalizado para E.164 Brasil (+55...)
** - EnsureSMSC: garante SMSC (AT+CSCA) via cfg/ENV
** - BuscaJobs: sucesso -> status=1; qualquer erro -> status=9
** - Tratamento de +CMS ERROR: e +ACMS ERROR: (PT-BR)
** - INSERT automático em sms_log (modelo simples)
**/

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

#define CTRL_Z 26
#define READ_LINE_BUF 1024
#define READ_CMD_BUF  8192
#define PROMPT_TIMEOUT_MS 15000
#define OK_TIMEOUT_MS      20000
#define SHORT_TIMEOUT_MS    3000

#define CFG_DIR  "/etc/SMS"
#define CFG_PATH "/etc/SMS/sms.cfg"
#define LOG_DIR  "/var/log/SMS"

/* tamanho máximo aceito pela aplicação/banco para mensagens de envio */
#define MAX_SMS_CHARS 1000
/* tamanho para LOG.mensagem (tabela sms_log usa VARCHAR(500)) */
#define MAX_LOG_MSG 500

/* ===== Config ===== */
typedef struct {
  int inicio_min;   // minutos desde 00:00
  int fim_min;      // minutos desde 00:00
  int dias[7];      // 0=domingo ... 6=sábado
  char localdb[80];
  char userdb[80];
  char passdb[80];
  char aliasdb[80];
  char smsc[64];    // número do SMSC (ex: +551199501234)
  char modo;        // 'S' (daemon) ou 'A' (aplicação)
} SMSConfig;

/* ===== Globais ===== */
MYSQL mycon;
SMSConfig g_cfg;

int fd = -1;
char g_tmpbuf[READ_CMD_BUF];

/* DB sem defaults no fonte (cfg/env apenas) */
static char localdb[80] = "";
static char userdb[80]  = "";
static char passdb[80]  = "";
static char aliasdb[80] = "";
static char smsc_cfg[64] = "";

char SQLCMD[512];

/* JOB */
int  job_id = 0;
char job_telefone[64];
char job_mensagem[MAX_SMS_CHARS + 1];

/* Porta serial */
char device[64] = "/dev/ttyUSB0";

/* Log diário */
static FILE *g_log_fp = NULL;
static int   g_log_yyyy = -1, g_log_mm = -1, g_log_dd = -1;
/* Modo atual: 1=daemon (S), 0=aplicação (A) */
static int   g_mode_daemon = 0;
/* Hot-reload cfg */
static time_t g_cfg_mtime = 0;
/* Override via CLI (-m S|A) */
static char   g_cli_modo  = 0;

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

/* ===== Protos ===== */
/* Serial */
static int serialport_init(const char* serialport, int baud);
static int serialport_write(int fd, const char* str);
static int serialport_write_raw(int fd, const uint8_t* data, size_t len);
static int serialport_read_until(int fd, char* out, char until, int timeout_ms);
static int serialport_read_cmd(int fd, char* out, int timeout_ms);
static int serialport_clear(int fd);

/* GSM */
static int send_at_expect_ok(const char *cmd, int timeout_ms, const char *etapa);
static int InicializaModem(void);
static int EnviaSMS(int job_id, const char *job_telefone, const char *job_mensagem);

/* MySQL */
static int Mysqlcon(void);
static bool ExecQuery(const char *SQLCMD);
static int  BuscaJobs(void);
static void Closecon(void);

/* Config */
static int write_default_config(void);
static void set_cfg_defaults(SMSConfig *cfg);
static int ensure_config(SMSConfig *cfg);
static int load_config(SMSConfig *cfg);
static void apply_db_from_cfg(const SMSConfig *cfg);
static int now_in_window(const SMSConfig *cfg);

/* Log diário + MODO */
static int  dir_ensure(const char *path, mode_t mode);
static void open_log_for_tm(const struct tm *tm, int redirect_streams);
static void init_daily_log(void);
static void maybe_rotate_daily_log(void);

/* Hot-reload MODO/CFG */
static time_t file_mtime(const char *path);
static void reapply_logging_mode(void);
static int reload_cfg_if_changed(void);

/* Outros */
static void Wellcome(void);

/* SMSC + Telefone */
static int EnsureSMSC(void);
static int normalize_br_e164(const char *in, char *out, size_t outsz);

/* ===== Tratamento de erros +CMS / +ACMS ===== */
static const char* cms_reason_pt(int code);
static const char* cms_hint_pt(int code);
static int extract_cms_from_resp(const char *resp, int *code, char *which, size_t which_sz);

/* ===== Logging: wrappers e macros ===== */
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

/* ===== Hot-reload MODO/CFG ===== */
static time_t file_mtime(const char *path) { struct stat st; return (stat(path,&st)==0)?st.st_mtime:0; }
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
static int serialport_write(int fd, const char* str) {
  size_t len=strlen(str); ssize_t n=write(fd,str,len); if(n<0||(size_t)n!=len) return -1; return (int)n;
}
static int serialport_write_raw(int fd, const uint8_t* data, size_t len) {
  ssize_t n=write(fd,data,len); if(n<0||(size_t)n!=len) return -1; return (int)n;
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
      return -1;
    }

    gettimeofday(&now, NULL);
    int elapsed = (int)((now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000);
    if (elapsed > timeout_ms) break;
  }
  out[i] = '\0';
  return i;
}

/* concatenação segura (sem overflow) */
static inline void safe_append(char *dst, size_t dst_cap, const char *src) {
  size_t dst_len = strlen(dst);
  if (dst_len >= dst_cap) return;
  size_t cap = dst_cap - dst_len - 1; /* reserva 1 p/ '\0' */
  if (cap == 0) return;
  strncat(dst, src, cap);
}

/* Lê várias linhas até encontrar OK/ERROR/>, com timeout total */
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

      /* filtra ruídos */
      int descartar = (strstr(line, "+CSQ:") || strstr(line, "+RSSI:")) ? 1 : 0;
      if (!descartar) {
        size_t free_cap = (READ_CMD_BUF - 1) - out_len;
        size_t to_copy  = (n > 0 && (size_t)n < free_cap) ? (size_t)n : free_cap;
        if (to_copy > 0) {
          memcpy(out + out_len, line, to_copy);
          out_len += to_copy;
          out[out_len] = '\0';
        } else {
          return 0;
        }
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

    if (elapsed_ms >= (long)timeout_ms) {
      return -2;
    }

    msleep(10);
  }
}

/* ===== GSM helpers: motivos/dicas (PT-BR) ===== */
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
    case 300: return "Reinicie o modem e verifique alimentação/USB.";
    case 301: return "Ajuste configuração/firmware; tente AT+CMGF=1.";
    case 302: return "Cheque CREG/CSCA/planos e bloqueios de SMS.";
    case 303: return "Use comandos suportados; revise modo texto/PDU.";
    case 304: return "Corrija a PDU.";
    case 305: return "Sanitize a msg/charset; use AT+CSCS=\"GSM\".";
    case 310: return "Insira o SIM corretamente.";
    case 311: return "Envie AT+CPIN=\"<PIN>\".";
    case 312: return "Desbloqueie com PUK junto à operadora.";
    case 313: return "Troque o SIM/teste em outro aparelho.";
    case 314: return "Aguarde e tente novamente.";
    case 315: return "Verifique compatibilidade/PLMN do SIM.";
    case 316: return "Desbloqueie com PUK2.";
    case 317: return "Informe PIN2 válido.";
    case 320: return "Limpe mensagens (AT+CMGD) ou reinicie.";
    case 321: return "Corrija o índice de memória.";
    case 322: return "Apague mensagens (AT+CMGD=1,4).";
    case 330: return "Configure o SMSC (AT+CSCA=\"+55...\").";
    case 331: return "Verifique sinal/antena; tente 2G/3G.";
    case 332: return "Tente novamente; confira cobertura/PLMN.";
    case 500: return "Habilite AT+CMEE=2 e revise os logs.";
    default:  return "Consulte operadora/modem para detalhes.";
  }
}

/* Extrai +CMS ERROR: N ou +ACMS ERROR: N */
static int extract_cms_from_resp(const char *resp, int *code, char *which, size_t which_sz) {
  if (!resp) return 0;
  const char *tags[] = { "+CMS ERROR:", "+ACMS ERROR:" };
  for (int i=0;i<2;i++) {
    const char *p = strstr(resp, tags[i]);
    if (p) {
      p += (int)strlen(tags[i]);
      while (*p==' ' || *p=='\t') p++;
      int n = atoi(p);
      if (code) *code = n;
      if (which && which_sz>0) snprintf(which, which_sz, "%s", (tags[i][1]=='C') ? "+CMS" : "+ACMS");
      return 1;
    }
  }
  return 0;
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

/* ===== Log de Envio (tabela simples sms_log) =====
   CREATE TABLE IF NOT EXISTS sms_log (
     idlog BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
     idjob INT UNSIGNED NULL,
     telefone VARCHAR(20) NOT NULL,
     mensagem VARCHAR(500) NOT NULL,
     resultado ENUM('SUCESSO','ERRO','DESCARTE') NOT NULL,
     codigo INT NULL,
     resposta TEXT NULL,
     observacao VARCHAR(255) NULL,
     dthr TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
   ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
*/
/* INSERT em sms_log (simples e sem helpers extras) */
static void LogEnvioDB(int idjob,                  /* use -1 para NULL */
                       const char *telefone,
                       const char *mensagem,       /* truncada a 500 chars */
                       const char *resultado,      /* 'SUCESSO' | 'ERRO' | 'DESCARTE' */
                       int codigo,                 /* use -1 para NULL */
                       const char *resposta,       /* pode ser NULL */
                       const char *observacao)     /* pode ser NULL */
{
  /* 1) truncagem segura da mensagem para caber no VARCHAR(500) */
  char msg_trunc[501] = {0};
  if (mensagem) {
    size_t n = strlen(mensagem);
    if (n > 500) n = 500;
    memcpy(msg_trunc, mensagem, n);
    msg_trunc[n] = '\0';
  }

  /* 2) escapar strings para MySQL */
  char tel_sql[64], msg_sql[1024], res_sql[32], resp_sql[2048], obs_sql[512];
  mysql_real_escape_string(&mycon, tel_sql, telefone ? telefone : "", (unsigned long)strlen(telefone ? telefone : ""));
  mysql_real_escape_string(&mycon, msg_sql, msg_trunc, (unsigned long)strlen(msg_trunc));
  mysql_real_escape_string(&mycon, res_sql, resultado ? resultado : "ERRO", (unsigned long)strlen(resultado ? resultado : "ERRO"));

  int have_resp = (resposta && *resposta);
  int have_obs  = (observacao && *observacao);
  if (have_resp) mysql_real_escape_string(&mycon, resp_sql, resposta,   (unsigned long)strlen(resposta));
  if (have_obs)  mysql_real_escape_string(&mycon, obs_sql,  observacao, (unsigned long)strlen(observacao));

  /* 3) montar partes condicionais (NULL vs valor) */
  char idstr[32];   snprintf(idstr,   sizeof(idstr),   (idjob  >= 0) ? "%d"  : "NULL", idjob);
  char codestr[32]; snprintf(codestr, sizeof(codestr), (codigo >= 0) ? "%d"  : "NULL", codigo);

  char respstr[2100];
  if (have_resp)  snprintf(respstr, sizeof(respstr), "'%s'", resp_sql);
  else            strcpy(respstr, "NULL");

  char obsstr[600];
  if (have_obs)   snprintf(obsstr, sizeof(obsstr),  "'%s'", obs_sql);
  else            strcpy(obsstr, "NULL");

  /* 4) SQL final — sem buracos */
  char sql[4096];
  snprintf(sql, sizeof(sql),
    "INSERT INTO sms_log (idjob, telefone, mensagem, resultado, codigo, resposta, observacao) "
    "VALUES (%s, '%s', '%s', '%s', %s, %s, %s);",
    idstr, tel_sql, msg_sql, res_sql, codestr, respstr, obsstr
  );

  ExecQuery(sql);
}

/* ===== GSM ===== */
static int send_at_expect_ok(const char *cmd, int timeout_ms, const char *etapa) {
  char tmp[READ_CMD_BUF]; serialport_clear(fd);
  if (serialport_write(fd, cmd)<0) { fprintf(stderr,"[Init][%s] Falha ao escrever: %s\n", etapa, cmd); return -1; }
  if (serialport_read_cmd(fd, tmp, timeout_ms)<0) { fprintf(stderr,"[Init][%s] Timeout aguardando resposta de: %s\n", etapa, cmd); return -1; }
  if (!strstr(tmp,"OK")) {
    int code = -1; char which[8] = "";
    if (extract_cms_from_resp(tmp, &code, which, sizeof(which))) {
      fprintf(stderr,"[Init][%s] %s ERROR: %d (%s). Dica: %s\n",
              etapa, which, code, cms_reason_pt(code), cms_hint_pt(code));
    } else {
      fprintf(stderr,"[Init][%s] Resposta sem OK: %s\n", etapa, tmp);
    }
    return -1;
  }
  printf("[Init][%s] OK\n", etapa); return 0;
}

static int InicializaModem(void) {
  if (send_at_expect_ok("AT\r",       SHORT_TIMEOUT_MS,"AT"  )!=0) return -1;
  if (send_at_expect_ok("ATE0\r",     SHORT_TIMEOUT_MS,"ATE0")!=0) return -1;
  if (send_at_expect_ok("AT+CMEE=2\r",SHORT_TIMEOUT_MS,"CMEE")!=0) return -1;
  if (send_at_expect_ok("AT+CMGF=1\r",SHORT_TIMEOUT_MS,"CMGF")!=0) return -1;
  if (send_at_expect_ok("AT+CSCS=\"GSM\"\r", SHORT_TIMEOUT_MS,"CSCS")!=0) return -1;
  if (send_at_expect_ok("ATI\r",      SHORT_TIMEOUT_MS,"ATI" )!=0) return -1;

  char tmp[READ_CMD_BUF];
  serialport_clear(fd); serialport_write(fd,"AT+CREG?\r");
  if (serialport_read_cmd(fd,tmp,SHORT_TIMEOUT_MS)>=0) printf("%s", tmp);
  serialport_clear(fd); serialport_write(fd,"AT+CSQ\r");
  if (serialport_read_cmd(fd,tmp,SHORT_TIMEOUT_MS)>=0) printf("%s", tmp);
  return 0;
}

/* Normaliza telefone para E.164 (Brasil). Retorna 0 se ok, -1 se inválido. */
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

/* Consulta AT+CSCA? e, se vazio, seta com smsc_cfg (se fornecido) */
static int EnsureSMSC(void) {
  char resp[READ_CMD_BUF];
  serialport_clear(fd);
  if (serialport_write(fd, "AT+CSCA?\r") < 0) return -1;
  if (serialport_read_cmd(fd, resp, SHORT_TIMEOUT_MS) < 0) return -1;
  printf("[SMSC] CSCA? resp:\n%s\n", resp);

  if (strstr(resp, "+CSCA:")) {
    const char *q1 = strchr(resp, '\"');
    const char *q2 = q1 ? strchr(q1+1, '\"') : NULL;
    if (q1 && q2 && (q2 > q1+1)) {
      printf("[SMSC] já configurado: %.*s\n", (int)(q2 - (q1+1)), q1+1);
      return 0;
    }
  }

  if (!*smsc_cfg) {
    fprintf(stderr, "[SMSC] ausente e nenhum SMSC fornecido no cfg/ENV.\n");
    return -1;
  }

  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+CSCA=\"%s\"\r", smsc_cfg);
  printf("[SMSC] definindo CSCA: %s\n", cmd);
  if (send_at_expect_ok(cmd, SHORT_TIMEOUT_MS, "CSCA") != 0) {
    fprintf(stderr, "[SMSC] falha ao setar CSCA\n");
    return -1;
  }
  return 0;
}

/* ===== Envio de SMS (com logs no banco) ===== */
static int EnviaSMS(int job_id_param, const char *telefone_e164, const char *texto) {
  int msg_len = (int)strlen(texto);
  printf("[SMS-%d] Iniciando envio...\n", job_id_param);
  printf("[SMS-%d] Telefone: %s\n", job_id_param, telefone_e164);
  printf("[SMS-%d] Mensagem (len=%d): \"%s\"\n", job_id_param, msg_len, texto);

  if (msg_len > MAX_SMS_CHARS) {
    fprintf(stderr, "[%d] Mensagem excede %d caracteres, abortando.\n", job_id_param, MAX_SMS_CHARS);
    LogEnvioDB(job_id_param, telefone_e164, texto, "DESCARTE", -1, NULL, "Mensagem muito longa");
    return -1;
  }

  char tmp[READ_CMD_BUF];

  /* 1. Testa modem */
  printf("[SMS-%d] Enviando AT (ping modem)...\n", job_id_param);
  if (send_at_expect_ok("AT\r", SHORT_TIMEOUT_MS, "Ping antes do envio") != 0) {
    fprintf(stderr, "[%d] Modem não respondeu AT antes do envio\n", job_id_param);
    LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", 500, "Sem resposta a AT", "Modem não respondeu AT");
    return -1;
  }

  /* 2. CMGS */
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+CMGS=\"%s\"\r", telefone_e164);
  printf("[SMS-%d] Enviando comando: %s\n", job_id_param, cmd);
  if (serialport_write(fd, cmd) < 0) {
    fprintf(stderr, "[%d] Falha escrevendo CMGS\n", job_id_param);
    LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", 500, "Falha ao escrever CMGS", "Escrita serial falhou");
    return -1;
  }

  /* 3. Espera '>' */
  printf("[SMS-%d] Aguardando prompt '>'...\n", job_id_param);
  if (serialport_read_cmd(fd, tmp, PROMPT_TIMEOUT_MS) < 0 || !strstr(tmp, ">")) {
    int code=-1; char which[8]="";
    if (extract_cms_from_resp(tmp, &code, which, sizeof(which))) {
      fprintf(stderr, "[%d] %s ERROR: %d (%s) durante espera de '>'\n",
              job_id_param, which, code, cms_reason_pt(code));
      LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", code, tmp, cms_reason_pt(code));
    } else {
      fprintf(stderr, "[%d] Timeout aguardando '>' para CMGS. Resposta bruta:\n%s\n", job_id_param, tmp);
      LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", 332, tmp, "Tempo de rede esgotado / prompt ausente");
    }
    return -1;
  }

  /* 4. Envia texto */
  if (serialport_write(fd, texto) < 0) {
    fprintf(stderr, "[%d] Falha escrevendo texto da mensagem\n", job_id_param);
    LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", 500, "Falha escrevendo texto", "Escrita serial falhou");
    return -1;
  }

  /* 5. CTRL+Z */
  uint8_t z = CTRL_Z;
  if (serialport_write_raw(fd, &z, 1) < 0) {
    fprintf(stderr, "[%d] Falha enviando CTRL+Z\n", job_id_param);
    LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", 500, "Falha enviando CTRL+Z", "Escrita serial falhou");
    return -1;
  }

  /* 6. Confirmação final */
  tmp[0] = '\0';
  int r = serialport_read_cmd(fd, tmp, OK_TIMEOUT_MS);
  if (r < 0) {
    fprintf(stderr, "[%d] Timeout aguardando confirmação do envio\n", job_id_param);
    LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", 332, tmp, "Tempo de rede esgotado na confirmação");
    return -1;
  }

  /* 7. Avalia resposta */
  if (strstr(tmp, "ERROR")) {
    int code = -1; char which[8] = "";
    if (extract_cms_from_resp(tmp, &code, which, sizeof(which))) {
      fprintf(stderr, "[%d] Modem retornou %s ERROR: %d (%s). Dica: %s\n",
              job_id_param, which, code, cms_reason_pt(code), cms_hint_pt(code));
      LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", code, tmp, cms_reason_pt(code));
    } else {
      fprintf(stderr, "[%d] Modem retornou ERROR (sem código explícito)\n", job_id_param);
      LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", 500, tmp, "Erro sem código");
    }
    return -1;
  }
  if (!strstr(tmp, "+CMGS:") || !strstr(tmp, "OK")) {
    int code=-1; char which[8]="";
    if (extract_cms_from_resp(tmp, &code, which, sizeof(which))) {
      fprintf(stderr, "[%d] %s ERROR: %d (%s) (confirmação incompleta)\n",
              job_id_param, which, code, cms_reason_pt(code));
      LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", code, tmp, cms_reason_pt(code));
    } else {
      fprintf(stderr, "[%d] Confirmação possivelmente incompleta.\n", job_id_param);
      LogEnvioDB(job_id_param, telefone_e164, texto, "ERRO", 500, tmp, "Confirmação incompleta");
    }
    return -1;
  }

  printf("[SMS-%d] SMS enviado com sucesso!\n", job_id_param);
  LogEnvioDB(job_id_param, telefone_e164, texto, "SUCESSO", -1, tmp, "Enviado com sucesso");
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

    /* telefone (normaliza p/ E.164) */
    snprintf(job_telefone, sizeof(job_telefone), "%s", row[1] ? row[1] : "");
    char e164[32];
    if (normalize_br_e164(job_telefone, e164, sizeof(e164)) != 0) {
      fprintf(stderr, "[JOB %d] Telefone inválido p/ E.164 (in: %s) — marcando status=9\n", this_idjob, job_telefone);
      snprintf(SQLCMD,sizeof(SQLCMD), "UPDATE jobs SET status = 9 WHERE idjob = %d;", this_idjob);
      ExecQuery(SQLCMD);
      /* log no banco: DESCARTE */
      LogEnvioDB(this_idjob, job_telefone, row[2] ? row[2] : "", "DESCARTE", -1, NULL, "Telefone inválido E.164");
      msleep(200);
      continue;
    }

    /* mensagem: copia segura até 1000 chars */
    size_t src_len = (row[2] ? lengths[2] : 0);
    size_t max_len = sizeof(job_mensagem) - 1;
    if (src_len > max_len) src_len = max_len;
    if (row[2] && src_len > 0) {
      memcpy(job_mensagem, row[2], src_len);
      job_mensagem[src_len] = '\0';
    } else {
      job_mensagem[0] = '\0';
    }

    /* normaliza CR/LF para espaço (para o modem) */
    for (char *p = job_mensagem; *p; ++p) {
      if (*p == '\r' || *p == '\n') *p = ' ';
    }

    printf("[JOB] id=%d telefone(E164)=%s len_msg=%zu\n", this_idjob, e164, strlen(job_mensagem));

    /* Envio com 2 tentativas */
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
      printf("Job %d: status atualizado para 1 (sucesso)\n", this_idjob);
    } else {
      snprintf(SQLCMD,sizeof(SQLCMD), "UPDATE jobs SET status = 9 WHERE idjob = %d;", this_idjob);
      ExecQuery(SQLCMD);
      fprintf(stderr, "Job %d: erro após 2 tentativas. Status=9.\n", this_idjob);
    }

    count++;
    msleep(200);
  }

  mysql_free_result(result);
  return count;
}

static void Closecon(void) { mysql_close(&mycon); }

/* ===== Outros ===== */
static void Wellcome(void) {
  printf("srvSMS - Servidor de SMS\n");
  printf("Criado por Marcelo Maurin Martins\n");
  printf("Todos os direitos reservados a Maurinsoft\n");
  printf("http://maurinsoft.com.br\n\n");
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
  toptions.c_cflag &= ~PARENB; toptions.c_cflag &= ~CSTOPB; toptions.c_cflag &= ~CSIZE; toptions.c_cflag |= CS8;
  toptions.c_cflag &= ~CRTSCTS; toptions.c_cflag |= CREAD | CLOCAL;
  toptions.c_iflag &= ~(IXON|IXOFF|IXANY);
  toptions.c_lflag &= ~(ICANON|ECHO|ECHOE|ISIG);
  toptions.c_oflag &= ~OPOST;
  toptions.c_cc[VMIN]=0; toptions.c_cc[VTIME]=10;
  if (tcsetattr(fd_local,TCSANOW,&toptions)<0) { perror("tcsetattr"); close(fd_local); return -1; }
  int flags=fcntl(fd_local,F_GETFL,0); fcntl(fd_local,F_SETFL, flags & ~O_NONBLOCK);
  int mctl; ioctl(fd_local,TIOCMGET,&mctl); mctl |= TIOCM_DTR|TIOCM_RTS; ioctl(fd_local,TIOCMSET,&mctl);
  return fd_local;
}

/* ===== MAIN ===== */
int main(int argc, char *argv[]) {
  if (ensure_config(&g_cfg)!=0) {
    fprintf(stderr,"Falha ao preparar/carregar %s\n", CFG_PATH);
    return EXIT_FAILURE;
  }
  g_cfg_mtime = file_mtime(CFG_PATH);

  /* CLI: -d /dev/tty..., -b 115200, -m S|A */
  int opt; int baud=9600;
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

  /* Determina MODO efetivo: CLI > ENV > CFG */
  char env_force=0; const char *f=getenv("SMS_FORCE_MODO");
  if (f && (*f=='S'||*f=='s'||*f=='A'||*f=='a')) env_force = (*f=='s'||*f=='S') ? 'S':'A';
  char efetivo = g_cli_modo ? g_cli_modo : (env_force ? env_force : g_cfg.modo);
  g_mode_daemon = (efetivo=='S');

  /* Inicia log conforme MODO efetivo */
  init_daily_log();
  Wellcome();

  /* Carrega/override DB */
  apply_db_from_cfg(&g_cfg);
  if (!*localdb || !*userdb || !*aliasdb) {
    fprintf(stderr,"Config de banco incompleta. Defina LOCALDB/USERDB/ALIASDB em %s ou via variáveis de ambiente SMS_*\n", CFG_PATH);
    return EXIT_FAILURE;
  }

  printf("Abrindo conexao com Banco de dados...\n");
  if (Mysqlcon()==-1) { fprintf(stderr,"Nao é possivel conectar ao banco de dados\n"); return EXIT_FAILURE; }

  /* Charset consistente */
  ExecQuery("SET NAMES utf8mb4");
  ExecQuery("SET CHARACTER SET utf8mb4");

  printf("Inicializando Device %s @ %d...\n", device, baud);
  fd=serialport_init(device, baud);
  if (fd==-1) { fprintf(stderr,"Erro ao abrir porta serial\n"); Closecon(); return EXIT_FAILURE; }

  printf("Inicializando Modem...\n");
  if (InicializaModem()!=0) {
    fprintf(stderr,"Falha ao iniciar: algum comando AT inicial não retornou OK.\n");
    Closecon(); close(fd); return EXIT_FAILURE;
  }

  /* DEBUG/status de rede + SMSC */
  serialport_clear(fd); serialport_write(fd,"AT+CPIN?\r");
  serialport_read_cmd(fd, g_tmpbuf, SHORT_TIMEOUT_MS); printf("[DBG] CPIN?:\n%s\n", g_tmpbuf);

  serialport_clear(fd); serialport_write(fd,"AT+CREG?\r");
  serialport_read_cmd(fd, g_tmpbuf, SHORT_TIMEOUT_MS); printf("[DBG] CREG?:\n%s\n", g_tmpbuf);

  serialport_clear(fd); serialport_write(fd,"AT+CSQ\r");
  serialport_read_cmd(fd, g_tmpbuf, SHORT_TIMEOUT_MS); printf("[DBG] CSQ:\n%s\n", g_tmpbuf);

  printf("Verificando SMSC...\n");
  if (EnsureSMSC() != 0) {
    fprintf(stderr, "SMSC não configurado/indisponível; isso pode causar erros de envio (+CMS 302/500).\n");
  }

  printf("Iniciando Serviço de Monitoramento de Solicitações. (MODO=%c)\n", efetivo);
  if (g_mode_daemon) printf("Rodando silencioso (apenas log em %s)\n", LOG_DIR);
  else               printf("Exibindo logs na tela e gravando em %s\n", LOG_DIR);

  while (1) {
    reload_cfg_if_changed();
    maybe_rotate_daily_log();

    if (!now_in_window(&g_cfg)) { msleep(10000); continue; }

    int n=BuscaJobs();
    if (n<=0) msleep(1000); else msleep(500);
    serialport_clear(fd);
  }

  Closecon(); close(fd);
  return EXIT_SUCCESS;
}
