/*
Criado por Marcelo Maurin Martins
Data: 23/04/2015
*/
 
#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include <mysql/mysql.h>
#include <mysql/my_global.h>
#include <stdbool.h> //Booleando
#include <pthread.h>

/*
struct thread_leitura{
   int id;
};
*/

//Tamanho maximo de usuarios conectados
#define maxcli = 100
#define BACKLOG = 4
#define PORT  8083

//Conexoes com banco de dados
#define USER "mmm"
#define PASSWORD "226468"
#define DATABASE "casadb"
#define HOSTDB  "maurinsoft.com.br"

//Prototipagem de variavel
void *Le_Ethernet(void *valor);
void *Le_Cliente(void *valor);

//Variavel de controle da thread
//Primeira Thread é sempre o servidor
pthread_t ListenTh[1];
pthread_t ClientesTh[100];
int th_execute;


MYSQL mycon;

int socket_desc , client_sock , c , read_size;
struct sockaddr_in server , client;
char client_message[2000];
bool flgAtivo = true;

//Buffer de Comandos Buffer
char Buffer[500] = "\0"; /*Buffer de teclado*/
char BufferTCP[500] = "\0"; /*Buffer de TCP*/

int Start_Mysql()
{
	//printf("Conectando ao banco de dados...\n");
    mysql_init(&mycon);

    if (mysql_real_connect(&mycon, HOSTDB, USER, PASSWORD, DATABASE, 0, NULL, 0) != NULL)
    {
        //printf("Conectou no banco\n");
        return 1;
    }
    else
    {
        printf("Erro ao conectar no banco\n");
		flgAtivo = false;
        return -1;
    }
}

//Fecha conexao
void Close_Mysql()
{
	mysql_close(&mycon);
	//printf("Fechou Banco\n");
}

int setNonblocking(int fd)
{
    int flags;

    /* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
    /* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    /* Otherwise, use the old way of doing it */
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}     

//Ativa conexao com servidor
void Start_Ethernet()
{
	printf("Conectando servidor.\n");
	//Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM  , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created"); 
     

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( PORT );
     
    //BIND a conexao
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
	    /* Force the network socket into nonblocking mode */
        setNonblocking(socket_desc); 
        //print the error message
        perror("bind failed. Error");
        //return 1;
		exit(-1);
    }
    puts("bind done");
    flgAtivo = true;
	
	//Listen
	listen(socket_desc , 4);
	
	//Cria thread para escutar ethernet
	th_execute = pthread_create(&ListenTh[0],NULL,Le_Ethernet,(void *)socket_desc);
	
}


void Wellcome()
{
	printf("Bem vindo ao Servidor de Corrente\n");
	printf("Desenvolvido por Marcelo Maurin Martins\n");
}

//Inicializacoes
void Setup()
{
	Start_Ethernet(); //Iniciando o Servico Ethernet
	//Start_Mysql(); //Conexao com Mysql
	Wellcome();
}

//Roda comando
int ExecQuery(char *SQLCMD)
{

    if (mysql_query(&mycon, SQLCMD))
    {
        return 0;
		printf("Sucesso na execução\n");
    }
    else
    {
        return -1;
		printf("Erro na execução da query: %s\n",SQLCMD);
    }
}

