//***********************************************************

//                    NEVO SAYAG 200484426

//***********************************************************

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <limits.h>
#include  <fcntl.h>
#include "threadpool.h"
#include <dirent.h>

#define TRUE 1
#define FALSE 0
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

//  METHODS
void stdError(char *message);
int isPosNumeric(const char *str) ;
int checkingRequest(char* request,int newsockfd,char** gpp);
int createResponse(char* errNum, char* errDesc,char* text,char* cl,int newsockfd,char* path,char* protocol);
int pathCheckAndSend(char* path,char* protocol, int newsockfd);
int isGET(char* get);
int targetFunc(void* p);
char* get_mime_type(char *name);
int readAndWriteFile(char* path,char* protocol,int newsockfd,char* buffer,int headerSize);
int numPlaces (int n);
int isPermmision(char* full_path,char* protocol,int newsockfd);
int checkingCommand(int argc ,char *argv[]);


//******************************************************************************************************************
//******************************************************************************************************************
//******************************************************************************************************************
//******************************************************************************************************************

int main(int argc, char *argv[])
{
	// memset(get,0,4096);
	// memset(path,0,4096);
	// memset(protocol,0,4096);

	int sockfd, portno;
 // socklen_t clilen;
  char buffer[256];
  struct sockaddr_in serv_addr;
  //int nBytes;

  if (checkingCommand(argc,argv)==FALSE)// if command is wrong..errors inside checkingCommand
  {
      stdError("usage: server <port> <pool-size> <max-number-of-request>");
      return 0; 
  }

  // if we get up to here the command is ok
  //int port=atol(argv[1]);
  int numOfThreads=atol(argv[2]);
  int numOfRequests=atol(argv[3]);
  int requestCounter;


   //Sockets Layer Call: socket()
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
      perror("socket was not opened ");
      exit(-1);
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  int x;
  //Sockets Layer Call: bind()
  x=bind (sockfd, (struct sockaddr *) &serv_addr, sizeof serv_addr) ;
  if(x<0)
  {
       perror("ERROR on binding\n");
       close(sockfd);
       exit(-1);
  }


  //Sockets Layer Call: listen()
  listen(sockfd, 5);
  //clilen = sizeof(cli_addr);


  threadpool* tp=create_threadpool(numOfThreads);
  if(tp == NULL)
  {
  	stdError("create_threadpool is NULL\n ");
  	return 0;
  }
  
 int* newsockfd;

  for (requestCounter= 0; requestCounter < numOfRequests; requestCounter++)
  {   
  	
      newsockfd=(int*)malloc(sizeof(int));
      if(newsockfd==NULL)
          exit(-1);


      //Sockets Layer Call: accept()
      *newsockfd = accept(sockfd,NULL,NULL);
      
      if (newsockfd < 0)
      {   
          perror("ERROR on accept\n");
          exit(-1);  
      }

      memset(buffer,0, 256);

 
      dispatch(tp,(dispatch_fn)&targetFunc,(void*)newsockfd ); // call to function with specific newsockfd for specific client

  }

  close(sockfd);
  destroy_threadpool(tp);
	

return 0;
}


//******************************************************************************************//
//******************************************************************************************//
//******************************************************************************************//
//******************************************************************************************//


int checkingCommand(int argc ,char *argv[])
{
    int i ;
    if (argc != 4) //there must be 4 arguments 
    {
        stdError("you must enter 4 arguments\n ");
        return FALSE;
    }

    for (i= 1; i < 4; ++i) //check validation fore each argument
    {
        if(isPosNumeric(argv[i]) == FALSE)
        {
           stdError("some of your arguments is not numeric or positive \n");
           return FALSE;
        }

        if(atoi(argv[1])>65535)
        {
           stdError("port number bigger then 65535 \n");
           return FALSE;
        }
    }
    return TRUE;  
}


//******************************************************************************************//
//******************************************************************************************//
//******************************************************************************************//
//******************************************************************************************//


int isPosNumeric(const char *str) // check if the word contains inly digits
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

//*********************************************************************************************************
//*********************************************************************************************************
//*********************************************************************************************************
//*********************************************************************************************************


