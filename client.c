# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <pthread.h>
# include "unpifiplus.h"
# include <stddef.h>
# include "utility.h"


ClientInput clientInput;

void readClientInput(const char *fileName,
                     ClientInput *clientInput);
void printClientInput();
void ActiveClose(int sockfd);

int lastack;//Rohan

typedef struct clientInfo{
   struct sockaddr clientIp;
   struct sockaddr netMaskIp;
   struct sockaddr_in subnet_addr;
   int flag;
   struct clientInfo *next;
}ClientIpInfo ;

# define LOOPBACK_ADDRESS "127.0.0.1"

void GetClientIPAddress(struct sockaddr_in *serverAddr,
                        struct sockaddr_in *clientAddr, 
                        int *isSameHost, 
                        int *isLocal);

ClientIpInfo* getSubAddrLongest( ClientIpInfo *prevMax, ClientIpInfo *curSubAddr, char *);  

void getServerLocation(char *servIp , ClientIpInfo *ipList, char *servIpToUse , char *clientIpToUse);
void * ConsumerThread(void *arg);
void ProducerMainThread(int sockfd, int fileLength);
static void sig_alarm(int signo)
{
    return;
}

int main(int argc , char ** argv)
{
   struct sockaddr_in serverAddr, clientAddr, boundClientAddr, peerServerAddr;
   int clientAddrLen, sockfd; 
   socklen_t boundClientAddrLen, peerServerAddrLen;
   char boundClientAddrString[16], peerServerAddrString[16];
   unsigned int boundClientPort, peerServerPort, receivedServerPort; 
   int local, sameHost;
   char buffer[40];
   ServerAck serverAck;
   ConsumerThreadParam threadParam;
   pthread_t consumerThreadId;
   fd_set readSet;
   int timeoutnum = 0; 
   struct timeval timeout = {3, 0};
   
   readClientInput("client.in", &clientInput);
   //printClientInput();
   bzero(&serverAddr, sizeof(serverAddr));
   serverAddr.sin_family = AF_INET;
   serverAddr.sin_port = htons(clientInput.serverPort); 
   Inet_pton(AF_INET, clientInput.serverIP, &serverAddr.sin_addr);
   
   bzero(&clientAddr, sizeof(clientAddr));
   clientAddr.sin_family = AF_INET;
   clientAddr.sin_port = htons(0); 
   GetClientIPAddress(&serverAddr, &clientAddr, &sameHost, &local);
   printf("Is server on Same host: %s Is server local: %s\n", sameHost ? "Yes" : "No", local ? "Yes" : "No");
   if (local || sameHost) {
      if (sameHost) {
         printf("Server is on the same host. We will use 127.0.0.1 for both client and server.\n");
      } else {
         printf("Server is local\n");
      }
   } else {
      printf("Server is not local\n");
   } 
   printf("IPclient selected %s\n", Sock_ntop_host((SA*)&clientAddr, sizeof(clientAddr)));
   printf("IPserver %s\n", Sock_ntop_host((SA*)&serverAddr, sizeof(serverAddr)));
   
   
   if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
      printf("client Error in creating socket. Error :%s\n", strerror(errno));
      return -1;
   } else {
      if (local) {
         int on = 1;
         printf("Client: Setting SO_DONOTROUTE option on the socket.\n");
         Setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof on);
      }
   }
   
   clientAddrLen = sizeof(clientAddr);
   if ((bind(sockfd, (struct sockaddr *)&clientAddr, clientAddrLen)) < 0) {
      printf("Client: Error in socket bind. Error: %s\n",strerror(errno));
      close(sockfd);
      return -1;	
   }
   
   boundClientAddrLen = sizeof(boundClientAddr);
   if(getsockname(sockfd, (struct sockaddr *)&boundClientAddr, &boundClientAddrLen) < 0) {
      printf("Error in getsockname() : %s\n", strerror(errno));
      close(sockfd);
      return -1;
   }
   
   if(inet_ntop(AF_INET, &boundClientAddr.sin_addr, boundClientAddrString, sizeof(boundClientAddrString)) == NULL) {
      printf("Error in copying the returned IP address to buffer: %s\n", strerror(errno));
      close(sockfd);
      return -1;
   }
   
   boundClientPort = boundClientAddr.sin_port;
   printf("\nIPclient: %s client ephemeral port: %d\n", boundClientAddrString, boundClientPort);

   //Issue connect for the client socket descriptor specifying the port   
   if( connect(sockfd, (SA *)&serverAddr, sizeof(serverAddr)) < 0){
      close(sockfd);
      if (errno = ETIMEDOUT) {
         printf("Connection request timed out.\n");
         exit(1); 
      } else {
         printf("Error in connection %s\n", strerror(errno));
         exit(1);   
      }
   }
   printf("Successfully connected to the server.\n"); 
   
   //fetch the Server IP and the well known port of the server to which the client is connected using the getPeername function
   peerServerAddrLen = sizeof(peerServerAddr);  
   if (getpeername(sockfd, (SA *)&peerServerAddr, &peerServerAddrLen) < 0) {
      printf("Error in getpeername() : %s\n", strerror(errno));
      close(sockfd);
      return -1;
   }
   
   if (inet_ntop(AF_INET, &peerServerAddr.sin_addr.s_addr, peerServerAddrString, sizeof(peerServerAddrString)) == NULL) {
      printf("\nerror in copying the returned IP address to buffer: %s\n",strerror(errno));
      close(sockfd);
      return -1;
   }
   
   peerServerPort = peerServerAddr.sin_port;
   printf("Server IP using getpeername function is : %s\nServer port using getpeername function is : %d\n", peerServerAddrString, peerServerPort);

   int i = 0;
