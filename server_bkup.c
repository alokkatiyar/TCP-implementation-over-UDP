#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
# include "unpifiplus.h"
# include <stddef.h>
# include "utility.h"

typedef struct serverInfo{
   char serverIp[INET_ADDRSTRLEN];
   char netMaskIp[INET_ADDRSTRLEN];
   char subnet_addr[INET_ADDRSTRLEN];
   int sockfd;
}ServerIntrIpInfo ;


typedef struct ServerInput {
   int serverPort;
   int sendingWindowSize;
} ServerInput;

ServerInput serverInput;
FileSegment *fileSegment;

void readServerInput(const char *fileName,
                     ServerInput *serverInput);
void printServerInput();

void sig_chld(int signo);

//structure to save Ip and Port of ech client that makes a request for a connection
typedef struct clientRequestInfo{
   char clientIp[16];
   int clientPort;
   int inUse;
   int fileLength;
   pid_t pid;
}clientInfo;

clientInfo cInfo[20];

int chkRequestRetransmitted(char *ipOfClient, int portNo);
void  HandleFileTransmission(int sockfd, int fd, int fileLength);

int main(int argc , char ** argv)
{
   ServerIntrIpInfo servIpInfo[20];
   struct sockaddr_in ipforSubnet, maskforSubnet, subNetAddr;
   int totalInterfaces, count, maxfdp1;
   struct sockaddr_in cliInfo;
   socklen_t addrlen = sizeof(cliInfo);
   char ipOfClient[16];
   char ipServ[INET_ADDRSTRLEN], clientRequestedFile[256]; 
   int n = 0;  
   fd_set rset;
   pid_t pid;
   ServerAck servInfo, servInfoAck; 
   int createNewChild = 0;  

   readServerInput("server.in", &serverInput);
   //printServerInput();
   totalInterfaces = BindServerToUnicastInterfaces(servIpInfo);
   printf("Total interfaces %d.\n", totalInterfaces);

   Signal(SIGCHLD,sig_chld);
   for( ; ;) {
      maxfdp1 = 0;
      FD_ZERO(&rset); 
      for( count = 0; count < totalInterfaces; count++) {
         FD_SET(servIpInfo[count].sockfd, &rset);
         if(maxfdp1 < servIpInfo[count].sockfd)
            maxfdp1 = servIpInfo[count].sockfd;
      }
      maxfdp1 = maxfdp1 + 1;
      printf("Maximum file descriptor set %d in process %d\n", maxfdp1, getpid()); 
      if (select(maxfdp1, &rset, NULL, NULL, NULL) < 0) {
         if(errno == EINTR) {  
             continue;
         } 
         // printf("error in select: %s\n",strerror(errno));
      } else {
         //printf("Select Returns\n");
      } 
       
      for (count = 0; count < totalInterfaces; count++) {
         if(FD_ISSET(servIpInfo[count].sockfd, &rset)) {
            printf("Socket fd number %d sockfd id %d is set for reading\n",count, servIpInfo[count].sockfd);
            // break;
     
            if (n = recvfrom(servIpInfo[count].sockfd, clientRequestedFile, sizeof clientRequestedFile, 0, (SA *)&cliInfo, &addrlen) < 0) {
               printf("recvfrom error %s\n",strerror(errno));
            } else {
	       printf("Incoming request from client is with IP: %s\n",inet_ntop(AF_INET, &cliInfo.sin_addr,ipOfClient , sizeof(ipOfClient)));
	       printf("Ephemeral port of the client to which the client socket is bound %d\n", cliInfo.sin_port);
               //printf("\nSize of string recieved by server child %d\n", n); 
               printf("Client requested file %s\n", clientRequestedFile); 
               if (1 == chkRequestRetransmitted(ipOfClient, cliInfo.sin_port)) {
                  //Save the address and the ephemeral port number of the client in the array of socket address structures so that we can utilized it to check if a filename corresponds to a retransmitted request or if it request from a new client 
                  createNewChild = 0; 
               } else {
                  createNewChild = 1; 
               }
               break;
            }    
         }//end of if          
      }//end of for
      
      if (createNewChild) {
         createNewChild = 0;
      } else {
         continue;
      }
     
      /*
      * Rohan: Before server forks a process we must check for the
      * condition of retransmission of same request by the client
      * if the client does not receive the datagram sent by the server
      * below, it will timeout and send the same file request again
      * in that case server must not create a child process.
      */
      if((pid=fork()) == 0) {
         int flagDontRoute = 0;
         int requestedFileFd;
         ssize_t bytesSent;
         struct timeval timeout = { 3, 0}; 
         //printf("\n total inter face count in child is %d ", totalInterfaces); 
         int interFaceCount = 0, val = 0;
         int isLocal = 0, isSameHost = 0;
         unsigned int connectfd, serverAddr_len, returned_portnum;
         socklen_t returned_servAddr_len, connect_clientAddr_len;
         struct sockaddr_in clientAddr, serverAddr, returned_servAddr, connect_clientAddr;
         char returned_servIp[16], buf[40];
         while (interFaceCount < totalInterfaces) {
            if (interFaceCount != count) {
               if( close(servIpInfo[interFaceCount].sockfd) < 0) {
                  printf("Error in closing sockfd %d inside the child. Child Exiting. Error is %s\n",servIpInfo[interFaceCount].sockfd,strerror(errno));   
               } else {
                  printf("Socket descriptor %d at position %d closed in child\n", servIpInfo[interFaceCount].sockfd, interFaceCount);  
               } 
            }
            interFaceCount++; 
         } 
  
         // Check if the client is local to the server
         printf("IP of client is %s\n", ipOfClient);
         struct sockaddr_in subNetClient; 
         char s[40]; 
         inet_pton( AF_INET , ipOfClient , &clientAddr.sin_addr.s_addr); 
         for (interFaceCount = 0 ;interFaceCount < totalInterfaces; interFaceCount++) {
            //Convert the subnet address and the Network mask from the structure to network format 
            inet_pton( AF_INET , servIpInfo[interFaceCount].netMaskIp, &maskforSubnet.sin_addr.s_addr);
            inet_pton( AF_INET , servIpInfo[interFaceCount].subnet_addr, &subNetAddr.sin_addr.s_addr);
            if(strcmp(servIpInfo[interFaceCount].serverIp, ipOfClient ) == 0) {
               printf("Server and client are on the same host\n");
               flagDontRoute = 1; 
               isSameHost = 1;
               break ;
            } 
           
            /*
             * Do the ANDing of client IP and Metwork mask of server interface to decide
             * if the client is local to server or not
             */
            subNetClient.sin_addr.s_addr = clientAddr.sin_addr.s_addr & maskforSubnet.sin_addr.s_addr ;
            if(subNetClient.sin_addr.s_addr == subNetAddr.sin_addr.s_addr){
               isLocal = 1;
               printf("Client and Server are on a local Network\n");
               flagDontRoute = 1; 
               break;  
            }
         }
         
         if (isLocal != 0 && isSameHost != 0) {
            printf("Client host and server are on different networks\n");  
         }

         /*Rohan: Oct 17: Creating new socket in server to handle file transfer*/
         connectfd = socket(AF_INET, SOCK_DGRAM, 0);
         bzero(&serverAddr, sizeof(serverAddr));
         serverAddr.sin_family = AF_INET;
         serverAddr.sin_port = htons(0);
         inet_pton(AF_INET, servIpInfo[count].serverIp, &serverAddr.sin_addr);
         serverAddr_len = sizeof(serverAddr);
 
         /*Rohan: 17Oct: binding the connection socket to the server*/ 
         if ((bind(connectfd, (struct sockaddr *)&serverAddr, serverAddr_len)) < 0) {
            printf("error in socket bind : %s\n",strerror(errno));
            close(connectfd);
            return -1;
         }

         /*Obtaining serverIp and ephemeral port number*/
         returned_servAddr_len = sizeof(returned_servAddr);
         if(getsockname(connectfd, (struct sockaddr *)&returned_servAddr, &returned_servAddr_len) < 0) {
            printf("error in getsockname() : %s\n",strerror(errno));
            close(connectfd);
            return -1;
         }
             
         if(inet_ntop(AF_INET, &returned_servAddr.sin_addr, returned_servIp, sizeof(returned_servIp)) == NULL) {
            printf("error in copying the returned IP address to buffer: %s\n", strerror(errno));
            close(connectfd);
            return -1;
         }
     
         returned_portnum = returned_servAddr.sin_port;
         printf("IPserver: %s  ServerPort : %d\n",returned_servIp, returned_portnum);
         /*
          * New ephemral port number obtained successfully 
          * Now changing the socket option to DONTROUTE if client
          *  and host are same or on local network
          */
         if(isLocal == 0 || isSameHost == 0) {
            if(setsockopt(connectfd,SOL_SOCKET,SO_DONTROUTE,&val,sizeof(int)) < 0)
               printf("error in setting the socket option to DONT_ROUTE\n");
         }

         /*Connecting the server socket to client IP*/
         bzero(&connect_clientAddr, sizeof(connect_clientAddr));
         connect_clientAddr.sin_family = AF_INET;
         connect_clientAddr.sin_port = htons(cliInfo.sin_port);
         inet_pton( AF_INET , ipOfClient , &connect_clientAddr.sin_addr.s_addr);
         connect_clientAddr_len = sizeof(connect_clientAddr);
         

         /*Sending the datagram to client with ephemeral port number and file size as payload*/
         servInfo.serverPort = htons(returned_portnum);
         requestedFileFd = open(clientRequestedFile, O_RDONLY);
         servInfo.sentFileLength = htonl(lseek(requestedFileFd, 0L, SEEK_END)); 
         if((bytesSent = sendto(servIpInfo[count].sockfd, &servInfo, sizeof(servInfo), 0, (struct sockaddr *)&cliInfo, sizeof(cliInfo))) < 0) {
            printf("error in sending server child's ephemeral port number to client: %s\n", strerror(errno));
         } else {
            printf("Succesfully sent server's port to client.\n"); 
            //printf("number of bytes sent: %d\n",bytesSent);
         }

retry:
         FD_ZERO(&rset);
         FD_SET(connectfd, &rset);
         maxfdp1 = connectfd + 1;
         if (0 == select(maxfdp1, &rset, NULL, NULL, &timeout)) {
            printf("Timeout while waiting for client ack.\n");
            goto done;
         }
         memset(&servInfoAck, 0, sizeof servInfoAck);
         
         if (FD_ISSET(connectfd, &rset)) {
            if (sizeof servInfoAck == Read(connectfd, &servInfoAck, sizeof servInfoAck)) {
               printf("Received ack from client for the server port and file. Sending file now %d %d.\n", servInfoAck.serverPort , servInfoAck.sentFileLength);
            } else {
               // What happened ?
               goto done;
            } 
         } 
         close(servIpInfo[count].sockfd);
         if(connect(connectfd, (SA *)&connect_clientAddr, sizeof(connect_clientAddr)) < 0) {
            close(connectfd);
            printf("\nerror in connetion: %s",strerror(errno));
            exit(1);
         }
         
         HandleFileTransmission(connectfd, requestedFileFd, servInfo.sentFileLength); 
done:
         close(requestedFileFd);
         close(connectfd);

         exit(0);  
      } //end of child
   } //end of infinite for 
   


   exit(0); 
 
} //end of main