int checkingRequest(char* request,int newsockfd,char** gpp)
{
  	if(request ==NULL)
    {
       // stdError("request is null\n"); 
        return FALSE;
    }

    int i;
    int wordsCounter=0;
    
    char* req = (char*)malloc(sizeof(char)*strlen(request) + 1);
    if(req==NULL)
    {
      createResponse("500","Internal Server Error","Some server side error.","144",newsockfd,gpp[0],"HTTP/1.0");
    	free(req);
    	return FALSE;
    }

    strcpy(req,request);

    for (i = 0; i<strlen(request); i++)// count how many words we have in the get
    {
        if(request[i] != ' ' && request[i]  != '\t')
        {
            wordsCounter++;
        }

        while(i<strlen(request) && request[i] != ' ' && request[i] != '\t' && request[i]!='\r' && request[i]!='\n')
        {
            i++;
        }
    }

    if (wordsCounter !=3)
    {
        stdError("not a three words\n");
        free(req);
        return FALSE; // who call will write the err
    }

    char* tPath;
    char* tGet;
    char* tProtocol;

    tGet=strtok(req," ");
    tPath=strtok(NULL," ");
    tProtocol=strtok(NULL," \n"); 

    strcpy(gpp[0],tGet);
    strcpy(gpp[1],tPath);
    strcpy(gpp[2],tProtocol);

    if(strcmp(gpp[2],"HTTP/1.0")!=0  && strcmp(gpp[2],"HTTP/1.1")!=0 )// if the protocol is'nt HTTP/1.0 
    {
        stdError("protocol are not HTTP/1.0 or HTTP/1.1\n");
        free(req);
        return FALSE; 
    }

    free(req);
    return TRUE;   
}


//******************************************************************************************************************
//******************************************************************************************************************
//******************************************************************************************************************
//******************************************************************************************************************