loop:
  if(timeoutnum >=12)
  {
     printf("\nNo response from the server even after twelve retransmits. Exiting");
     exit(1);
  }  
  if((send(sockfd, clientInput.fileName, strlen(clientInput.fileName) + 1, 0 )) < 0) {
      printf("\nInside sendto error"); 
      //fflush(stdout); 
      printf("\nerror in sendto(): %s",strerror(errno));
   }

   // Resend request in case of timeout to the server. 
   FD_ZERO(&readSet);
   FD_SET(sockfd, &readSet);
   if (0 == Select(sockfd + 1, &readSet, NULL, NULL, &timeout)) {
      printf("Time out receiving response from server. Retrying.\n");
      timeoutnum = timeoutnum + 1; 
     goto loop;
   }
   
   if(recv(sockfd, &serverAck, sizeof(serverAck), 0) < 0) {
      printf("\n error returned %s",strerror(errno));
      fflush(stdout); 
   }
   /*assigning client original receiving window size to server ack..need to send this coz server needs
    * original client window size to set the threshhold*/

   serverAck.originalRecvWindow = clientInput.receivingWindowSize;

   //Reconnect the client socket to the port received from the datagram
   receivedServerPort = ntohs(serverAck.serverPort);
   printf("Received new server port :%d\n", receivedServerPort);

   // Dont reset all the other values. Because in case of sameHost we would have selected 127.0.0.1 as the IP only reset the new port.
   serverAddr.sin_port = htons(receivedServerPort);

   if (connect(sockfd, (SA *)&serverAddr, sizeof(serverAddr)) < 0) {
       close(sockfd);
       printf("Error in connection %s\n", strerror(errno));
       exit(1);
   } else {
      printf("Client:Successfully connected to the server on the new received port.\n");
   }

   // Write the ack back to the server to tell that we received its port and file size.
   Write(sockfd, &serverAck, sizeof serverAck);
   
   threadParam.fileLength = ntohl(serverAck.sentFileLength);

   threadParam.sockfd = sockfd;
   SlidingWindowInit();
   srand48(clientInput.seedValue);
   pthread_create(&consumerThreadId, NULL, ConsumerThread, &threadParam);
   ProducerMainThread(sockfd, serverAck.sentFileLength);
   pthread_join(consumerThreadId, NULL);
   SlidingWindowDestroy();
   exit(0);
}