int GetSegmentCount(int fileLength)
{
   int totalNumSegments = fileLength / SEGMENTDATASIZE;
   if (fileLength % SEGMENTDATASIZE) {
      totalNumSegments = totalNumSegments + 1;
   }
   printf("File length %d segmentsize %d Number of segments in the file being sent %d\n", fileLength, SEGMENTDATASIZE, totalNumSegments);
   return totalNumSegments;
}
void InitializeFileSegmentInfo(int fileLength)
{
   int totalNumSegments = GetSegmentCount(fileLength);
   int i;  
   fileSegment = calloc(1, totalNumSegments * sizeof fileSegment);
   for (i = 0; i < totalNumSegments; i++) {
      fileSegment[i].segmentNum = i;
      fileSegment[i].segmentOffset = i * SEGMENTDATASIZE;
      if (i == (totalNumSegments - 1)) {
         fileSegment[i].totalBytesInSegment = fileLength % SEGMENTDATASIZE;
      } else {
         fileSegment[i].totalBytesInSegment = SEGMENTDATASIZE;
      }
   }
}

void DeinitFileSegmentInfo()
{
   if (fileSegment) {
      free(fileSegment);
      fileSegment = NULL;
   }
}
void SendSegment(int sockfd, int fd, int segmentNum)
{
   Segment segment;
   lseek(fd, fileSegment[segmentNum].segmentOffset, SEEK_SET);
   Read(fd, (char *)&(segment.segmentData), fileSegment[segmentNum].totalBytesInSegment);
   segment.segmentNum  = segmentNum;
   Write(sockfd, (char *)&segment, SEGMENTSIZE);
   fileSegment[segmentNum].totalTransmissionCount++;
}
 

