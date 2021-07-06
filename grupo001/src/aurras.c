#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFERSIZE 1024
#define SENDTOCLIENT "sendToClient"
#define FIFO "fifo"

void sendPid(int fifo_fd);
void leResposta();

void handler (int signum) {
    switch (signum)
    {
    case SIGPIPE:
        printf("Sigpipe!!!!\n");
        break;
    default:
        break;
    }
}

int main(int argc, char* argv[]){
    if(signal(SIGUSR1, handler) == SIG_ERR){
        perror("Sinal 1");
    }
    if (argc < 2) {
        printf("./aurras status\n");
        printf("./aurras transform input-filename output-filename filter-id-1 filter-id-2 ...\n");
        return -1;
    }
    int fifo_fd = open(FIFO,O_WRONLY);
    if(fifo_fd == -1){
        puts("[Error] Server not iniciated!\n");
        return 0;
    }
    char* output = malloc(sizeof(char)*500);
    for (int i = 1; i < argc; i++){
        strcat(output,argv[i]);
        strcat(output," ");
    }
    write(fifo_fd,output,strlen(output));
    leResposta(fifo_fd);
    return 0;
}

void sendPid(int fifo_fd){
    pid_t pid = getpid();
    char str1[10];
    sprintf(str1,"%d;",pid);
    write(fifo_fd,str1,strlen(str1));
}

void leResposta(){
    int readFifo_fd = open(SENDTOCLIENT,O_RDONLY);
    int bytes = 0;
    char buffer[BUFFERSIZE];
    while ((bytes = read(readFifo_fd, buffer, BUFFERSIZE)) > 0) {
            buffer[bytes] = '\0';
            if(strcmp(buffer,"none") == 0){
            } else if(strcmp(buffer,"processing") == 0){
                printf("processing\n");
            }else {
                write(STDOUT_FILENO, buffer, strlen(buffer));
            }          
    }
}