int GetSegmentCount(int fileLength)
{
   int totalNumSegments = fileLength / SEGMENTDATASIZE;
   if (fileLength % SEGMENTDATASIZE) {
      totalNumSegments = totalNumSegments + 1; 
   }
   //printf("Total number of segments in the file being sent %d\n", totalNumSegments); 
   return totalNumSegments;
}

int ShouldSimulatePacketLoss()
{
   if (clientInput.pDataLoss > drand48()) {
      //printf("Simulated packet Loss.\n");
      return 1;
   }
   return 0;
}

void SendClientAckWithLoss(int sockfd, const char *name)
{
   ClientAck ack;
   // Todo uncomment this out when server side is implemented.
   if (ShouldSimulatePacketLoss()) {
      //printf("Simulated Ack packet Loss.\n");
      return;
   }
   ack.currentRequest = htonl(SlidingWindowGetNextAckIndex());
   ack.advtWindow = htonl(SlidingWindowGetAvailableWindowSize());
   /*Rohan: checking end condition*/
   printf("advertised window is %d\n",ack.advtWindow); 
    if(lastack == 1)
   ack.lastack_sent = 1;
   else
   ack.lastack_sent = 0;
   printf("%s : Sending ack request current request %d current advt window %d\n", name, ack.currentRequest, ack.advtWindow); 
   write(sockfd, &ack, sizeof (ack)); 
}

void * ConsumerThread(void *arg) {
  int fileFd = ((ConsumerThreadParam *)arg)->writeFileFd; 
  int sockFd = ((ConsumerThreadParam *)arg)->sockfd; 
  int fileLength = ((ConsumerThreadParam *)arg)->fileLength;
  int totalNumSegments = GetSegmentCount(fileLength);
  int currentSegmentNum;
  Segment segment;
  int count = 0;

  while (count < totalNumSegments) {
     useconds_t sleepDelay;
     int isWindowFull;
     pthread_mutex_lock(&syncWindowAccess);
     isWindowFull = SlidingWindowGetAvailableWindowSize() == 0 ? 1 : 0;
     SlidingWindowFetchAndRemoveSegmentFromWindow((char *)&segment, &currentSegmentNum);
     while(currentSegmentNum >= 0) {
        if (count == (totalNumSegments -1)) {
          // This is the last segment
          printf("\n");
          write(fileFd, segment.segmentData, fileLength % SEGMENTDATASIZE);
          write(1, segment.segmentData, fileLength % SEGMENTDATASIZE);
          printf("\n");
        } else {
          printf("\n");
          write(fileFd, segment.segmentData, SEGMENTDATASIZE);
          write(1, segment.segmentData, SEGMENTDATASIZE);
          printf("\n");
        }
        count++;
        //printf("Count is %d\n", count);
        SlidingWindowFetchAndRemoveSegmentFromWindow((char *)&segment, &currentSegmentNum);
     }
     // A full window was consumed. We need to send ack if we are not done with all the segments
     if (isWindowFull) {
        if (SlidingWindowGetNextAckIndex() < totalNumSegments) {
           SendClientAckWithLoss(sockFd, "Consumer");
        }
     }
     pthread_mutex_unlock(&syncWindowAccess);

     sleepDelay = (-1 * clientInput.mean * log(drand48()))*1000;
     //printf("Consumer: Sleeping for %d microseconds.\n", sleepDelay);
     usleep(sleepDelay);
  }
  //SendClientAckWithLoss(sockFd, "Consumer");//Rohan: checking oct 24
   ActiveClose(sockFd);
   printf("Consumer thread ends.\n");
}