int OpenQuery(char *SQLCMD)
{
    int cont = 0;
    if (mysql_query(&mycon, "SELECT * FROM Cars"))
    {
        return -1;
    };

    MYSQL_RES *result = mysql_store_result(&mycon);

    if (result == NULL)
    {

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

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}

int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

void Prompt()
{
	printf("\n \n $>");
}

//Retorna opcoes do MAN
void RetornaMAN()
{
	printf("Relação de Comandos\n\n");
	printf("MAN - Help de Comandos\n");
	printf("LSTCON - Lista de Conexoes ativas\n");
	printf("SAIR - Sair da aplicação\n");
}

/*Executa Processamento de comandos*/
void CMDEXEC()
{	
	//printf("Comando: %s\n",Buffer);
    if (strcmp(Buffer,"SAIR")==0)
	{
		printf("Encontrou Sair\n");
   		flgAtivo = false;
	}
	if (strcmp(Buffer,"MAN")==0)
	{
		//printf("Encontrou MAN\n");
		RetornaMAN();
		
	}
	if (strcmp(Buffer,"id:")==0)
	{
		printf("Encontrou id\n");   		
	}
	
	if (strcmp(Buffer,"LSTCON")==0)
	{
		printf("Encontrou LSTCON\n");   		
	}	
	Prompt();
    sprintf(Buffer,"\0"); /*Reseta Buffer*/
}

void Le_Teclado()
{
	if (kbhit()>0)
	{	
		char c = getch();
		if(c != EOF)
		{
		  if (c != '\n')
		  {
			sprintf(Buffer,"%s%c\0",Buffer,c);
		  }
		   else
		 {
		   CMDEXEC(); /*Processa Arquivo*/
		 }
		}
		
	}
	//printf(".");
}

//Escreve no buffer 
void writesock(int client,char *Info)
{
	char Info2[255];
	sprintf(Info2,"%s",Info);
	int tamanho = strlen(Info2); 
	//printf("tam:%d; %s\n\0",tamanho,Info2);
	
	write(client,Info2,tamanho);
	//printf(client,"%s",Info2);
}

void GravaDados(int ID, float Corrente, float Consumo)
{
	if (Start_Mysql()>0)
	{
	char SQLInfo[255];
	sprintf(SQLInfo,"update IDCORR set CORRENTE = '%f', CONSUMO = '%f' where ID = %d\n",Corrente, Consumo, ID);
	//printf(SQLInfo); 
	ExecQuery(SQLInfo);
	Close_Mysql();
	}
}

int LeDadoPID(int pid,float *Corrente,float *Consumo)
{
	if (Start_Mysql()>0)
	{
		//printf("Entrou LeDadoPID\n");
		int cont = 0;
		char SQLCMD1[500];
		char strCorrente[20];
		char strConsumo[20];
		//memset(SQLCMD1,' ',sizeof(SQLCMD1));
		sprintf(SQLCMD1,"SELECT ID, CORRENTE, CONSUMO FROM IDCORR where ID = %d;\n",pid);
		//printf("SQL:%s",SQLCMD1);
		if (mysql_query(&mycon, SQLCMD1))
		{
			printf("Erro: Não conseguiu realizar pesquisa\n");
			return -1;
		}

		MYSQL_RES *result = mysql_store_result(&mycon);

		if (result == NULL)
		{
			printf("Pesquisa não encontrou resultado\n");
			return 0;
		}

		int num_fields = mysql_num_fields(result);

		MYSQL_ROW row;
		row = mysql_fetch_row(result); //Pega primeiro Registro
		/*
		while ((row = mysql_fetch_row(result)))
		{
			
			for( cont = 0; cont < num_fields; cont++)
			{
				printf("%s ", row[cont] ? row[cont] : "NULL");
			}
			printf("\n");

		}
		*/
		
		strcpy(strCorrente,row[1]);
		strcpy(strConsumo,row[2]);
		
		*Corrente = atof(strCorrente);
		*Consumo = atof(strConsumo);
		//printf("ID:%d, Corrente:%f, Consumo:%f\n",pid,Corrente,Consumo);
		mysql_free_result(result);	
		int NroReg = num_fields;
		Close_Mysql();
		return NroReg;
	}
	else
	{
		return -1;
	}
}

//Analisa comandos
void AnalisaID(char *Dados)
{
	static const char delim[] = "\t\n:\r";
	char *svptr;
	char  *strid;
	char *stridnro;
	char *strCorrente;
	char *strCorrentenro;
	char *strConsumo;
	char *strConsumonro;
	strid= strtok_r(Dados,delim,&svptr);
	//printf("%s\n",strid);
	stridnro= strtok_r(NULL,delim,&svptr);
	//printf("%s\n",stridnro);
	strCorrente = strtok_r(NULL,delim,&svptr);
	//printf("%s\n",strCorrente);		
	strCorrentenro = strtok_r(NULL,delim,&svptr);
	//printf("%s\n",strCorrentenro);	
	strConsumo = strtok_r(NULL,delim,&svptr);
	//printf("%s\n",strConsumo);		
	strConsumonro = strtok_r(NULL,delim,&svptr);
	//printf("%s\n",strConsumonro);	
	//Trabalha os nros
	int ID;
	float Corrente;
	float Consumo;
	ID = atoi(stridnro);
	Corrente = atof(strCorrentenro);
	Consumo = atof(strConsumonro);
	//printf("%d, %f , %f\n", ID,Corrente, Consumo);
	GravaDados(ID,Corrente,Consumo);
	
}

//Analisa comandos
void AnalisaPID(int client, char *Dados)
{
	//printf("Envia:%s\n",Dados);
	
	static const char delim[] = "\t\n:=\r";
	char *svptr;
	char  *strpid;
	char *strpidnro;

		//printf("Info:%s\n",Dados);
		strpid= strtok_r(Dados,delim,&svptr);
		strpidnro= strtok_r(NULL,delim,&svptr);
		//printf("PID:%s\n",strpidnro);
		
		//printf("%s\n",strConsumonro);	
		//Trabalha os nros
		float Corrente = 0;
		float Consumo = 0;
		int pid = atoi(strpidnro);
		//printf("PIDNRO=%d\n",pid);
		if (pid > 0 )
		{
			//Captura dados
			if (LeDadoPID(pid,&Corrente,&Consumo) > 0)
			{
				//printf("Retornou: %d, %f , %f\n", pid,Corrente, Consumo);
				char Info[500];
				sprintf(Info,"Corrente Atual: %f\r Consumo Watts: %f\r",Corrente,Consumo);
				writesock(client,Info); 	
			}
			else
			{
				printf("Nao registrou Marcação\n");
			}
		}
}

void WebPage(int client, float Consumo, float Corrente)
{
		//printf("Retornou: %d, %f , %f\n", pid,Corrente, Consumo);
			char Info[500];
			writesock(client,"HTTP/1.1 200 OK");
			writesock(client,"Content-Type: text/html");
			writesock(client,"Connection: close");
			writesock(client,"Refresh: 30");
			writesock(client,"<!DOCTYPE HTML>");
			writesock(client,"<html>");
			writesock(client,"<head>");
			writesock(client,"<style>");
			writesock(client,"body  { margin: 0; background-color: gray;}");
			writesock(client,".HEADER  { padding: 20px; width:100%; background-color: #ccc;}");
			writesock(client,".Menu  {  float: left;  height:500px; width:15%; background-color: #ccc;}");
			writesock(client,".Lateral { float: left; text-align: right; width: 85%; height:500px; background-color: white; }");
			writesock(client,".Linha  {  text-align: left;  width: 100%; height:60px;}");
			writesock(client,".Rotulo { float: left; text-align: right; width: 200px; }");
			writesock(client,".Info { float: left; text-align: left; width: 300px; border-style: solid;}");
			writesock(client,"</style>");
			writesock(client,"<title>Maurinsoft</title>");
			writesock(client,"</head>");
			writesock(client,"<body>");
			writesock(client,"<div class='HEADER'>");
			writesock(client,"<t1><center>Maurinsoft<center></t1>");
			writesock(client,"</div>");
			writesock(client,"<div class='Menu'>");
			writesock(client,"<br/>");
			writesock(client,"</div>");
			writesock(client,"<div class='Lateral'>");
			writesock(client,"<div class='Linha'>");
			writesock(client,"</div>");
			writesock(client,"<div class='Linha'>");
			writesock(client,"<div class='Rotulo'>");
			writesock(client,"Consumo Watss:");
			writesock(client,"</div>");
			writesock(client,"<div class='Info'>");
			sprintf(Info,"%f",Consumo);
			writesock(client,Info);
			writesock(client,"</div>");
			writesock(client,"</div>");
			writesock(client,"<div class='Linha'>");
			writesock(client,"<div class='Rotulo'>");
			writesock(client,"Corrente Atual:");
			writesock(client,"</div>");
			writesock(client,"<div class='Info'>");
			sprintf(Info,"%f",Corrente);
			writesock(client,Info); 	
			writesock(client,"</div>");
			writesock(client,"</div>");
			writesock(client,"</body>");
			writesock(client,"</html>");			
}

//Analisa comandos
void AnalisaWEB(int client, char *Dados)
{
	//printf("Envia:%s\n",Dados);
	
	static const char delim[] = "\t\n:=\r";
	char *svptr;
	char  *strpid;
	char *strpidnro;
	//printf("Info:%s\n",Dados);
	//strpid= strtok_r(Dados,delim,&svptr);
	//strpidnro= strtok_r(NULL,delim,&svptr);
	//printf("PID:%s\n",strpidnro);
	
	//printf("%s\n",strConsumonro);	
	//Trabalha os nros
	float Corrente = 0;
	float Consumo = 0;
	//int pid = atoi(strpidnro);
	int pid = 1;
	//printf("PIDNRO=%d\n",pid);
	if (pid > 0 )
	{
		//Captura dados
		if (LeDadoPID(pid,&Corrente,&Consumo) > 0)
		{
			//WebPage(client, Consumo, Corrente);
			char Info[500];
			sprintf(Info,"Id:%d , Corrente: %f , Consumo: %f \n",pid,Corrente,Consumo);
			writesock(client,Info);
		}
		else
		{
			printf("Nao registrou Marcação\n");
		}
	
	}
}

//Retorna opcoes do MAN
void RetornaMANCLIENT(int Client)
{
	writesock(Client,"Relação de Comandos\n\n");
	writesock(Client,"MAN - Help de Comandos\n");
	writesock(Client,"LSTCON - Lista de Conexoes ativas\n");
	writesock(Client,"SAIR - Sair da aplicação\n");
}

void PromptClient(int Client)
{
	printf("\n \n $>");
}

/*Executa Processamento de comandos*/
void CMDEXECClient(int Client, char *BufferClient)
{	
	//printf("Comando: %s\n",BufferClient);
    if (strstr(BufferClient,"SAIR")>0)
	{
		printf("Conexao remota nao pode parar o servico!\n");
   		//flgAtivo = false; 
	}
	if (strstr(BufferClient,"MAN")>0)
	{
		RetornaMANCLIENT(Client);
		PromptClient(Client);
	}
	if (strstr(BufferClient,"id:")>0)
	{
		printf("ID: %s\n",BufferClient);  
        //writesock(Client,"OK!\n"); 		
		AnalisaID(BufferClient);
	}
	if (strstr(BufferClient,"pid=")>0) 
	{
		printf("PID= %s\n",BufferClient);  
        //writesock(Client,"Corrente Atual: 1.43\r Consumo Watts: 96.00\r"); 		
		AnalisaPID(Client,BufferClient);
	}
	if (strstr(BufferClient,"GET / ")>0)
	{
		printf("GET %s\n",BufferClient);   	
		AnalisaWEB(Client,BufferClient); 		
		
	}
	//PromptClient();
	memset(BufferClient, ' ', sizeof(BufferClient));
    sprintf(BufferClient,"\0"); /*Reseta Buffer*/
	
}


//Cliente de conexao multi thread
//Lê apenas um cliente, atendendo suas requisições
void *Le_Cliente(void *valor)
{
	
	int client_sock = (int*)valor;
	bool flgClient = true;
	while (flgClient==true)
	{
		//Receive a message from client
		read_size = recv(client_sock , client_message , 2000 , 0);
		if( read_size > 0 )
		{
			printf("Texto:%s\0",client_message); 
			CMDEXECClient(client_sock,client_message);
			close(client_sock);
			flgClient = false;
		}
		else
		{
			if(read_size == 0)
			{
				printf("Client disconnected");
				fflush(stdout);
				flgClient = false;
			}
			else if(read_size == -1)
			{
				perror("recv failed");
				flgClient = false;  
			}
		}
	}
}

//Processo single thread
void Le_SCliente(int client_sock)
{
	bool flgClient = true;
	while (flgClient==true)
	{
		//Receive a message from client
		read_size = recv(client_sock , client_message , 2000 , 0);
		if( read_size > 0 )
		{
			CMDEXECClient(client_sock,client_message);
			//Ler(client_message); 
			//printf("Fechou\n");
			close(client_sock); //Fecha conexao		
			flgClient = false;	 		
		}
		else
		{
			if(read_size == 0)
			{
				printf("Client disconnected");
				fflush(stdout);
				flgClient = false;
			}
			else if(read_size == -1)
			{
				perror("recv failed");
				flgClient = false;  
			} 
		}
	}
	printf("Saiu do while\n");
}


//Espera o clinte conectar
void Espera_Cliente()
{
	//Accept and incoming connection
	printf("Esperando Conexao\n");
	c = sizeof(struct sockaddr_in);
	while(flgAtivo==1)
	{		 
		//accept connection from an incoming client
		client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
		if (client_sock < 0)
		{
			perror("accept failed");
			//return 1;
			flgAtivo = false;
		}
		if (client_sock>0)
		{
			  
			//Le_SCliente(client_sock);
			th_execute = pthread_create(&ClientesTh[0],NULL,Le_Cliente,(void *)client_sock);
			if(th_execute) 
			 {
				 fprintf(stderr,"Error - pthread_create() return code: %d\n",th_execute);
				 exit(EXIT_FAILURE); 
			 }
		}	
	}
}
 
void *Le_Ethernet(void *valor)
{
  //Posteriormente abre o socket
  Espera_Cliente();
  pthread_exit(0); /* exit */
}

void Leituras()
{
	//Le_Ethernet();
	Le_Teclado();
	//printf(".");
}
 
void Looping()
{
	Leituras();
}

int main(int argc , char *argv[])
{     
	//Realiza configuração de aplicacao
    Setup();
	
	//Simula operacao de looping
    while(flgAtivo==true)     
    {
		//Chamada da função Looping
		Looping();
    } 
    return( 0);
}
