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
pthread_mutex_t mutex[1000];

void *Operate(void* rank);  /* Thread function */


typedef struct {
    int readers;
    int writer;
    pthread_cond_t readers_proceed;
    pthread_cond_t writer_proceed;
    int pending_writers;
    pthread_mutex_t read_write_lock;
} mylib_rwlock_t;

mylib_rwlock_t rwlock;

void mylib_rwlock_init (mylib_rwlock_t *l) {
    l -> readers = l -> writer = l -> pending_writers = 0;
    pthread_mutex_init(&(l -> read_write_lock), NULL);
    pthread_cond_init(&(l -> readers_proceed), NULL);
    pthread_cond_init(&(l -> writer_proceed), NULL);
}

void mylib_rwlock_rlock(mylib_rwlock_t *l) {
/* if there is a write lock or pending writers, perform
condition wait, else increment count of readers and grant
read lock */
    pthread_mutex_lock(&(l -> read_write_lock));
    while ((l -> pending_writers > 0) || (l -> writer > 0))
        pthread_cond_wait(&(l -> readers_proceed),
        &(l -> read_write_lock));
        l -> readers ++;
        pthread_mutex_unlock(&(l -> read_write_lock));
}

void mylib_rwlock_wlock(mylib_rwlock_t *l) {
/* if there are readers or writers, increment pending
writers count and wait. On being woken, decrement pending
writers count and increment writer count */
    pthread_mutex_lock(&(l -> read_write_lock));
    while ((l -> writer > 0) || (l -> readers > 0)) {
        l -> pending_writers ++;
        pthread_cond_wait(&(l -> writer_proceed),
        &(l -> read_write_lock));
        l -> pending_writers --;
    }
    l -> writer ++;
    pthread_mutex_unlock(&(l -> read_write_lock));
}

void mylib_rwlock_unlock(mylib_rwlock_t *l) {
/* if there is a write lock then unlock, else if there are
read locks, decrement count of read locks. If the count is 0
and there is a pending writer, let it through, else if there
are pending readers, let them all go through */
pthread_mutex_lock(&(l -> read_write_lock));
    if (l -> writer > 0)
        l -> writer = 0;
    else if (l -> readers > 0)
        l -> readers --;
    if (l -> readers > 0)
        pthread_cond_broadcast(&(l -> readers_proceed));
    else if ((l -> readers == 0) && (l -> pending_writers > 0))
        pthread_cond_signal(&(l -> writer_proceed));
    else if ((l -> readers == 0) && (l -> pending_writers==0))
        pthread_cond_broadcast(&(l -> readers_proceed));
        pthread_mutex_unlock(&(l -> read_write_lock));
}



void *ServerEcho(void *args)
{
    int clientFileDescriptor=(int)args;
    char str[1000];

    read(clientFileDescriptor,str,1000);

    ClientRequest resc;
    ParseMsg(str, &resc);

    //printf("the message decoded is : %d \n",resc.msg);
    //printf("the message position is : %s \n",resc.pos);
    //printf("the message is_read is : %d \n",resc.is_read);
    
    char buffer[STR_LEN]; // buffer to read and write
    char bufferRead[STR_LEN]; // buffer to read and write
    // Find a random position in theArray for read or write
    int pos = resc.pos;
    int randNum = resc.is_read;  // write with 10% probability
    sprintf(buffer, resc.msg); // prepare the message



    
    // write the content
    if (randNum != 1){ //write operation
        mylib_rwlock_wlock(&rwlock);
        setContent(buffer, pos, (char**)theArray); // defined in "common.h"
        getContent(bufferRead, pos, (char**)theArray); // defined in "common.h"
        mylib_rwlock_unlock(&rwlock);
    }else{
        mylib_rwlock_rlock(&rwlock);
        getContent(bufferRead, pos, (char**)theArray); // defined in "common.h"
        mylib_rwlock_unlock(&rwlock);
    }
    




    write(clientFileDescriptor,bufferRead,1000);
    close(clientFileDescriptor);
    return NULL;
}




int main(int argc, char* argv[])
{
    struct sockaddr_in sock_var;
    int serverFileDescriptor=socket(AF_INET,SOCK_STREAM,0);
    int clientFileDescriptor;
    int i;
    double start, finish, elapsed[1000]; 

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

    mylib_rwlock_init(&rwlock);

    pthread_mutex_t* mutex[NUM_STR_];

    sock_var.sin_family=AF_INET;
    if(bind(serverFileDescriptor,(struct sockaddr*)&sock_var,sizeof(sock_var))>=0)
    {
        printf("socket has been created\n");
        listen(serverFileDescriptor,1000); 
        while(1)        //loop infinity
        {
            for(i=0;i<1000;i++)      //can support 20 clients at a time
            {
                clientFileDescriptor=accept(serverFileDescriptor,NULL,NULL);
                printf("Connected to client %d\n",clientFileDescriptor);
                GET_TIME(start); 
                pthread_create(&t[i],NULL,ServerEcho,(void *)(long)clientFileDescriptor);
            }
            for (i=0;i<1000;i++) {
                pthread_join(t[i], NULL); 
            }
                
            
            GET_TIME(finish); 
            elapsed[i] = finish - start;  
            saveTimes(&elapsed, 1000);
        }
        close(serverFileDescriptor);
    }
    else{
        printf("socket creation failed\n");
    }

    free(t);
    free(seed);
    for (i=0; i<NUM_STR; ++i){
        free(theArray[i]);
    }
    free(theArray);


    return 0;
}