void ProducerMainThread(int sockfd, int fileLength)
{
  int totalNumSegments = GetSegmentCount(fileLength);
  fd_set readSet;
  Segment segment;

  FD_ZERO(&readSet);
  for (;;) {
     FD_SET(sockfd, &readSet);
     Select(sockfd + 1, &readSet, NULL, NULL, NULL);
     if (FD_ISSET(sockfd, &readSet)) {
        if (SEGMENTSIZE == Read(sockfd, (char *)&segment, sizeof segment)) {
          if (ShouldSimulatePacketLoss()) {
               //printf("Simulated data packet Loss.\n");
               continue;
            }
            pthread_mutex_lock(&syncWindowAccess);
            printf("Producer: Received segment %d from server.\n", segment.segmentNum); 
            SlidingWindowAddSegmentToWindow((char *)&segment, segment.segmentNum);            
            SendClientAckWithLoss(sockfd, "Producer");
            if (SlidingWindowGetNextAckIndex() >= totalNumSegments) {
               printf("Producer: Client received all the segments.\n");
               lastack = 1;//Rohan
               pthread_mutex_unlock(&syncWindowAccess);
               SendClientAckWithLoss(sockfd, "Producer"); // Rohan
               // Todo Send Close
               return;
            }
            pthread_mutex_unlock(&syncWindowAccess);
        }
     }
  }
}

void getServerLocation(char *servIp , ClientIpInfo *ipList, char servIpToUse[] , char clientIpToUse[])
{  // This function return the position of the server wrt the client ie whether it is on the same host or is it local or is it on a outside network
    ClientIpInfo *start = ipList;
    ClientIpInfo *prevMax = ipList;
    int flag = 0; 
    char clientAddr[50],mask[50],subAddr[50];//, serverAddr[50]; 
    struct sockaddr_in ipforClient , maskforSubnet, serverSubnetAddr , subnet,servAddr ;  
     inet_pton(AF_INET , servIp , &servAddr.sin_addr.s_addr);  
     do{
          strcpy(clientAddr, Sock_ntop_host(&start->clientIp , sizeof(struct sockaddr)));
          inet_pton(AF_INET, clientAddr , &ipforClient.sin_addr.s_addr);
          if( ipforClient.sin_addr.s_addr == servAddr.sin_addr.s_addr)
          {
             printf(" Server and clinet are on the same host ");
	     strcpy(servIpToUse , "127.0.0.1");
	     strcpy(clientIpToUse , "127.0.0.1");
             return;
          } 
             start=start->next;	
    }while( start->next !=NULL);
    start = ipList;
    do{
       strcpy(clientAddr, Sock_ntop_host(&start->clientIp , sizeof(struct sockaddr)));
       inet_pton(AF_INET, clientAddr , &ipforClient.sin_addr.s_addr);
       strcpy(mask, Sock_ntop_host(&start->netMaskIp,sizeof(struct sockaddr)));
       inet_pton(AF_INET, mask, &maskforSubnet.sin_addr.s_addr);
       if((ipforClient.sin_addr.s_addr & maskforSubnet.sin_addr.s_addr) == (servAddr.sin_addr.s_addr & maskforSubnet.sin_addr.s_addr))
       {
           serverSubnetAddr.sin_addr.s_addr = servAddr.sin_addr.s_addr & maskforSubnet.sin_addr.s_addr ;
           printf("\nSubnet Address of the server is :%s\n ", inet_ntop(AF_INET, &serverSubnetAddr.sin_addr,subAddr , sizeof(subAddr))); 
           if(flag == 0)
           {
              prevMax = start;
              flag++;
           }
          if( flag > 0 ){
          /*call function to find longest prefix*/
          prevMax =  getSubAddrLongest( prevMax, start, servIp);
          }
       }
      start=start->next;	
    }while(start->next != NULL); 
    if( flag == 1)
    {
        printf("Server and Client are on the same local network\n");
        strcpy(servIpToUse , servIp);
        strcpy(clientAddr, Sock_ntop_host(&prevMax->clientIp , sizeof(struct sockaddr)));
	strcpy(clientIpToUse , clientAddr);
        return;
    }
    start = ipList;
    start = start->next;
    printf("Server and Client are on the  different  networks");
    strcpy(clientAddr, Sock_ntop_host(&start->clientIp , sizeof(struct sockaddr)));
    strcpy(clientIpToUse , clientAddr);
    strcpy(servIpToUse , servIp);
    return;
}


