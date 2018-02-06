/** Servidor de Envio de SMS
** Criado por Marcelo Maurin Martins\n
** marcelomaurinmartins@gmail.com
** whatsapp +55 16981434112
**/

#include <stdio.h>    /* Standard input/output definitions */
#include <stdlib.h>
#include <stdint.h>   /* Standard types */
#include <string.h>   /* String function definitions */
#include <unistd.h>   /* UNIX standard function definitions */
#include <fcntl.h>    /* File control definitions */
#include <errno.h>    /* Error number definitions */
#include <termios.h>  /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <getopt.h>
#include <mysql/mysql.h>
#include <mysql/my_global.h>
#include <stdbool.h>

#define CTRL_Z 26



void usage(void);

int serialport_init(const char* serialport, int baud);

int serialport_writebyte(int fd, uint8_t b);

int serialport_write(int fd, const char* str);

int serialport_read_until(int fd, char* buf, char until);
//Le todas as linhas do comando ate encontrar ERROR ou OK
int serialport_read_cmd(int fd, char* buf);

//Limpa porta serial
int serialport_clear(int fd);

int	EnviaSMS(int job_id, char *job_telefone, char *job_mensagem);

MYSQL mycon; //Ponteiro de conexao Mysql


int res;
int fd;
char dat[80];
char buf[500];
char PRODUTO[20];
//char SERIAL[20];
char HUMIDADE[20];
char OldHUMIDADE[20];
char TEMPERATURA[20];
char OldTEMPERATURA[20];
char RELEStatus[20];
char MovimentoStatus[20];
char LuzManual[20];
char strerro[255];

//flags
bool flgRELEStatus;
bool flgMovimentoStatus;
bool flgOldMovimentoStatus;
bool flgLuzManual;
bool flgOldLuzManual;
 
char localdb[80] = "hostdatabase";
char userdb[80] = "userdatabase";
char passdb[80] = "password";
char aliasdb[80] = "databasename";
char SQLCMD[500];

/**Campos de JOB**/
int job_id = 0;
char job_telefone[30];
char job_mensagem[500];

//Device USB modem, please check your
char device[30] = "/dev/ttyUSB2";


int kbhit(void)
{
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

int Mysqlcon()
{
    sprintf(strerro,"MySQL client version: %s",mysql_get_client_info());
    //LogOcorrencia(strerro);
    mysql_init(&mycon);


    if (mysql_real_connect(&mycon, localdb, userdb, passdb, aliasdb, 0, NULL, 0) == NULL)
    {
        
		//LogOcorrencia("Erro ao conectar no banco");
        return -1;
    }
    else
    {
	printf("Conectou no banco\n");
        return 0;
    }

}
 
//Roda comando
bool ExecQuery(char *SQLCMD)
{

    res =  (mysql_query(&mycon, SQLCMD));
    if (!res)
    {
        return true;
    }
    else
    {
        sprintf(strerro,"Erro ao Executar SQL:%s",SQLCMD);
        //LogOcorrencia(strerro);
        return false;
    }
}

//Pega a lista de Jobs Selecionados
bool BuscaJobs()
{
    int cont = 0;
    //char sqlcmd[250];
    sprintf(SQLCMD,"SELECT idjob,telefone,mensagem,status FROM jobs where status = 0;");
    if (mysql_query(&mycon, SQLCMD)==true)
    {
        //LogOcorrencia("Erro ao Executar SQL");
        return -1;
    };

    MYSQL_RES *result = mysql_store_result(&mycon);

    if (result == NULL)
    {
        sprintf(strerro,"Resultado nulo em OpenQuery SQL:%s",SQLCMD);
		//LogOcorrencia(strerro);
        return -1;
    }

    int num_fields = mysql_num_fields(result);

    MYSQL_ROW row;
    
	//printf("Pesquisou na base!\n");
    while ((row = mysql_fetch_row(result)))
    {        
		job_id = atoi(row[0]);
		sprintf(job_telefone,"%s",row[1]);
		sprintf(job_mensagem,"%s",row[2]);
		printf("Achou Registro!\n");
		
		printf("Enviando SMS para %s (%s)\n",job_telefone,job_mensagem);
		/**Colocar logica Aqui*/
		if (EnviaSMS(job_id, job_telefone, job_mensagem) == 0)
		{		
			//Atualiza Status de Registro
			printf("Processou job %s!\n",job_telefone);
			sprintf(SQLCMD,"update jobs set status = 1 where idjob = %i ",job_id);
			ExecQuery(SQLCMD);
		}
    }

    mysql_free_result(result);
    //printf("Saiu da funcao BuscaCMD\n");
    return 1;   
 }

int OpenQuery(char *SQLCMD)
{
    int cont = 0;
    if (mysql_query(&mycon, "SELECT * FROM Cars"))
    {
        //LogOcorrencia("Erro ao Executar SQL");
        return -1;
    };

    MYSQL_RES *result = mysql_store_result(&mycon);

    if (result == NULL)
    {
        sprintf(strerro,"Resultado nulo em OpenQuery SQL:%s",SQLCMD);
		//LogOcorrencia(strerro);
        return -1;
    }

    int num_fields = mysql_num_fields(result);

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result)))
    {

        for( cont = 0; cont < num_fields; cont++)
        {
            printf("%s ", row[cont] ? row[cont] : "NULL");
        }
        printf("\n");

    }

    mysql_free_result(result);
    return 0;

}