void  HandleFileTransmission(int sockfd, int fd, int fileLength)
{
   int totalNumSegments = GetSegmentCount(fileLength);  
   int currentWindow = SEGMENTSIZE;
   int index = 0;
   int totalToSend;
   int lastAdvtWindow = serverInput.sendingWindowSize;
   fd_set rset;
   int numRead;
   // Fix this to some value
   struct timeval timeout = {2, 0};
   int k, i, clientRequest;
   int lastAckReceived = -1;
   int duplicateAck = 0;
   ClientAck ack;


   InitializeFileSegmentInfo(fileLength);
   currentWindow = 1;
    
   printf("\nlastAdvtWindow = %d..currentWindow = %d",lastAdvtWindow, currentWindow);
   while (index < totalNumSegments) {
      #if 0
      if (lastAdvtWindow < currentWindow / SEGMENTSIZE) {
         totalToSend = lastAdvtWindow; 
         printf("\nlastAdvtWindow = %d..currentWindow = %d",lastAdvtWindow, currentWindow);
         printf("\ntotalToSend = %d",totalToSend);
      } else {
         totalToSend = currentWindow / SEGMENTSIZE;
         printf("\ntotalToSend = %d", totalToSend);
      }
      SendSegment(sockfd, fd, index);
      
      FD_ZERO(&rset); 
      FD_SET(sockfd, &rset);
      k = Select(sockfd + 1, &rset, NULL, NULL, &timeout);

      if (k == 0) {
         // timeout
      }

      if (FD_ISSET(sockfd, &rset)) {
         int tryAgain = 1;
         while(tryAgain) {
            numRead = Read(sockfd, &ack, sizeof ack);
            if (numRead == sizeof ack) {
               printf("Client ack next ack %d advt window %d\n", ack.currentRequest, ack.advtWindow); 
            }
            numRead = recv(sockfd, &ack, sizeof ack, MSG_DONTWAIT | MSG_PEEK);
            if (numRead == sizeof ack) {
               // try again
            } else {
                break;
            }
         }

      } 
      index++;
      #endif
      /*Hardcoding as we have to do slow start also for conjestion control*/
      totalToSend = (currentWindow < lastAdvtWindow) ? currentWindow : lastAdvtWindow;
      for(i = 0; i < totalToSend; i++)
      {
          SendSegment(sockfd, fd, index);
          index++;
      }
      
      for(i = 0; i < totalToSend; i++)
      {
          numRead = Read(sockfd, &ack, sizeof ack);
          if (numRead == sizeof ack) {
             printf("Client ack next ack %d advt window %d\n", ack.currentRequest, ack.advtWindow);
          }
          
          if(lastAckReceived == ack.currentRequest)
            {
                duplicateAck++;
            }
            else
            lastAckReceived = ack.currentRequest;
            
            clientRequest = ack.currentRequest;
            currentWindow = ack.advtWindow;
          
           if(duplicateAck == 2)
           {
               /*fast retransmit*/
           }       
      }
   }
   
   DeinitFileSegmentInfo();
 
} 