ClientIpInfo* getSubAddrLongest( ClientIpInfo *prevMax, ClientIpInfo *curSubAddr, char servIp[]) 
{
  char clientAddr[50], mask[50], subAddrCur[50], prevMaxSubAddr[50];char *token;
  int arrTokenPrevSubNet[4] , arrTokenCurrSubNet[4],flagDiffOctet=0,i=0;
  ClientIpInfo *maxSubAddr;
  struct sockaddr_in servAddr , ipforClient , maskforSubnet,subAddrComp;
  inet_pton(AF_INET , servIp , &servAddr.sin_addr.s_addr);   
  strcpy(clientAddr, Sock_ntop_host(&curSubAddr->clientIp , sizeof(struct sockaddr)));
  inet_pton(AF_INET, clientAddr , &ipforClient.sin_addr.s_addr); 
  //getting the Network mask to be applied on both the client and the server and store it in a sockaddr_in structure
  strcpy(mask, Sock_ntop_host(&curSubAddr->netMaskIp,sizeof(struct sockaddr)));
  inet_pton(AF_INET, mask, &maskforSubnet.sin_addr.s_addr);
  //applying the network Mask of the current Ip on the server Ip and storing the result in a string
  subAddrComp.sin_addr.s_addr = servAddr.sin_addr.s_addr & maskforSubnet.sin_addr.s_addr ;
  inet_ntop(AF_INET, &subAddrComp.sin_addr,subAddrCur , sizeof(subAddrCur)); 
  //get the Subnet address of the previous longest prefix Subnet address
  strcpy(prevMaxSubAddr, inet_ntop(AF_INET, &prevMax->subnet_addr.sin_addr.s_addr , prevMaxSubAddr,sizeof(prevMaxSubAddr)));
  token = strtok(prevMaxSubAddr , "." );
  arrTokenPrevSubNet[0] = atoi(token);
  for( i=1; i<=3; i++) 
  { 
     arrTokenPrevSubNet[i]= atoi(strtok(NULL , "." ));
    // printf(" %d \n",arrTokenPrevSubNet[i]);
  }
  token = strtok(subAddrCur , "." );
  arrTokenCurrSubNet[0] = atoi(token);
  for( i=1; i<=3; i++) 
  { 
     arrTokenCurrSubNet[ i ]= atoi(strtok( NULL , "." ));
     //printf(" %d \n",arrTokenCurrSubNet[i]);
  }
  //Now compare the previous maximum subnet address and the current maximum subnet address taking each octet  by the IP    one at a time
  for(i=0; i <= 3; i++)
  {
     if(arrTokenCurrSubNet[i] != arrTokenPrevSubNet[i])
     {
        flagDiffOctet = 1;
        break;
     }
  }
  if(flagDiffOctet == 1)
  {
     if(arrTokenCurrSubNet[i] > arrTokenPrevSubNet[i])
     {
        maxSubAddr=curSubAddr ;
     }else{
        maxSubAddr=prevMax ;   
     }
   }else{
        maxSubAddr=curSubAddr;
   }
   return maxSubAddr;
}

