#include "aurrasd.h"

#define MAX_READ_BUFFER 2048
#define MAX_BUF_SIZE 1024
#define SENDTOCLIENT "sendToClient"
#define FIFO "fifo"
#define CONFIG_PATH "../etc/aurrasd.conf"
#define NUMBER_OF_FILTERS 5
#define STRING_SIZE 200

void* create_shared_memory(size_t size) {
    // Criar memória com permissão read and write
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED;
    return mmap(NULL, size, protection, visibility, -1, 0);
}



//struct utilizada para armazenar a informação dos filtros
struct config {
    char* filtersPath;                //Pasta onde estão os filtros (bin/aurras-filters)
    int* runningProcesses;            //processos a correr de momento
    char** execFiltros;               //aurrasd-gain-double
    int* maxInstancias;               // 2
    char** identificadorFiltro;       //alto
};

//Função que tem por objetivo receber e manipular sinais
void handler(int signum){
    switch (signum)             //optei por um switch pq assim ficam todos os sinais neste handler
    {
    case SIGINT:
        unlink(FIFO); 
        unlink(SENDTOCLIENT);
        printf("Fechando pipes e terminando servidor\n");
        exit(0);
        break;
    case SIGPIPE:
        printf("[SIGPIPE] O cliente fechou o terminal de escrita\n");
        break;
    case SIGUSR1:
        printf("Começou processo\n");
        return;
        break;
    case SIGUSR2:
        printf("Terminou processo\n");;
        return;
    default:
        break;
    }
    
}


//------------------------------FUNÇÃO AUXILIAR (readline) para o serverConfig -----------------------------------------------

int contador = 0;

int readch(int fd, char* buf){
    contador ++;
    return read(fd,buf,1);
}

//Funçao que lê linha a linha o conteudo de um ficheiro
int readline(int fd, char* buf, size_t size){
    char* tmp = malloc(sizeof(char*));
    int curr = 0;
    int read = 0;
    while ((read = readch(fd,tmp))>0 && curr < size)
    {
        buf[curr] = tmp [read-1];
        curr++;
        if (tmp[read-1]=='\n')
            return -1;
    }
    return 0;
}

//-------------------------------------------------------------------------------

//Função que faz o povoamento da struct config com todas as informações provenientes do ficheiro aurrasd.conf (cada informação tem um campo) 
int serverConfig (char* path, CONFIG cfg) {    
    char buffer[MAX_BUF_SIZE];
    int file_open_fd = open (path, O_RDONLY);    //começar por abrir o ficheiro para leitura
    if (file_open_fd < 0) {
        perror ("[Erro] Ficheiro de configuração inválido :");
        _exit(-1);
    }
    for (int i=0; i<NUMBER_OF_FILTERS; i++) {
        readline(file_open_fd, buffer, BUFFERSIZE);
        char * substr = strtok(buffer, " ");
        cfg -> identificadorFiltro[i] = malloc(sizeof(char)*100);
        strcpy((char*) cfg -> identificadorFiltro[i],substr);
        substr = strtok(NULL, " ");
        cfg -> execFiltros[i] = malloc(sizeof(char)*100);
        strcpy ((char*) cfg->execFiltros[i], substr);
        substr = strtok(NULL, " ");
        cfg->maxInstancias[i] = atoi(substr);
    }
    for (int i = 0; i < NUMBER_OF_FILTERS; i++)
        printf("%s %s: %d\n",cfg->identificadorFiltro[i],cfg->execFiltros[i], cfg->maxInstancias[i]);
    return 0;
}


//Verifica se um filtro existe e devolve o seu indice na struct config
int filtro_existente (char* nomeFiltro, CONFIG cfg) {
    for (int i = 0; i<NUMBER_OF_FILTERS; i++) {
        if (strcmp (cfg->execFiltros[i], nomeFiltro) == 0) {
            return i;           //caso encontre, retornamos o indice em que encontramos
        }
    } 
    return -1;  //caso dê erro retornamos -1
}


//Verifica se ha menos pedidos a correr do que o maximo permitido para um dado filtro
int filtro_permitido (int idx_filtro, CONFIG cfg) {
    //return 1 se for possivel; return 0 se nao or possivel
    if (cfg->maxInstancias[idx_filtro] > cfg->runningProcesses[idx_filtro]) {
        return 1;
    }
    return 0;
}

//Função para encontrar o indice das informaçoes de um filtro na struct config
int encontraIndice (char* identificador, struct config *cfg) {
    for (int i=0; i<5; i++) {
        if (strcmp (cfg->identificadorFiltro[i], identificador) == 0) {
            return i;
        }
    }
    return -1;
}