int chkRequestRetransmitted(char *ipOfClient,
                            int portNo)
{
   int i = 0;
   int  clientAlreadyFound = 0;
   //iterate to find if the client port and Ip is already present in our array of structures if present display the appropriate message and do not fork off a child for this request
   for(i = 0; i < sizeof cInfo / sizeof cInfo[0]; i++) {
      if(cInfo[i].inUse && (strcmp(cInfo[i].clientIp, ipOfClient) == 0) && (portNo == cInfo[i].clientPort)) {
           printf("\nReceived a retransmitted request from the client with IP: %s and port %d",ipOfClient, portNo); 
         return 1;
      }    
   }
   return 0;
}

void AddClientToKnownClientList(char *ipOfClient, int portNo)
{
   int i = 0;
   for( i=0; i < sizeof cInfo / sizeof cInfo[0]; i++) {
   //insert the port and IP address in the first  empty location of our array of structures
      if ((cInfo[i].inUse == 0)) {
         strcpy( cInfo[i].clientIp, ipOfClient);
         cInfo[i].clientPort = portNo;
         cInfo[i].inUse = 1;
      }
   }  
}

void RemoveClientFromKnownClientList(char *ipOfClient, int portNo)
{
   int i = 0;
   for( i=0; i < sizeof cInfo / sizeof cInfo[0]; i++) {
   //insert the port and IP address in the first  empty location of our array of structures
      if(cInfo[i].inUse && (strcmp(cInfo[i].clientIp, ipOfClient) == 0) && (portNo == cInfo[i].clientPort)) {
         memset(&cInfo[i], 0, sizeof cInfo[i]);
      }
   }  
}

