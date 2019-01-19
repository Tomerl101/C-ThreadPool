#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset() */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "thpool.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

#define IP_ADDR 0x7f000001
#define PORT 8888 /* Port to listen on */
#define QUEUE_LEN   10  /* Passed to listen() */

struct Game{
 int boolInGame;
 int pgiaInGame;
};

struct lastGuess {
    pthread_t  threadId;
    char serverNumbers[5];
    char lastGuess[5];
};
/*  auxiliary functions  */
void handle(int newsock, int filefd, int req_type);
int listenToStdin(void* thpool);
void* masterMind(void* thread_pool);
void* getAdminInput(void* thpool);

struct lastGuess *getLastGuess;
int numOfThreads;
/* main */
int main(int argc, char *argv[])
{
  int sockfd, filefd;
  struct sockaddr_in address, *res;
  int reuseaddr = 1; /* True */
  int getConnections = 1;
  fd_set readfds;
  int maxfd;

  if(argv[1] == NULL)
  {
      printf("Error:please enter number of threads...\n");
      return -1;
  }
  /* convert num of threads to int */
  numOfThreads = strtol(argv[1], NULL, 10);

  getLastGuess = malloc(numOfThreads * sizeof(struct lastGuess));
  if(getLastGuess == NULL)
  {
	perror("Could not allocate memory for lastGuess...\n");
    return -1;
  }
  for(int i = 0 ; i < numOfThreads ; i++)
    getLastGuess[i].threadId =  0;

  printf("Making threadpool with %d threads\n" ,numOfThreads);
  ThreadPoolManager thpool;
  if( ThreadPoolInit(&thpool,numOfThreads) == -1)
	{

		printf("ThreadPoolInit() faild...\n");
		return -1;
	}

  /* Get the address info */
  /* fill memory with a constant byte */
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(IP_ADDR);
  address.sin_port = htons(PORT);

  /* Create the socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    perror("socket");
    return 1;
  }

  /* Enable the socket to reuse the address */
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1) {
    perror("setsockopt");
    return 1;
  }

  /* Bind to the address */
  if (bind(sockfd, (struct sockaddr*)&address, sizeof(address)) == -1) {
    perror("bind");
    return 1;
  }

  /* Listen */
  if (listen(sockfd, QUEUE_LEN) == -1) {
    perror("listen");
    return 1;
  }

  /* Main loop */
  while (1) {
    struct sockaddr_in their_addr;
    socklen_t size = sizeof(struct sockaddr_in);

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sockfd, &readfds);

    maxfd = sockfd;

    //printf("before SELECT\n");
    if (select(maxfd + 1, &readfds, NULL, NULL, NULL) == -1) {
        perror("select");
        return -1;
    }
    //check if something is in the STDIN
    if(FD_ISSET(STDIN_FILENO, &readfds)){
        char command[10];
        fgets(command,10,stdin);

        if (strcmp(command, "list\n") == 0)
        {
          printf("start LIST command.\n");
          for(int i = 0 ; i < numOfThreads ; i++)
          {
              if(getLastGuess[i].threadId == 0)
                break;

             printf("Game %d - Number %s, Last Guess: %s\n",i,getLastGuess[i].serverNumbers,getLastGuess[i].lastGuess);
          }
        }
        else if (strcmp(command, "quit\n") == 0)
        {
           printf("Close all connections...\n");
           ThreadPoolManagerdestroy(&thpool);
            for(int i = 3 ; i < maxfd ; i++)
            {
                close(i);
            }
           break;
        }
        else /* default: */
        {
            printf("Unknown command. Please enter valid command..\n");
        }
        continue;
     }

    int newsock = accept(sockfd, (struct sockaddr*)&their_addr, &size);
    if (newsock == -1) {
      perror("accept");
      return -1;
    }
    //save the max fd;
    if(newsock > maxfd)
    {
        maxfd = newsock;
    }
    printf("Got a connection from %s on port %d\n", inet_ntoa(their_addr.sin_addr),htons(their_addr.sin_port));
    int* sock_p = (int*)malloc(sizeof(int));
    if(sock_p == NULL)
    {
        perror("malloc failed...\n");
        return -1;
    }
    *sock_p = newsock;
    /* add new task to every new client connected to the server */
	  ThreadPoolInsertTask(&thpool, masterMind, (void*)sock_p);
  }
  free(getLastGuess);
  close(sockfd);
  sleep(1);
  printf("GOODBYE...\n");
  return 0;
}

/* master mind game loop*/
 void* masterMind(void* arg){
   int sock = *(int*)arg;
   int i=0, j=0;
   int nrecv;
   char serverNumbers[5];
   char clientNumbers[5];
   struct Game gameResult;

   serverNumbers[4] = '\0';
   clientNumbers[4] = '\0';

   gameResult.boolInGame = 0;
   gameResult.pgiaInGame = 0;

   //generate new seed
   srand(time(NULL));
    //generate 4 random number
    printf(" --- START GAME [#thread: %d]--- \n",(int)pthread_self());
    while(1)
    {
        for(i=0;i<4;i++)
        {
            /* generate numbers between 1-9 */
            serverNumbers[i]  = '1' + (random() % 9);
        }

        /* check for duplicate numbers - if exist then generate new numbers */
        if((serverNumbers[0] != serverNumbers[1]) && (serverNumbers[0] != serverNumbers[2])  && (serverNumbers[0]!= serverNumbers[3])
        && (serverNumbers[1]!= serverNumbers[2]) && (serverNumbers[1]!= serverNumbers[3])
        && (serverNumbers[2] !=serverNumbers[3]))
        {
            break;
        }
    }
    printf("server numbers:[%c|%c|%c|%c]\n",serverNumbers[0],serverNumbers[1],serverNumbers[2],serverNumbers[3]);

    //2.wait for client guess
    while(1)
    {
        if ( nrecv = recv(sock, clientNumbers, sizeof(clientNumbers), 0) <= 0)
        {
            printf("recv failed...\n");
            break;
        }

        for(int i =0 ; i< numOfThreads ; i++)
        {
            if(getLastGuess[i].threadId == 0)
            {
                getLastGuess[i].threadId = pthread_self();
                strcpy(getLastGuess[i].lastGuess, clientNumbers);
                strcpy(getLastGuess[i].serverNumbers, serverNumbers);
                break;
            }
            else if( getLastGuess[i].threadId == pthread_self())
            {
                strcpy(getLastGuess[i].lastGuess, clientNumbers);
                break;
            }
        }


        printf("Client Numbers are:%s\n --------------\n", clientNumbers);
        sleep(1);

        //3. check client guess
        for(i=0; i < 4 ; i++)
        {
            for(j=0 ; j<4 ; j++)
            {
                if ( (i == j) && (serverNumbers[j] == clientNumbers[i]) )
                    gameResult.boolInGame ++;
                else if (serverNumbers[j] == clientNumbers[i])
                {
                    gameResult.pgiaInGame++;
                }
            }
        }
        if( (write(sock, &gameResult.boolInGame, sizeof(int))) < 0 )
        {
            perror("write() error...\n");
            return NULL;
        }
        if( (write(sock, &gameResult.pgiaInGame, sizeof(int))) < 0 )
        {
            perror("write() error...\n");
            return NULL;
        }

        /* if client guess all the numbers break from loop and end session*/
        if (gameResult.boolInGame == 4)
            break;
        /* reset turn result */
        nrecv = 0;
        gameResult.boolInGame = 0;
        gameResult.pgiaInGame = 0;
    }
    printf(" --- END GAME [#thread: %d]--- \n",(int)pthread_self());
    close(sock);
}