//Recebe ../bin/aurrasd-filters/aurrasd-echo e procura em execFiltro aurrasd-echo e devolve o indice
int encontraIndicePath (char* path, struct config *cfg) {
    char* token;
    token = strtok(path,"/");
    for (int i = 0; i < 3; i++) token = strtok(NULL,"/");
    for (int i=0; i<5; i++) {
        if (strcmp (cfg->execFiltros[i], token) == 0) {
            return i;
        }
    }
    return -1;
}

//Cria uma string com tamanho len aleatória
char* randomString(int len) {
    char* s = malloc(sizeof(char)*len+1);
    char letters[] = "abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < len; ++i) {
        s[i] = letters[rand() % (sizeof(letters) - 1)];
    }
    s[len] = 0;
    return s;
}

//Função que aplica os filtros (provenientes do switch transform do comando ./aurras) ao ficheiro de input
void executaTarefa (int n_filtros,char ** filtros_args, char * input_file, char * output_name, CONFIG cfg) {
    char * path = malloc(sizeof(char)*100);
    //strcat(path,"./"); vai ser util dps para por em /tmp ou ot local
    strcat(path,output_name);
    int inputFile_fd = open(input_file, O_RDONLY, 0666);
    if(inputFile_fd == -1){
        perror("ficheiro inexistente");
        sendTerminate();
        return;
    }
    for (int i = 0; filtros_args[i] != NULL ; i++)
    {
        printf("%d: %s\n",i,filtros_args[i]);  
    }
    int outputFile_fd = open(path, O_CREAT | O_RDWR, 0666);
    char* random = randomString(5);
    pid_t pid = getpid();
    if(fork() == 0){
        for (int i = 0; i < n_filtros; i++)
        {
            if(fork() == 0){
                if(i == 0) {
                    dup2(inputFile_fd,STDIN_FILENO);
                    if(i == n_filtros -1){
                        dup2(outputFile_fd,STDOUT_FILENO);
                    } else{
                        char str[30];
                        sprintf(str,"../tmp/%s%d",random,i);
                        creat(str,0777);
                        outputFile_fd = open(str, O_CREAT | O_RDWR, 0666);
                        dup2(outputFile_fd,STDOUT_FILENO);
                    }
                }
                else if(i == n_filtros -1){
                    char tmp_output_path[30];
                    if(i>1) {
                        sprintf(tmp_output_path,"../tmp/%s%d",random,i-2);
                        remove(tmp_output_path);
                    }
                    sprintf(tmp_output_path,"../tmp/%s%d",random,i-1);
                    outputFile_fd = open(tmp_output_path,O_RDWR, 0666);
                    dup2(outputFile_fd,STDIN_FILENO);
                    outputFile_fd = open(path, O_CREAT | O_RDWR, 0666);
                    dup2(outputFile_fd,STDOUT_FILENO);
                } else {
                    char tmp_output_path[30];
                    if(i>1) {
                        sprintf(tmp_output_path,"../tmp/%s%d",random,i-2);
                        remove(tmp_output_path);
                    }
                    sprintf(tmp_output_path,"../tmp/%s%d",random,i-1);
                    outputFile_fd = open(tmp_output_path,O_RDWR, 0666);
                    dup2(outputFile_fd,STDIN_FILENO);
                    char str[30];
                    sprintf(str,"../tmp/%s%d",random,i);
                    creat(str,0777);
                    outputFile_fd = open(str, O_RDWR, 0666);
                    dup2(outputFile_fd,STDOUT_FILENO);
                }
                kill(pid,SIGUSR1);
                if(execl(filtros_args[3+i],filtros_args[3+i],NULL) == -1){
                    perror("Erro ao fazer exec");
                    _exit(0);
                }
            }
            wait(NULL);
            if(i == n_filtros-1){
                char str[30];
                sprintf(str,"../tmp/%s%d",random,n_filtros-2);
                remove(str);
                printf("Removido /tmp/%s%d\n",random,n_filtros-1);
                kill(pid,SIGUSR2);
            }
        } 
        for (int i = 0; i < numeroFiltros(filtros_args); i++)
        {
            cfg ->runningProcesses[encontraIndicePath(filtros_args[3+i],cfg)]++;
        }
        _exit(0);  
    } 
}