//fecha conexao
int Closecon()
{

    mysql_close(&mycon);
    exit(1);

}


//Envia Mensagem
int	EnviaSMS(int job_id, char *job_telefone, char *job_mensagem)
{
    //printf("Escrevendo comando releon\n");
    int rc,n;  

  	//Envia Comandos Texto
	printf("SMS para %s\n");
    sprintf(dat,"AT+CMGS=\"%s\"\r\n",job_telefone);
	//printf("env:%s\n",dat);
	//AT+CMGS='+16981434112'
    //Envia cmd
    rc = serialport_write(fd, dat);
    n = serialport_read_until(fd, buf,'>');
	printf("rec:%s\n",buf);
	sprintf(dat,"%s\n%c",job_mensagem,CTRL_Z);
	printf("env:%s\n",dat);
    //Envia cmd
    rc = serialport_write(fd, dat);
    n = serialport_read_cmd(fd, buf);
	printf("rec:%s\n",buf);
    
	if (strstr(buf,"ERROR")>0)
	{
		printf("Erro na comunicacao:%s\n",buf);
		return -1;
	}
	return 0;
}

//Inicializa Modem GSM
int InicializaModem()
{
	
    //printf("Escrevendo comando releoff\n");
    int rc,n;
	
	printf("Configurando ASCII\n");
    sprintf(dat,"AT\r\n");
    //Envia cmd
    rc = serialport_write(fd, dat);
	n = serialport_read_until(fd, dat,'\n');
	printf("rec:%s\n",dat);
	
	//Envia Comandos Texto
	printf("Configurando ASCII\n");
    sprintf(dat,"at+cmgf=1\r\n");
    //Envia cmd
    rc = serialport_write(fd, dat);
    n = serialport_read_cmd(fd, buf);
    printf("teste1:%s\n",buf);
	if (strstr(buf,"ERROR")>0)
	{
		return -1;
	}
	
	//Pega nome do Modem
	printf("Verificando nome do modem\n");
    sprintf(dat,"ATI\r\n");
    //Envia cmd
    rc = serialport_write(fd, dat);
    n = serialport_read_cmd(fd, buf);
    printf("%s\n",buf);
	if (strstr(buf,"ERROR")>0)
	{
		return -1;
	}
	
	//Status do Servico
	printf("Verificando status do Servico\r");
    sprintf(dat,"AT+CREG?\r\n");
    //Envia cmd
    rc = serialport_write(fd, dat);
    n = serialport_read_cmd(fd, buf);
    printf("%s\n",buf);
	if (strstr(buf,"ERROR")>0)
	{
		return -1;
	}
	
	//AT+CSQ
	//Força do sinal
	printf("Verificando força do sinal\n");
    sprintf(dat,"AT+CSQ\r\n");
    //Envia cmd
    rc = serialport_write(fd, dat);
    n = serialport_read_cmd(fd, buf);
    printf("%s\n",buf);
	if (strstr(buf,"ERROR")>0)
	{
		return -1;
	}	
	return 0;
}



bool AguardaEsc()
{
    char opcao;

    //enquanto nao se pressiona nada
    if (kbhit())
    {
        //fim de bloco
        //opcao = getch();  //captura teclado
        opcao =  getchar();

        //se ler esc sai do laco
        if(opcao == 27)
        {
            //printf("Tecla esc pressionada - fim de programa\n");
            printf("Obrigado por usar nosso programa\n");

            return true;

        }

    }
}

void Wellcome()
{
	   printf("srvSMS - Servidor de SMS\n");
	   printf("Criado por Marcelo Maurin Martins\n");
	   printf("Todos os direitos reservados a Maurinsoft \n");
	   printf("http://maurinsoft.com.br\n\n"); 
}



int main(int argc, char *argv[])
{
	//Cabecalho do Projeto
	Wellcome();
    bool controle = true;

    printf("Abrindo conexao com Banco de dados...\n");

    if (Mysqlcon()==-1) //Abre conexao com mysql
    {
        printf("Nao é possivel conectar ao banco de dados\n");
        return -1;
    }
    printf("Inicializando Device...\n");

    fd = 0;

	int baudrate = B9600;
    baudrate = 9600; //Velocidade padrao do modem GSM

    printf("Device default:%s\n",device);
    fd = serialport_init(device, baudrate);
    if(fd==-1)
    {
        printf("Erro ao abrir porta serial\n");
        return -1;
    }

    printf("Inicializando Modem...\n");
    if (InicializaModem()==0)
	//if(0==0)
	{
		printf("Iniciando Serviço de Monitoramento de Solicitações.\n");
		printf("Para sair do programa pressione esc\n");

		//serialport_clear(fd);
		while (controle==true)
		{     
			sleep(1);
			
			if( BuscaJobs() ==1)
			{
				//Executou buscajobs
			}	  
			/*	
			if (AguardaEsc() == true)
			{
				controle = false;
				break;
			}
			*/
			
			sleep(1);
			//printf("Limpa Serial\n");
			serialport_clear(fd); 
		}
	}
	else
	{
		printf("Não conseguiu inicializar GSM Modem\n");
	}
		
    //finaliza programa
    Closecon(); //fecha conexao com mysql
    close(fd);
    exit(EXIT_SUCCESS);
} // end main