int createResponse(char* errNum, char* errDesc,char* text,char* cl,int newsockfd,char* path,char* protocol)
{
	int findIdx=0;
	int nBytes;
	char* buffer=(char*)calloc(4096,sizeof(char));
	if(buffer==NULL)
	{   
    createResponse("500","Internal Server Error","Some server side error.","144",newsockfd ,path, protocol);
		free(buffer);
		return FALSE;
	}
	memset(buffer,0,4096);

	char* directory=(char*)calloc(4096,sizeof(char));
	if(directory==NULL)
	{
    createResponse("500","Internal Server Error","Some server side error.","144",newsockfd,path, protocol);
		free(buffer);
		free(directory);
		return FALSE;
	}
	memset(buffer,0, 4096);
	time_t now;

	char lastModify[128];
	char* full_path = (char*)calloc(6000,sizeof(char));
	if(full_path==NULL)
	{
    createResponse("500","Internal Server Error","Some server side error.","144",newsockfd,path, protocol);
		free(buffer);
		free(directory);
		return FALSE;
	}
	bzero(full_path,4096);

	strcpy(full_path,"./");
	strcat(full_path,path);
	struct stat attrib;
	stat(full_path,&attrib);	

	if(strcmp(errNum,"200")!=0) // if isn't well path
  	{
  		char html[4096]="<HTML><HEAD><TITLE>";
  		strcat(html,errNum);
  		strcat(html," ");
  		strcat(html,errDesc);
  		strcat(html,"</TITLE></HEAD><BODY><H4>");
  		strcat(html,errNum);
  		strcat(html," ");
  		strcat(html,errDesc);
		  strcat(html,"</H4>");
  		strcat(html,text);
		  strcat(html,"</BODY></HTML>");

  		strcpy(directory,html); // adding html to directory
  	} 	
	

  	else // if is a well path
  	{
  		struct dirent **itemList;
		  int i,n;

  		n = scandir(full_path, &itemList, 0, alphasort);
	    if (n >= 0)
	    {
          directory=realloc(directory,500*n*sizeof(char)); // allocate 500* num of directories
          if(directory==NULL)
          {
            createResponse("500","Internal Server Error","Some server side error.","144",newsockfd,path, protocol);
            free(buffer);
            free(directory);
            free(full_path);
            return FALSE;
          }

          buffer=realloc(buffer,((500*n)+1000)*sizeof(char)); // allocate 500* num of directories
          if(buffer==NULL)
          {
            createResponse("500","Internal Server Error","Some server side error.","144",newsockfd,path, protocol);
            free(buffer);
            free(directory);
            free(full_path);
            return FALSE;
          }



	        for (i = 0; i < n; i++) // search if index.html are in the folder
	        {	        	
        	 	if( strcmp(itemList[i]->d_name,"index.html")==0 )
        		{
        			char tempi[strlen(full_path)+10];
			  		  strcpy(tempi,full_path);

			  		  strcat(tempi,"index.html");
        			stat(tempi,&attrib);
        			if(S_ISREG(attrib.st_mode))
        			{
        				findIdx=1; // flag that tell me i was found the index.html file
        				break;
        			}
        		}
	        } 

	        if(findIdx !=1) // if i dont found the index.html i will create by my self root directory
	        {
	        	strcpy(directory,"<HTML><HEAD><TITLE>Index of ");
  			    strcat(directory,path);
  			    strcat(directory," </TITLE></HEAD><BODY><H4>Index of ");
  			    strcat(directory,path);
  			    strcat(directory,"</H4><table CELLSPACING=8><tr><th>Name</th><th>\tLast Modified: ");
  			  	strcat(directory,"</th><th>\tSize</th></tr>");


		        for (i = 0; i < n; i++) // print to the client the directory list
		        {
		        	char ss[32];
		        	char tempPath[4096];
		        	strcpy(tempPath,full_path);
		        	strcat(tempPath,itemList[i]->d_name);
		        	stat(tempPath,&attrib);
		        	strftime(lastModify, sizeof(lastModify), RFC1123FMT, gmtime(&attrib.st_mtime));

		        	strcat(directory,"<tr>\n<td><A HREF=\"");
		        	strcat(directory,itemList[i]->d_name);

		        	if(S_ISDIR(attrib.st_mode))  
		        		strcat(directory,"/\"> ");
		        	
		        	else
		        		strcat(directory,"\"> ");
		        	
		        	strcat(directory,itemList[i]->d_name);
		        	strcat(directory,"</A></td>");
		        	strcat(directory,"<td>");
		        	strcat(directory,lastModify);
		        	strcat(directory,"</td>\n");

		        	int x=attrib.st_size;
		        	sprintf(ss, "%d", x);
		        	strcat(directory,"<td>");
		        	strcat(directory,ss);
		        	strcat(directory,"</td>\n");
		        	strcat(directory,"</tr>\n"); 

		        	memset(tempPath,0,1000);
	       		}

		    	    strcat(directory,"</table><HR><ADDRESS>webserver/1.0</ADDRESS></BODY></HTML>");
		    	

	        } 

          //free dirnet table
	        for (i = 0; i < n; i++)
  	    	{
  	    		 free(itemList[i]);
  	    	}
  	    	free(itemList);
	        
  		}

	}

  // create header 
	stat(full_path,&attrib);
	char* firstLine=(char*)calloc(4096,sizeof(char));
	if(firstLine==NULL)
	{
    createResponse("500","Internal Server Error","Some server side error.","144",newsockfd,path, protocol);
		free(buffer);
		free(directory);
		free(full_path);
		return FALSE;
	}

	strcpy(firstLine,protocol); 
	strcat(firstLine," ");  
	strcat(firstLine,errNum);
	strcat(firstLine," "); 
	strcat(firstLine,errDesc);
	strcat(firstLine,"\r\n");

	char* secondLine=(char*)calloc(100,sizeof(char));
	if(secondLine==NULL)
	{
    createResponse("500","Internal Server Error","Some server side error.","144",newsockfd,path, protocol);
		free(buffer);
		free(directory);
		free(full_path);
		free(firstLine);
		return FALSE;
	}

	strcat(secondLine,"Server: webserver/1.0\r\n");
    
  char timebuf[128];
  now = time(NULL);

  strftime(timebuf, sizeof timebuf, RFC1123FMT, gmtime(&now)); 
      
  char date[500];
  strcpy(date,"Date: ");
  strcat(date,timebuf);
  strcat(date,"\r\n");

  if(strcmp("302",errNum)==0)
  {
  	strcat(date,"location: ");
    strcat(date,"/");
  	strcat(date,path);
  	strcat(date,"/");
  	strcat(date,"\r\n");
  }

  strcat(date,"Content-Type: ");
  
 
 if(strcmp("200",errNum)==0)
 {
 		if(S_ISDIR(attrib.st_mode))  
 			strcat(date,"text/html\r\n");
 		
 		else
 		{
 			char* type=get_mime_type(full_path); // get the content type
	    if(type==NULL)
	    	strcat(date,"\r\n");// not /r/n/r/n  twice !!! one of them!!!!!!!


	    else
	    {
	    	strcat(date,type);
	    	strcat(date,"\r\n");
	    }
 		}
 		
 }

 else // if is'nt 200 ok
 {
 		strcat(date,"text/html\r\n");
 }
  


  strcat(date,"Content-Length: ");
  if(strcmp("200",errNum)==0)
  {
		if(S_ISREG(attrib.st_mode))//if this is a file 
  	{
  		int size=attrib.st_size; // get the file size
  		int sizeNumOfDig=numPlaces(size);

  		char tempSize[sizeNumOfDig];
  		sprintf(tempSize, "%d", size);

  		strcat(date,tempSize);
  	}
  }
	

 	
 	else
 	{
 		strcat(date,cl);
 	}
	
  strcat(date,"\r\n");

  if(strcmp("200",errNum)==0)
  {
  	stat(full_path,&attrib);
  	strftime(lastModify, sizeof(lastModify), RFC1123FMT, gmtime(&attrib.st_mtime));
  	strcat(date,"Last-Modified: ");
  	strcat(date,lastModify);
 		strcat(date,"\r\n");
  }
  

  strcat(date,"Connection: close\r\n\r\n");

  //adding the headers
  strcpy(buffer,firstLine);
	strcat(buffer,secondLine);
	strcat(buffer,date);



	int headerSize= (int)strlen(firstLine)+ (int)strlen(secondLine)+(int)strlen(date); // header size  	
	
	if( findIdx !=1) // if it is not a file add string directory to buffer
	{
		headerSize+=(int)strlen(directory);
		strcat(buffer,directory);//
	}  

	//printf("\n\n\n\n\nbuffer send to client is:\n %s \n\n\n\n\n\n\n",buffer);
	

  if(strcmp("200",errNum)!=0)// if it is NOT a 200 ok situation write the response
  {
    // write the headers
    nBytes=write(newsockfd,buffer,headerSize);
    if(nBytes<0)
    {
      stdError("error on write  !\n");

      //free memory 
      free(firstLine);
      free(secondLine);
      free(full_path);
      free(buffer);
      free(directory);
      return FALSE;
    }
  }

  else // if it is 200 ok
  {
    if(findIdx !=1) // if is index.html i dont want it 
    {     

       if(!(S_ISREG(attrib.st_mode)) ) // if it is not a file
       {
            nBytes=write(newsockfd,buffer,headerSize);
            if(nBytes<0)
            {

              stdError("error on write  !!!!\n");
              // free memory 
              free(firstLine);
              free(secondLine);
              free(full_path);
              free(buffer);
              free(directory);
              return FALSE;
            }
       }



       else if(readAndWriteFile(full_path,protocol,newsockfd,buffer,headerSize)==FALSE) // if it is a file
       {
          free(firstLine);
          free(secondLine);
          free(full_path);
          free(buffer);
          free(directory);
          return FALSE;
       }

       else
       {
          free(firstLine);
          free(secondLine);
          free(full_path);
          free(buffer);
          free(directory);
          return TRUE;
       }
    }
  }
	


if(findIdx==1) // if i found the index.html
{

		char pathdeme[strlen(full_path)+10];
		strcpy(pathdeme,full_path);

		strcat(pathdeme,"index.html");

  	if(readAndWriteFile(pathdeme,protocol,newsockfd,buffer,headerSize)==FALSE) // read and write the index.html
  	{
  		  // free memory 
  	  	free(firstLine);
  	  	free(secondLine);
  	  	free(full_path);
  	  	free(buffer);
  	  	free(directory);
  	  	return FALSE;			
  	}

}

	// free memory 
	free(firstLine);
	free(secondLine);
	free(full_path);
	free(buffer);
	free(directory);
	return TRUE;
  	
}