void AddFileLengthToKnownClientList(char *ipOfClient, int portNo, int fileLength)
{
   int i = 0;
   for( i=0; i < sizeof cInfo / sizeof cInfo[0]; i++) {
   //insert the port and IP address in the first  empty location of our array of structures
      if(cInfo[i].inUse && (strcmp(cInfo[i].clientIp, ipOfClient) == 0) && (portNo == cInfo[i].clientPort)) {
         cInfo[i].fileLength = fileLength;
      }
   }  
}

void AddChildPidToKnownClientList(char *ipOfClient, int portNo, pid_t pid)
{
   int i = 0;
   for( i=0; i < sizeof cInfo / sizeof cInfo[0]; i++) {
   //insert the port and IP address in the first  empty location of our array of structures
      if(cInfo[i].inUse && (strcmp(cInfo[i].clientIp, ipOfClient) == 0) && (portNo == cInfo[i].clientPort)) {
         cInfo[i].pid = pid;
      }
   }  
}

int BindServerToUnicastInterfaces(ServerIntrIpInfo servIpInfo[])
{
   struct ifi_info *ifi;
   struct ifi_info *ifihead;
   int family = AF_INET;
   int doAliases = 1, i = 0;
   int on = 1;
   struct sockaddr_in ipforSubnet, maskforSubnet, subNetAddr;
   struct sockaddr *sa, *ma;
   
   /* Iterate through the list of server interface Ip's and
    * create and bind sockets for ech Ip address also save the
    * IP, Netmask and IP for each interface
    */
   for (ifihead = ifi = Get_ifi_info_plus(family, doAliases); ifi != NULL ; ifi = ifi->ifi_next) {
      sa = ifi->ifi_addr;
      //store the IP Address in the network format in a sockaddr_in structure
      strcpy(servIpInfo[i].serverIp, Sock_ntop_host(sa, sizeof(struct sockaddr)));
      inet_pton( AF_INET , servIpInfo[i].serverIp , &ipforSubnet.sin_addr); 
      ipforSubnet.sin_family = AF_INET;
      ipforSubnet.sin_port = htons(serverInput.serverPort);
      // inet_pton(AF_INET, clientAddr , &ipforSubnet.sin_addr); 
      //  printf("\nIP addr from Ntop %s",Sock_ntop_host(&nodePtrIp->clientIp, sizeof(struct sockaddr)));
      servIpInfo[i].sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
      Setsockopt(servIpInfo[i].sockfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)); 
      // FD_SET(servIpInfo[i].sockfd, &rset);
       
      Bind( servIpInfo[i].sockfd, (SA *)&ipforSubnet , sizeof(ipforSubnet));
      
      ma = ifi->ifi_ntmaddr;
      strcpy(servIpInfo[i].netMaskIp, Sock_ntop_host(ma, sizeof(*ma)));
      //store the Network Mask in the network format in a sockaddr_in structure
      inet_pton(AF_INET, servIpInfo[i].netMaskIp, &maskforSubnet.sin_addr); 
      subNetAddr.sin_addr.s_addr = ipforSubnet.sin_addr.s_addr & maskforSubnet.sin_addr.s_addr ;
      inet_ntop(AF_INET, &subNetAddr.sin_addr, servIpInfo[i].subnet_addr, sizeof(servIpInfo[i].subnet_addr));
      printf("Ip address for interface %d of the server is : %s\n", i, servIpInfo[i].serverIp);
      printf("Network Mask for interface %d of the server is : %s\n", i, servIpInfo[i].netMaskIp);
      printf("Subnet address for interface %d of the server is : %s\n", i, servIpInfo[i].subnet_addr);
      printf("Socket Descriptor for the interface %d of the server is %d\n", i, servIpInfo[i].sockfd);
      i++; 
   }
   fflush(stdout);
   return i;
}
//signal handler

