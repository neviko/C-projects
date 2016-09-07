#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "slist.h"

#define TRUE 1
#define FALSE 0

int isPosNumeric(const char *str);

typedef struct client
{
	char buffer[4096];
	struct sockaddr_in clientAddr;

} client_t;

void toUpperCase(char *s)
{
	int i=0;
	while(s[i]!='\0')
	{
		if(s[i]>96 && s[i]<123)// if this is a word in small latter char
		s[i]-=32;
		i++;
	}
}

int main(int argc, char*args[])
{
	int myRcvSocket;
	struct sockaddr_in  myAddr,clientAddr;
	int i, addrSize, bytesRcv, selectRC,SERVER_PORT;
	fd_set readfds,writefd;
	struct timeval timeout={1000,0};
	char buffer[30],incomingAddr[30];
	slist_t *list;
	slist_init(list);// init the list

	
	if(argc!=2)
	{
	 	perror("Please Enter Port number\n");
	 	exit(-1);
	}

	if(isPosNumeric(args[1])==FALSE)
	{
		 perror("port number cant have latters or negative numbers\n");
		 exit(-1);
	}

	/* create socket */
	myRcvSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (myRcvSocket < 0) 
	{
		perror("could not open server socket\n");
		exit(-1);
	}

	SERVER_PORT=atoi(args[1]);

	/* setup my server address */
	memset(&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myAddr.sin_port = htons((unsigned short) SERVER_PORT);

	/* bind my socket */
	i = bind(myRcvSocket, (struct sockaddr *) &myAddr, sizeof(myAddr));
	if (i < 0) 
	{
		perror("could not bind server socket\n");
		exit(-1);
	}

	do
	{
		FD_ZERO(&readfds);
		FD_ZERO(&writefd);
		FD_SET(myRcvSocket, &readfds);
		FD_SET(myRcvSocket, &writefd);
		selectRC = select(myRcvSocket+1, &readfds,&writefd, NULL, &timeout); //read 

		if (selectRC < 0) 
		{
			perror("error in select!\n");
		 	return 0;
		}

		if (selectRC == 0)
		{
		 	perror("Time out!\n");
		}

		if (FD_ISSET(myRcvSocket,&readfds)) 
		{
			printf("Server is ready to read\n");

			addrSize = sizeof(clientAddr);
			bytesRcv = recvfrom(myRcvSocket, buffer, sizeof(buffer),0, (struct sockaddr *) &clientAddr, &addrSize);
			if (bytesRcv > 0)
			{
				buffer[bytesRcv] = '\0';
				inet_ntop(AF_INET, &(clientAddr.sin_addr), incomingAddr, INET_ADDRSTRLEN);
				client_t *p=malloc(sizeof(client_t));
				if(p==NULL)
					exit(-1);
				
				p->clientAddr=clientAddr;
				printf("buffer is: %s\n",buffer);
				strcpy(p->buffer,buffer);
				slist_append(list,p);
				//printf("%s : %s\n",incomingAddr, buffer);
				
			}
		}

		if (slist_size(list)>0 && FD_ISSET(myRcvSocket,&writefd)) 
		{
			client_t *q=slist_pop_first(list);// pop the first element
			toUpperCase(q->buffer);
			printf("Server is ready to write\n");
			sendto(myRcvSocket, q->buffer, strlen(q->buffer), 0,(struct sockaddr *) &(q->clientAddr), sizeof(clientAddr));	//write to client
		}  

	} while (1);

	close(myRcvSocket);
	slist_destroy(list,SLIST_FREE_DATA);
	
	return 0;
}


int isPosNumeric(const char *str) // check if the word contains only positive digits
{
    int i;
    for(i=0;i<strlen(str);i++)
    {
        if(str[i] < '0' || str[i] > '9')
            return FALSE;
    }

    int temp=atoi(str);
    if(temp <= 0)
        return FALSE;
    
    return TRUE;
}