//******************************************************************************************************************
//******************************************************************************************************************
//******************************************************************************************************************
//******************************************************************************************************************


int pathCheckAndSend(char* path,char* protocol, int newsockfd)
{
	struct stat attrib;
	int status;
		
	if(strlen(path) >1)
	{
		path++;// inc the pointer 1 bit and path will be without the first char
	}

	status = stat(path, &attrib);
	if(status !=0) // if the path are not exist
	{
		if(createResponse("404","Not Found","File not found.","112",newsockfd,path,protocol)==FALSE)
			 return FALSE;

		return FALSE;	
	}

	if(isPermmision(path,protocol,newsockfd)==FALSE)
	{
		if(createResponse("403","Forbidden","Access denied.","111",newsockfd,path,protocol)==FALSE)
			return FALSE;
		
		return TRUE;
	}


	if(S_ISDIR(attrib.st_mode)) // if it is a directory
	{
		if(path[strlen(path)-1] != '/')// check if the client dont add / at the end 
		{
				if(createResponse("302","Found","Directories must end with a slash.","60",newsockfd,path,protocol)==FALSE)
					return FALSE;

				close(newsockfd);
				return TRUE;
		 		
		}

	 	else// if the path end with "/"
	 	{
		 	status = stat(path, &attrib);

			if(status !=0)// if the path isnt directory
			{
				stdError("the file are you looking for arre not on this path");
				if(createResponse("404","Not Found","File not found.","112",newsockfd,path,protocol)==FALSE)
					return FALSE;

				return TRUE;
			}

			else // if the path is directory
			{
		    	if(createResponse("200","OK","","",newsockfd,path,protocol)==FALSE)
		    		return FALSE; 	

		    	return TRUE;
			}	 	
	 	}
	}

	else  // if the path is a file
    {
    	if(S_ISREG(attrib.st_mode))    
  		{         
  	    	if(createResponse("200","OK","","",newsockfd,path,protocol)==FALSE)
  		    	return FALSE;

  		}
    }
    return TRUE;
}