int serialport_writebyte( int fd, uint8_t b)
{
	
    int n = write(fd,&b,1);
    if( n!=1)
        return -1;
    return 0;
}

int serialport_write(int fd, const char* str)
{
    printf("Escrevendo bytes %s",str);
    int len = strlen(str);
    int n = write(fd, str, len);
    if( n!=len )
        return -1;
    return n;
}

//Le todas as linhas do comando ate encontrar ERROR ou OK
int serialport_read_cmd(int fd, char* buf)
{
	int n = 0;
	sprintf(buf,'\0');
	char info[500];
	//char buf[200];
	do
	{
	   n = serialport_read_until(fd, info,'\n');
	   if ((strstr(info,"+CSQ:")>=0) || (strstr(info,"+RSSI:")>=0))
	    {
		  //Parametro de Sinal do Celular deve ignorar   
		  printf("Ignorou linha!\n");
		  
		}
		else
		{
			//printf("%s",info);
			sprintf(buf,"%s%s",buf,info);	
		}
	}  
	while ((strstr(buf,"OK")<0) and (strstr(buf,"ERROR")<0));
	//while ((strstr(info,"OK")<=0) and (strstr(info,"ERROR")<=0));
	//printf("buffer in:%s\n",buf);
	return 0;
    
}


int serialport_read_until(int fd, char* buf, char until)
{
    //printf("capturando informacoes da porta...");
    char b[1];
    int i=0;
	int cont = 0;
    do {
		cont++; //incrementador de espera
        int n = read(fd, b, 1);  // read a char at a time
        //printf("%c",b);
        if( n==-1) return -1;    // couldn't read
        if( n==0 ) {
            usleep( 10 * 1000 ); // wait 10 msec try again
            continue;
        }
		//printf("byte:%c",b[0]);
        buf[i] = b[0];
        i++;
        //} while( strcmp(buf,until)!=0 );
    } while( (b[0] != until )&&(cont<5));
	//} while (b[0] != until );
    buf[i] = 0;  // null terminate the string
	//printf("Saiu:%s\n",buf);
    return i;
}

//Limpa porta serial
int serialport_clear(int fd)
{
 tcflush(fd, TCIOFLUSH); 
}

// takes the string name of the serial port (e.g. "/dev/tty.usbserial","COM1")
// and a baud rate (bps) and connects to that port at that speed and 8N1.
// opens the port in fully raw mode so you can send binary data.
// returns valid fd, or -1 on error
int serialport_init(const char* serialport, int baud)
{
    struct termios toptions;
    int fd;
    //fprintf(stderr,"init_serialport: opening port %s @ %d bps\n",
    //        serialport,baud);
    //fd = open(serialport, O_RDWR | O_NOCTTY | O_NDELAY);
    fd = open(serialport, O_RDWR | O_NOCTTY);
    if (fd == -1)  {
        perror("init_serialport: Unable to open port ");
        return -1;
    }
    if (tcgetattr(fd, &toptions) < 0) {
        perror("init_serialport: Couldn't get term attributes");
        return -1;
    }
    speed_t brate = baud; // let you override switch below if needed
    switch(baud) {
    case 4800:
        brate=B4800;
        break;
    case 9600:
        brate=B9600;
        break;
        // if you want these speeds, uncomment these and set #defines if Linux
        //#ifndef OSNAME_LINUX
        //    case 14400:  brate=B14400;  break;
        //#endif
        //
    case 19200:
        brate=B19200;
        break;
        //#ifndef OSNAME_LINUX
        //    case 28800:  brate=B28800;  break;
        //#endif
        //case 28800:  brate=B28800;  break;
    case 38400:
        brate=B38400;
        break;
    case 57600:
        brate=B57600;
        break;
    case 115200:
        brate=B115200;
        break;
    }
    cfsetispeed(&toptions, brate);
    cfsetospeed(&toptions, brate);
    // 8N1
    toptions.c_cflag &= ~PARENB;
    toptions.c_cflag &= ~CSTOPB;
    toptions.c_cflag &= ~CSIZE;
    toptions.c_cflag |= CS8;
    // no flow control
    toptions.c_cflag &= ~CRTSCTS;
    toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
    toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
    toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
    toptions.c_oflag &= ~OPOST; // make raw
    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 0;
    toptions.c_cc[VTIME] = 20;
    if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
        perror("init_serialport: Couldn't set term attributes");
        return -1;
    }
    return fd;
}




