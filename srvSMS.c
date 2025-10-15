/** Servidor de Envio de SMS
** Criado por Marcelo Maurin Martins
** Ajustes por pedido:
** - Config /etc/SMS/sms.cfg (auto-criação; sem credenciais hardcoded)
** - Lê credenciais do cfg com override por ENV (SMS_LOCALDB, SMS_USERDB, SMS_PASSDB, SMS_ALIASDB)
** - Valida configuração sem vazar segredos nos logs
** - Janela de envio por dia/horário
** - Inicialização do modem: exige OK em cada comando (fail-fast)
** - Log diário em /var/log/SMS/YYYY-MM-DD.log com rotação à meia-noite
** - MODO: S (daemon silencioso) / A (aplicação verbosa). Ambos mantêm log diário.
** - Hot-reload da configuração: ao editar /etc/SMS/sms.cfg (incl. MODO), reconfigura em tempo real
** - Prioridade de MODO: CLI -m > ENV SMS_FORCE_MODO > cfg
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
#define READ_CMD_BUF  2048
#define PROMPT_TIMEOUT_MS 15000
#define OK_TIMEOUT_MS      20000
#define SHORT_TIMEOUT_MS    3000

#define CFG_DIR  "/etc/SMS"
#define CFG_PATH "/etc/SMS/sms.cfg"
#define LOG_DIR  "/var/log/SMS"

/* ===== Config ===== */
typedef struct {
  int inicio_min;   // minutos desde 00:00
  int fim_min;      // minutos desde 00:00
  int dias[7];      // 0=domingo ... 6=sábado  (POSIX: tm_wday)
  char localdb[80];
  char userdb[80];
  char passdb[80];
  char aliasdb[80];
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

char SQLCMD[512];

/* JOB */
int  job_id = 0;
char job_telefone[64];
char job_mensagem[512];

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

  if (h < 0 || h > 23 || m < 0 || m > 59) {
    return -1;
  }

  return h * 60 + m;
}

static int dir_exists(const char *path) { struct stat st; return (stat(path,&st)==0 && S_ISDIR(st.st_mode)); }
static int file_exists(const char *path){ struct stat st; return (stat(path,&st)==0); }

/* ===== Protos ===== */
/* Serial */
static int serialport_init(const char* serialport, int baud);
static int serialport_write(int fd, const char* str);
static int serialport_write_raw(int fd, const uint8_t* data, size_t len);
static int serialport_read_until(int fd, char* buf, char until, int timeout_ms);
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

/* Hot-reload MODO/cfg */
static time_t file_mtime(const char *path);
static void   reapply_logging_mode(void);
static int    reload_cfg_if_changed(void);

