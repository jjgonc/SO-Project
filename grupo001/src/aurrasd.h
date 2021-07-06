#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>


#define BUFFERSIZE 1024

typedef struct config *CONFIG;


int main(int, char**);
int readch(int, char*);
int readline(int, char*, size_t);
int serverConfig (char*, CONFIG);
void execTarefa (char*, char*, CONFIG );
void sendStatus(CONFIG);
void sendTerminate();
void sendProcessing();
void sendInvalidCommand();
void initializeConfig(CONFIG);
char** splitWord(char* ,CONFIG);
int numeroFiltros(char** );
void handler(int);