void GetClientIPAddress(struct sockaddr_in *serverAddr,
                        struct sockaddr_in *clientAddr,
                        int *isSameHost,
                        int *isLocal)
{
   int i = 0;
   int sameHost = 0, local = 0;
   unsigned int longestPrefixMatch = 0; 
   struct sockaddr_in *interfaceAddress, *networkMask;
   struct ifi_info *ifiInfo, *ifiInfoHead; 
   *isLocal = 0;
   *isSameHost = 0;
   
   ifiInfoHead = Get_ifi_info_plus(AF_INET, 1);
   printf("\n----Interface list Begin----\n\n");
   
   for (ifiInfo = ifiInfoHead; ifiInfo != NULL; ifiInfo = ifiInfo->ifi_next) {
      interfaceAddress = (struct sockaddr_in *)ifiInfo->ifi_addr;
      networkMask = (struct sockaddr_in *)ifiInfo->ifi_ntmaddr;
      if (0 == i) {
         // Copy whatever is the 1st interface.
         clientAddr->sin_addr.s_addr = interfaceAddress->sin_addr.s_addr;
      } else if (!*isLocal && !*isSameHost && clientAddr->sin_addr.s_addr == inet_addr(LOOPBACK_ADDRESS)) {
         // We have not yet found something which is local or is on same host and we have put stored a loopback address to return then reset it to non loopback.
         clientAddr->sin_addr.s_addr = interfaceAddress->sin_addr.s_addr;
      }
      printf("Interface IP Address %s\n", Sock_ntop_host((SA*)interfaceAddress, sizeof(*interfaceAddress)));
      printf("Network mask %s\n", Sock_ntop_host((SA*)networkMask, sizeof(*networkMask)));
      if (interfaceAddress->sin_addr.s_addr == serverAddr->sin_addr.s_addr) {
         // Case both local and same host 
         *isLocal = 1;
         *isSameHost = 1;
         clientAddr->sin_addr.s_addr = inet_addr(LOOPBACK_ADDRESS);
         serverAddr->sin_addr.s_addr = inet_addr(LOOPBACK_ADDRESS);
      }
     
      // Do this only if we have not yet found that we are on the same host aka using loopback 
      if (!*isSameHost) {
         unsigned int interfaceNetworkId = interfaceAddress->sin_addr.s_addr & networkMask->sin_addr.s_addr;
         unsigned int serverNetworkId = serverAddr->sin_addr.s_addr & networkMask->sin_addr.s_addr;
         if (interfaceNetworkId == serverNetworkId) { 
            if (longestPrefixMatch < networkMask->sin_addr.s_addr) {
               *isLocal = 1; 
               clientAddr->sin_addr.s_addr = interfaceAddress->sin_addr.s_addr;
               longestPrefixMatch = networkMask->sin_addr.s_addr;
            } 
         }
      } 
      i++;
   } 
   free_ifi_info_plus(ifiInfoHead);
   printf("\n----Interface list End----\n\n");
} 

void readClientInput(const char *fileName,
                     ClientInput *clientInput)
{
   char buffer[256];
   struct in_addr serverAddress;

   int count = 0;
   FILE *fp = fopen(fileName, "r");

   if (fp == NULL) {
      printf("Client: Unable to open input file %s. Please check if the file exist and try again.\n", fileName);
      exit(0);
   }

   while (fgets(buffer, sizeof (buffer), fp) != NULL) {
      if (feof(fp)) {
         break;
      }

      if (buffer[0] == '\0') {
         // Empty line
         continue;
      }

      if (buffer[strlen(buffer) - 1] == '\n') {
         buffer[strlen(buffer) - 1] = '\0';
      }

      switch(count) {
         case 0:
            if (0 >= inet_pton(AF_INET, buffer, &serverAddress)) {
               printf("Client: Input file %s has a server address which is invalid. Please try again.\n", fileName);
               exit(0);
            }
            strcpy(clientInput->serverIP, buffer);
            break;
         case 1:
            if (0 == (clientInput->serverPort = atoi(buffer))) {
               printf("Client: Input file %s has a server port which is 0. Please try again.\n", fileName);
               exit(0);
            }
            break;
         case 2:
            strcpy(clientInput->fileName, buffer);
            break;
         case 3:
            clientInput->receivingWindowSize = atoi(buffer);
            break;
         case 4:
            clientInput->seedValue = atoi(buffer);
            break;
         case 5:
            clientInput->pDataLoss = atof(buffer);
            break;
         case 6:
            clientInput->mean = atoi(buffer);
            break;
         default:
            printf("Client: Input file %s has more input lines than required. Please try again.\n", fileName);
            exit(0);
            break;
      }
      count++;
   }
   strcpy(clientInput->fileNameReceived, clientInput->fileName);
   strcat(clientInput->fileNameReceived, ".received");
   fclose(fp);
   return;
}