/* Outros */
static void Wellcome(void);

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
  );
  fclose(f);
  printf("Config padrão criado em %s\n", CFG_PATH);
  return 0;
}
static void set_cfg_defaults(SMSConfig *cfg) {
  cfg->inicio_min = 7*60; cfg->fim_min = 20*60;
  for (int i=0;i<7;i++) cfg->dias[i]=1;
  cfg->localdb[0]=cfg->userdb[0]=cfg->passdb[0]=cfg->aliasdb[0]='\0';
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

  snprintf(localdb,sizeof(localdb),"%s",(e_local&&*e_local)?e_local:cfg->localdb);
  snprintf(userdb, sizeof(userdb), "%s",(e_user &&*e_user )?e_user :cfg->userdb);
  snprintf(passdb, sizeof(passdb), "%s",(e_pass &&*e_pass )?e_pass :cfg->passdb);
  snprintf(aliasdb,sizeof(aliasdb),"%s",(e_alias&&*e_alias)?e_alias:cfg->aliasdb);
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
static int BuscaJobs(void) {
  snprintf(SQLCMD,sizeof(SQLCMD),
           "SELECT idjob, telefone, mensagem, status FROM jobs WHERE status = 0;");
  if (mysql_query(&mycon, SQLCMD)!=0) {
    fprintf(stderr, "Erro ao executar SQL: %s\n", mysql_error(&mycon));
    return -1;
  }
  MYSQL_RES *result=mysql_store_result(&mycon);
  if (!result) { fprintf(stderr,"Resultado nulo em SQL: %s\n", SQLCMD); return -1; }
  MYSQL_ROW row; int count=0;
  while ((row=mysql_fetch_row(result))!=NULL) {
    job_id = row[0]?atoi(row[0]):0;
    snprintf(job_telefone,sizeof(job_telefone), "%s", row[1]?row[1]:"");
    snprintf(job_mensagem,sizeof(job_mensagem), "%s", row[2]?row[2]:"");
    printf("Job %d: Enviando SMS para %s (%s)\n", job_id, job_telefone, job_mensagem);
    if (EnviaSMS(job_id, job_telefone, job_mensagem)==0) {
      snprintf(SQLCMD,sizeof(SQLCMD), "UPDATE jobs SET status = 1 WHERE idjob = %d;", job_id);
      ExecQuery(SQLCMD);
      printf("Job %d: status atualizado para 1\n", job_id);
    }
    count++;
  }
  mysql_free_result(result);
  return count;
}
static void Closecon(void) { mysql_close(&mycon); }

/* ===== Serial ===== */
static int serialport_clear(int fd) { return tcflush(fd, TCIOFLUSH); }
static int serialport_write(int fd, const char* str) {
  size_t len=strlen(str); ssize_t n=write(fd,str,len); if(n<0||(size_t)n!=len) return -1; return (int)n;
}
static int serialport_write_raw(int fd, const uint8_t* data, size_t len) {
  ssize_t n=write(fd,data,len); if(n<0||(size_t)n!=len) return -1; return (int)n;
}
static int serialport_read_until(int fd, char* out, char until, int timeout_ms) {
  int i=0; out[0]='\0';
  struct timeval start,now; gettimeofday(&start,NULL);
  while (1) {
    char b; int n=read(fd,&b,1);
    if (n==1) {
      out[i++]=b; if (i>=READ_LINE_BUF-1) break; if (b==until) break;
    } else if (n==0 || (n<0 && (errno==EAGAIN||errno==EWOULDBLOCK))) {
      msleep(10);
    } else if (n<0) { return -1; }
    gettimeofday(&now,NULL);
    int elapsed=(int)((now.tv_sec-start.tv_sec)*1000 + (now.tv_usec-start.tv_usec)/1000);
    if (elapsed>timeout_ms) break;
  }
  out[i]='\0'; return i;
}
/* Lê várias linhas até encontrar OK/ERROR/>, com timeout total em ms */
static int serialport_read_cmd(int fd, char* out, int timeout_ms) {
  out[0]='\0'; char line[READ_LINE_BUF];
  struct timeval start,now; gettimeofday(&start,NULL);
  while (1) {
    int n=serialport_read_until(fd,line,'\n',1000);
    if (n>0) {
      if (strstr(line,"+CSQ:")==NULL && strstr(line,"+RSSI:")==NULL) {
        strncat(out,line, READ_CMD_BUF-1-(int)strlen(out));
      }
      if (strstr(out,"OK")!=NULL || strstr(out,"ERROR")!=NULL || strstr(out,">")!=NULL) return 0;
    }
    gettimeofday(&now,NULL);
    long elapsed_ms=(long)(now.tv_sec-start.tv_sec)*1000L + (long)(now.tv_usec-start.tv_usec)/1000L;
    if (elapsed_ms >= (long)timeout_ms) return -2;
    msleep(10);
  }
}

/* ===== GSM ===== */
static int send_at_expect_ok(const char *cmd, int timeout_ms, const char *etapa) {
  char tmp[READ_CMD_BUF]; serialport_clear(fd);
  if (serialport_write(fd, cmd)<0) { fprintf(stderr,"[Init][%s] Falha ao escrever: %s\n", etapa, cmd); return -1; }
  if (serialport_read_cmd(fd, tmp, timeout_ms)<0) { fprintf(stderr,"[Init][%s] Timeout aguardando resposta de: %s\n", etapa, cmd); return -1; }
  if (!strstr(tmp,"OK")) { fprintf(stderr,"[Init][%s] Resposta sem OK: %s\n", etapa, tmp); return -1; }
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
static int EnviaSMS(int job_id, const char *job_telefone, const char *job_mensagem) {
  char tmp[READ_CMD_BUF];
  if (send_at_expect_ok("AT\r", SHORT_TIMEOUT_MS, "Ping antes do envio")!=0) {
    fprintf(stderr,"[%d] Modem não respondeu AT antes do envio\n", job_id); return -1;
  }
  char cmd[128]; snprintf(cmd,sizeof(cmd), "AT+CMGS=\"%s\"\r", job_telefone);
  if (serialport_write(fd, cmd)<0) { fprintf(stderr,"[%d] Falha escrevendo CMGS\n", job_id); return -1; }
  if (serialport_read_cmd(fd,tmp,PROMPT_TIMEOUT_MS)<0 || !strstr(tmp,">")) {
    fprintf(stderr,"[%d] Timeout aguardando '>' para CMGS: %s\n", job_id, tmp); return -1;
  }
  if (serialport_write(fd, job_mensagem)<0) { fprintf(stderr,"[%d] Falha escrevendo texto da mensagem\n", job_id); return -1; }
  uint8_t z=CTRL_Z; if (serialport_write_raw(fd,&z,1)<0) { fprintf(stderr,"[%d] Falha enviando CTRL+Z\n", job_id); return -1; }
  tmp[0]='\0';
  if (serialport_read_cmd(fd,tmp,OK_TIMEOUT_MS)<0) { fprintf(stderr,"[%d] Timeout aguardando confirmação do envio\n", job_id); return -1; }
  printf("[%d] Resposta envio: %s\n", job_id, tmp);
  if (strstr(tmp,"ERROR")) { fprintf(stderr,"[%d] Modem retornou ERROR\n", job_id); return -1; }
  if (!strstr(tmp,"+CMGS:") || !strstr(tmp,"OK")) { fprintf(stderr,"[%d] Confirmação possivelmente incompleta.\n", job_id); }
  return 0;
}

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
  /* Carrega cfg para saber MODO antes do log */
  if (ensure_config(&g_cfg)!=0) {
    fprintf(stderr,"Falha ao preparar/carregar %s\n", CFG_PATH);
    return EXIT_FAILURE;
  }
  g_cfg_mtime = file_mtime(CFG_PATH);

  /* CLI: -d /dev/tty..., -b 115200, -m S|A (força modo) */
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

  printf("Inicializando Device %s @ %d...\n", device, baud);
  fd=serialport_init(device, baud);
  if (fd==-1) { fprintf(stderr,"Erro ao abrir porta serial\n"); Closecon(); return EXIT_FAILURE; }

  printf("Inicializando Modem...\n");
  if (InicializaModem()!=0) {
    fprintf(stderr,"Falha ao iniciar: algum comando AT inicial não retornou OK.\n");
    Closecon(); close(fd); return EXIT_FAILURE;
  }

  printf("Iniciando Serviço de Monitoramento de Solicitações. (MODO=%c)\n", efetivo);
  if (g_mode_daemon) printf("Rodando silencioso (apenas log em %s)\n", LOG_DIR);
  else               printf("Exibindo logs na tela e gravando em %s\n", LOG_DIR);

  while (1) {
    reload_cfg_if_changed();   /* hot-reload cfg/MODO */
    maybe_rotate_daily_log();  /* reabre log quando mudar o dia */

    if (!now_in_window(&g_cfg)) { msleep(10000); continue; }

    int n=BuscaJobs();
    if (n<=0) msleep(1000); else msleep(500);
    serialport_clear(fd);
  }

  Closecon(); close(fd);
  return EXIT_SUCCESS;
}
