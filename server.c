#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> 
#include <string.h>
#include "timer.h"
#include "common.h"
#define NUM_STR 1024
#define STR_LEN 1000

int thread_count;  
char **theArray; 
unsigned int* seed;
pthread_mutex_t mutex;
double start, finish, elapsed[1000]; 
int i;


void *Operate(void* rank);  /* Thread function */


void *ServerEcho(void *args)
{
    int clientFileDescriptor=(int)args;
    char str[1000];

    read(clientFileDescriptor,str,1000);
    //printf("reading from client:%s\n",str);

    ClientRequest resc;
    ParseMsg(str, &resc);

    
    char buffer[STR_LEN]; // buffer to read and write
    char bufferRead[STR_LEN]; // buffer to read and write

    int pos = resc.pos;
    int randNum = resc.is_read; 
    sprintf(buffer, resc.msg); // prepare the message

    GET_TIME(start); 
    pthread_mutex_lock(&mutex); 
    // write the content

    if (randNum != 1){ //write operation
        setContent(buffer, pos, (char**)theArray); // defined in "common.h"
        getContent(bufferRead, pos, (char**)theArray); // defined in "common.h"
    }else{
        getContent(bufferRead, pos, (char**)theArray); // defined in "common.h"
    }
    
    pthread_mutex_unlock(&mutex);
    GET_TIME(finish); 
    elapsed[i] = finish - start; 


    write(clientFileDescriptor,bufferRead,1000);
    close(clientFileDescriptor);
    return NULL;
}




int main(int argc, char* argv[])
{
    struct sockaddr_in sock_var;
    int serverFileDescriptor=socket(AF_INET,SOCK_STREAM,0);
    int clientFileDescriptor;

    /* Command line parameter count should be 4 */
    if (argc != 4){ 
        fprintf(stderr, "usage: %s <Size of theArray_ on server> <server ip> <server port>\n", argv[0]);
        exit(0);
    }
    int NUM_STR_ = strtol(argv[1], NULL, 10);


    /* Get number of threads from command line */
    thread_count = 1000;
    
    /* Intializes random number generators */
    seed = malloc(thread_count*sizeof(unsigned int));
    for (i = 0; i < thread_count; i++)
        seed[i] = i;
    
    /* Create the memory and fill in the initial values for theArray */
    theArray = (char**) malloc(NUM_STR * sizeof(char*));
    for (i = 0; i < NUM_STR; i ++){
        theArray[i] = (char*) malloc(STR_LEN * sizeof(char));
        sprintf(theArray[i], "theArray[%d]: initial value", i);
    }


    pthread_t t[1000];
    sock_var.sin_addr.s_addr=inet_addr(argv[2]);
    sock_var.sin_port=strtol(argv[3], NULL, 10);

    pthread_mutex_init(&mutex, NULL);

    sock_var.sin_family=AF_INET;
    if(bind(serverFileDescriptor,(struct sockaddr*)&sock_var,sizeof(sock_var))>=0)
    {
        printf("socket has been created\n");
        listen(serverFileDescriptor,1000); 
        while(1)        //loop infinity
        {
            for(i=0;i<1000;i++)     
            {
                clientFileDescriptor=accept(serverFileDescriptor,NULL,NULL);
                //printf("Connected to client %d\n",clientFileDescriptor);
                pthread_create(&t[i],NULL,ServerEcho,(void *)(long)clientFileDescriptor);
                
            }
            

            for (i=0;i<1000;i++) {
                pthread_join(t[i], NULL); 
            } 
            saveTimes(elapsed, 1000);

        }
        
        close(serverFileDescriptor);
    }
    else{
        printf("socket creation failed\n");
    }

    pthread_mutex_destroy(&mutex);
    free(t);
    free(seed);
    for (i=0; i<NUM_STR; ++i){
        free(theArray[i]);
    }
    free(theArray);


    return 0;
}