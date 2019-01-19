#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/stat.h>
#include<netinet/in.h>
#include <fcntl.h>
#include <sys/time.h>


#define IP_ADDR 0x7f000001
#define PORT    8888 /* Port to listen on */

int playMasterMind(int sockfd);

int main(int argc, char** argv)
{
	struct sockaddr_in address = {0};

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		perror("socket() fail...\n");
		return -1;
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(PORT);
	address.sin_addr.s_addr = htonl(IP_ADDR);

	if ( connect(sockfd, (struct sockaddr*)&address, sizeof(address)) < 0 )
	{
		perror("connect\n");
		return -1;
	}
	playMasterMind(sockfd);
	close(sockfd);
	return 1;
}



int playMasterMind(int sockfd)
{
	fd_set readfds;
	int maxfd;

	printf(" --- Welcome to MASTERMIND ---\n");
	char clientNumbers[4];
	int received_int = 0;
	int boolGame;
	int pgiaGame;
	while(1)
	{

		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);
		FD_SET(sockfd, &readfds);

		maxfd = sockfd;

		printf("guess 4 numbers:\n");

		if (select(maxfd + 1, &readfds, NULL, NULL, NULL) == -1) {
			perror("select");
			return -1;
		}
		//check if something is in the STDIN
		if(FD_ISSET(STDIN_FILENO, &readfds)){
			scanf("%s",clientNumbers);
			if( send(sockfd, clientNumbers, sizeof(clientNumbers), 0) < 0 )
			{
				perror ("Fail sending data...\n");
				return -1;
			}
		}else{
			received_int = read(sockfd, &boolGame, sizeof(int));
			if (received_int <= 0)
			{
				perror(" --- SERVER CLOSED CONNECTION ---\n");
				close(sockfd);
				return -1;
			}
			printf("bool: %d\n", boolGame);

			received_int = read(sockfd, &pgiaGame, sizeof(int));
			if (received_int <= 0)
			{
				perror(" --- SERVER CLOSED CONNECTION ---\n");
				close(sockfd);
				return -1;
			}
			printf("pgia: %d\n", pgiaGame);

			if(boolGame == 4)
			{
				printf("You guessed correct! \n");
				printf(" --- GAME OVER ---\n");
				close(sockfd);
				return 1;
			}
			received_int = 0;
			boolGame = 0;
			pgiaGame = 0;
		}
	}
	//get server respond
	//check response
	return 0;
}