int main(int argc, char* argv[]){
    if(signal(SIGINT, handler) == SIG_ERR){
        perror("Erro de terminação");
    }
    signal(SIGPIPE,handler);
    signal(SIGUSR1,handler);
    signal(SIGUSR2,handler);
    if (argc < 3) {
        perror("Formato de execução incorreto!");
        return -1;
    }
    CONFIG cfg = malloc(sizeof(struct config));
    initializeConfig(cfg);
    serverConfig(argv[1], cfg);
    cfg ->filtersPath = argv[2];
    printf("Pasta dos filtros: %s\n",cfg->filtersPath);
    char buffer[1024];
    int bytes = 0;
    mkfifo(FIFO,0777);
    mkfifo(SENDTOCLIENT,0777);
    int fifo_fd = open(FIFO, O_RDONLY);
    while(1){
        while ((bytes = read(fifo_fd, buffer, BUFFERSIZE)) > 0) {
            buffer[bytes]='\0';
            printf("Lido: %s\n",buffer);
            if(strcmp(buffer,"status ") == 0){
                printf("Recebido [STATUS]\n");
                sendStatus(cfg);
            }else {
                char** args = splitWord(buffer,cfg);
                if(args == NULL){
                    sendInvalidCommand();
                }else if(strcmp(args[0],"transform") == 0){
                    printf("Recebido [Transform]\n");
                    printf("Numero de filtros: %d\n",numeroFiltros(args));
                    sendProcessing();
                    executaTarefa(numeroFiltros(args),args,args[1],args[2],cfg);
                } else{
                    sendTerminate();
                }
            }
        }
    }
    unlink(FIFO);
    unlink(SENDTOCLIENT);
    return 0;
}

//Faz o parsing do argumento enviado pelo cliente e obtem-se um array de strings do tipo: [[transform], [samples/sample-1.m4a], [output.m4a], [../bin/aurrasd-filters/aurras-gain-double]]
char** splitWord(char* str,CONFIG cfg){
    char** args = malloc(sizeof(char)*10*100);
    args[0] = malloc(sizeof(char)*STRING_SIZE);
    char* token;
    token = strtok(str," ");
    strcpy(args[0],token);
    int i = 1;
    int index = 0;
    while ((token = strtok(NULL," ")) != NULL){
        args[i] = malloc(sizeof(char)*STRING_SIZE);
        if (i > 2){
            index = encontraIndice(token,cfg);
            if(index == -1) return NULL;
            fflush(stdout);
            if(index >= 0 && index < NUMBER_OF_FILTERS) {
                strcat(args[i],cfg->filtersPath);
                strcat(args[i],"/");
                strcat(args[i],(char*) (cfg->execFiltros[index]));
            }
        }else
            strcpy(args[i],token);
        i++;
    }
    if(i < 4) return NULL;
    for (int i = 0; args[i] != NULL ; i++)
    {
        printf("%d: %s\n",i,args[i]);   
    }
    return args;
}


//Calcula o numero de filtros que o cliente pretende aplicar
int numeroFiltros(char** args){
    int i = 0;
    for (i = 0; args[i]!=NULL; i++);
    return i - 3;
}

//Envia uma mensagem de processing ao cliente
void sendProcessing(){
    int sendToClient_fd = open(SENDTOCLIENT, O_WRONLY);
    write(sendToClient_fd, "processing", 10);
    close(sendToClient_fd);
    printf("Enviado [PROCESSING]\n");
    fflush(stdout);
}


//Envia uma mensagem de erro ao cliente devido a um comando inválido
void sendInvalidCommand(){
    int sendToClient_fd = open(SENDTOCLIENT, O_WRONLY);
    write(sendToClient_fd, "Comando inválido\n", 18);
    close(sendToClient_fd);
    printf("Enviado [INVALID]\n");
    fflush(stdout);
}

//Envia uma mensagem de terminação ao cliente
void sendTerminate(){
    int sendToClient_fd = open(SENDTOCLIENT, O_WRONLY);
    write(sendToClient_fd, "none", 4);
    close(sendToClient_fd);
    printf("Enviado [NONE]\n");
    fflush(stdout);
}

//Funçao para enviar o status dos processos para o cliente e notificá-lo do que esta a decorrer
void sendStatus(CONFIG cfg){
    int sendToClient_fd = open(SENDTOCLIENT, O_WRONLY);
    char string[200];
    for (int i = 0; i < NUMBER_OF_FILTERS; i++)
    {
        sprintf(string, "filter %s: %d/%d (running/max)\n",cfg -> identificadorFiltro[i] ,cfg->runningProcesses[i], cfg->maxInstancias[i]);
        write(sendToClient_fd, string, strlen(string));
        printf("Escrito %s para o cliente\n",string);
        fflush(stdout);
    }
    pid_t pid = getpid();
    sprintf(string,"pid: %d\n",pid);
    write(sendToClient_fd,string,strlen(string));
    printf("Enviado status\n");
    close(sendToClient_fd);
}

//Função para alocar o espaço necessario para a struct config 
void initializeConfig(CONFIG cfg){
    cfg -> identificadorFiltro = malloc(sizeof(char*)*NUMBER_OF_FILTERS);
    cfg -> execFiltros = malloc(sizeof(char*)*NUMBER_OF_FILTERS);
    cfg -> maxInstancias = malloc(sizeof(int)*NUMBER_OF_FILTERS);
    cfg -> runningProcesses = malloc(sizeof(int)*NUMBER_OF_FILTERS);
}