//******************************************************************************************************************
//******************************************************************************************************************
//******************************************************************************************************************
//******************************************************************************************************************


int isGET(char* get)
{
    if(get == NULL)
        return FALSE;

    if(strcmp(get,"GET")!=0 )// if the method is'nt GET 
        return FALSE;

    return TRUE;
}

//******************************************************************************************************************
//******************************************************************************************************************
//******************************************************************************************************************
//******************************************************************************************************************

int targetFunc(void* p)
{
	char buffer[4096];
	int nBytes;
	int* newsockfd2= (int*)(p);
  int newsockfd = *newsockfd2;
  free(newsockfd2);
	bzero(buffer,4096);

  nBytes = read(newsockfd, buffer, sizeof buffer); 
  if (nBytes < 0)
  {
      stdError("\nERROR while reading from socket\n");
      close(newsockfd); 
      return FALSE; 
  }
    
  char* tempRequest;
  tempRequest= strtok(buffer,"\r\n");

  char** gpp=(char**)calloc(3,sizeof(char*));// allocate matrix for get , path and protocol
  int i;
  for ( i= 0; i < 3; i++)
  {
    gpp[i]=(char*)calloc(4096,sizeof(char));
  }


  if(checkingRequest(tempRequest,newsockfd,gpp) == FALSE) /// if false 400 "Bad Request" respond, as in file 400.txt.
  {
    if(createResponse("400"," Bad Request","Bad Request","113",newsockfd,gpp[1],gpp[2])==FALSE)
    {
    	close(newsockfd);
      for (i= 0; i < 3; i++)
      {
        free(gpp[i]);
      }
      free(gpp);
    	return FALSE;
    }

    close(newsockfd);
    for (i= 0; i < 3; i++)
    {
      free(gpp[i]);
    }
    free(gpp);
    return FALSE;
  }


  if(isGET(gpp[0]) == FALSE) 
  {        
      if(createResponse("501","Not supported","Method is not supported.","129",newsockfd,gpp[1],gpp[2])==FALSE)
      {
      	close(newsockfd);
        for (i= 0; i < 3; i++)
        {
          free(gpp[i]);
        }
        free(gpp);
      	return FALSE;
      }

      for (i= 0; i < 3; i++)
      {
        free(gpp[i]);
      }
      free(gpp);
      close(newsockfd);
      return FALSE;            
  }

  if(pathCheckAndSend(gpp[1],gpp[2],newsockfd)==FALSE)
  {	
  	close(newsockfd);
    for (i= 0; i < 3; i++)
    {
      free(gpp[i]);
    }
    free(gpp);
  	return FALSE;
  }

  //printf("\n\n\n\nEND WITH TARGET\n");
  close(newsockfd);
  for (i= 0; i < 3; i++)
  {
    free(gpp[i]);
  }
  free(gpp);
  return TRUE;
 
}