void sig_chld(int signo)
{
   pid_t pid;
   int stat;
   while((pid = waitpid(-1,&stat,WNOHANG)) > 0) {
   }
   return;
}

void readServerInput(const char *fileName,
                     ServerInput *serverInput)
{
   char buffer[256];

   int count = 0;
   FILE *fp = fopen(fileName, "r");

   if (fp == NULL) {
      printf("Server: Unable to open input file %s. Please check if the file exist and try again.\n", fileName);
      exit(0);
   }

   while (fgets(buffer, sizeof (buffer), fp) != NULL) {
      if (feof(fp)) {
         break;
      }

      if (buffer[strlen(buffer) - 1] == '\n') {
         buffer[strlen(buffer) - 1] = '\0';
      }

      if (buffer[0] == '\0') {
         // Empty line
         continue;
      }

      switch(count) {
         case 0:
            if (0 == (serverInput->serverPort = atoi(buffer))) {
               printf("Server: Input file %s has a server port which is 0. Please try again.\n", fileName);
               exit(0);
            }
            break;
         case 1:
            serverInput->sendingWindowSize = atoi(buffer);
            break;
         default:
            printf("Server: Input file %s has more input lines than required. Please try again.\n", fileName);
            exit(0);
            break;
      }
      count++;
   }
   fclose(fp);
   return;
}

void printServerInput()
{
   printf("Server port %d\n", serverInput.serverPort);
   printf("Sending window size %d\n", serverInput.sendingWindowSize);   
}