void printClientInput()
{
   printf("Server address %s\n", clientInput.serverIP);
   printf("Server port %d\n", clientInput.serverPort);
   printf("File name %s\n", clientInput.fileName);
   printf("Receiving window size %d\n", clientInput.receivingWindowSize);
   printf("Seed Value %d\n", clientInput.seedValue);
   printf("Data Loss %f\n", clientInput.pDataLoss);
   printf("Mean %d\n", clientInput.mean);
}

void ActiveClose(int sockfd)
{
   ClientAck ack, fin;
   fd_set rset; 
   struct timeval timeout = {2, 0};
   int maxFd;
   int selected;
   int numRetry = 3;
   int twoMSL = 2; 
   ActiveCloseStates currentState = FIN_WAIT1;
   int numRead;

   memset(&ack, 0, sizeof (ack));
   memset(&fin, 0, sizeof (fin));
   fflush (stdout); 
   printf("Client: Starting active closing on client side.\n");
   while (1) {
      fin.currentRequest = htonl(FIN_REQUEST); 
      Write(sockfd, &fin, sizeof (fin));
      printf("Client: Sent FIN to server.\n");
      while(1) {
         FD_SET(sockfd, &rset);
         maxFd = sockfd + 1;
         timeout.tv_sec = 2;
         timeout.tv_usec = 0;
         selected = Select(maxFd, &rset, NULL, NULL, &timeout);
         if (selected == 0) {
            if (currentState == TIME_WAIT) {
               twoMSL--;
               if (twoMSL == 0) {
                  // TIME_WAIT state and we waited for twoMSL
                  break;
               } else {
                  continue;
               }
            } else {
               numRetry--;
            }
            break; 
         }
         
         if (FD_ISSET(sockfd, &rset)) {
            numRead = Read(sockfd, &ack, sizeof (ack));
            if (ntohl(ack.currentRequest) == ACK_REQUEST) {
               if (currentState == FIN_WAIT1) {
                  printf("Client: FIN_WAIT1 --> FIN_WAIT2.\n");
                  currentState = FIN_WAIT2;
               } else if (currentState == CLOSING) {
                  printf("Client: CLOSING --> TIME_WAIT.\n");
                  currentState = TIME_WAIT;
               }
            } else if (ntohl(ack.currentRequest) == FIN_REQUEST) {
               if (currentState == FIN_WAIT1) {
                  printf("Client: FIN_WAIT1 --> CLOSING.\n");
                  currentState = CLOSING;
               } else if (currentState == FIN_WAIT2) {
                  printf("Client: FIN_WAIT2 --> TIME_WAIT.\n");
                  currentState = TIME_WAIT;
               }
               printf("Client:Sending ACK for the server FIN.\n");
               // Send ACK
               ack.currentRequest = htonl(ACK_REQUEST); 
               Write(sockfd, &ack, sizeof (ack)); 
            } else {
               break;
            }
         }
      }
      if (twoMSL == 0) {
         // TIME_WAIT state and we waited for twoMSL
         break;
      }
      if (numRetry == 0) {
         printf("Client: Active close failed Num retries:2\n");
         break;
      }
   }   
   printf("Client: Active closing complete.\n"); 
   fflush (stdout); 
}