//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************

char* get_mime_type(char *name)
{
	char *ext = strrchr(name, '.');	
	if (!ext) return NULL;
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".gif") == 0) return "image/gif";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".au") == 0) return "audio/basic";
	if (strcmp(ext, ".wav") == 0) return "audio/wav";
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
	if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
	return NULL;
}


//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************


int readAndWriteFile(char* path,char* protocol,int newsockfd, char* buffer,int headerSize)
{
  
	int fp = open(path,O_RDONLY);
  if(fp<0)
  {
      createResponse("500","Internal Server Error","Some server side error.","144",newsockfd,path,protocol);
      stdError("File open error\n");
      return FALSE;   
  }   

  int nBytes;
  nBytes=write(newsockfd,buffer,headerSize);
  if(nBytes<0)
  {
      stdError("error on write  !!!!\n");
  }


  while(1)
  {        
      char buff[256];
      int nread = read(fp,buff,256); 
      if(nread<0)
      {
      	  perror("reading error\n");
          return FALSE;
      }          
     
      if(nread > 0)
      {  
          if(write(newsockfd, buff, nread) < 0)
          {
          	//perror("\nwriting error : ");
          	return FALSE;
          }
      }
      if (nread == 0)    
          break;    
  }
  close(fp);
  return TRUE;
}

//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************

int numPlaces(int n) 
{
	int counter=0;
	while(n>0)
	{
		counter++;
		n/=10;
	}

	return counter;
}


//**********************************************************************************************
//**********************************************************************************************
//**********************************************************************************************
//**********************************************************************************************


int isPermmision(char* full_path,char* protocol,int newsockfd)
{
	struct stat attrib;
	stat(full_path,&attrib);

	if(full_path==NULL)
		return FALSE;

	char tempPath[strlen(full_path)+10];
	strcpy(tempPath,full_path);

	char* token = strtok(tempPath, "/");
	  
	char* cutten=(char*)calloc(strlen(full_path)+10,sizeof(char));  	
	if(cutten==NULL)
  {
    createResponse("500","Internal Server Error","Some server side error.","144",newsockfd,full_path,protocol);
  	free(cutten);
  	return FALSE;
  }
	strcpy(cutten,"/");

   while( token != NULL ) 
   {
      strcat(cutten,token);
      strcat(cutten,"/");
      stat(cutten,&attrib);

      if(S_ISDIR(attrib.st_mode))
      {
      	  if(!(S_IXOTH & attrib.st_mode))
      	  {
  			free(cutten);
      	  	return FALSE;
      	  }
      }

      else 
      {
    		if(S_ISREG(attrib.st_mode))
    		{

    		  	if(!(S_IROTH & attrib.st_mode))
      			{
      				free(cutten);
      			  	return FALSE;
      			}
  		  }  
      }

      token = strtok(NULL, "/");
   }

   free(cutten);
   return TRUE;
}

//**********************************************************************************************
//**********************************************************************************************
//**********************************************************************************************
//**********************************************************************************************


void stdError(char *message) 
{
    fprintf(stderr, "%s",message);
}