#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "unpifiplus.h"
#include <stddef.h>
#include "utility.h"
#include <setjmp.h>
#include "unprtt.h"

#define ssthresh_min	2

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
static sigjmp_buf jmpbuf;

void readServerInput(const char *fileName,
                     ServerInput *serverInput);
void printServerInput();
void PassiveClose(int sockfd, char *s);
int clientWindowProbe( int sockfd, ClientAck *ack, int fd);
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
void  HandleFileTransmission(int sockfd, int fd, int fileLength, unsigned int ClientOriginalWindow, char *cliIP);
void AddClientToKnownClientList(char *ipOfClient, int portNum);

static void signal_alarm(int signo);

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
   unsigned int ClientOriginalWindow = 0;
   unsigned int file_send_count = 0;

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
      //printf("Maximum file descriptor set %d in process %d\n", maxfdp1, getpid()); 
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
               printf("Recvfrom error %s\n",strerror(errno));
            } else {
	       printf("Incoming request from client is with IP: %s\n",inet_ntop(AF_INET, &cliInfo.sin_addr,ipOfClient , sizeof(ipOfClient)));
	       printf("Ephemeral port of the client %s to which the client socket is bound %d\n",ipOfClient, cliInfo.sin_port);
               //printf("Size of string recieved by server child %d\n", n); 
               printf("Client %s has requested for the file %s\n",ipOfClient, clientRequestedFile); 
               if (1 == chkRequestRetransmitted(ipOfClient, cliInfo.sin_port)) {
                  //Save the address and the ephemeral port number of the client in the array of socket address structures so that we can utilized it to check if a filename corresponds to a retransmitted request or if it request from a new client 
                 //printf("\nRequest from the client %s is a retransmitted one. Hence not creating a new child to serve this request",ipOfClient);  
                 createNewChild = 0;
                 //continue; 
               } else {
                  createNewChild = 1;
                  AddClientToKnownClientList(ipOfClient, cliInfo.sin_port);
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
   //  AddClientToKnownClientList(ipOfClient, cliInfo.sin_port); 
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
         //printf("total inter face count in child is %d\n", totalInterfaces); 
         int interFaceCount = 0, val = 0;
         int isLocal = 0, isSameHost = 0;
         unsigned int connectfd, serverAddr_len, returned_portnum;
         socklen_t returned_servAddr_len, connect_clientAddr_len;
         struct sockaddr_in clientAddr, serverAddr, returned_servAddr, connect_clientAddr;
         char returned_servIp[16], buf[40];

         while (interFaceCount < totalInterfaces) {
            if (interFaceCount != count) {
               if( close(servIpInfo[interFaceCount].sockfd) < 0) {
                  printf("Error in closing sockfd %d inside the child for serving client %s . Child Exiting. Error is %s\n",servIpInfo[interFaceCount].sockfd,ipOfClient,strerror(errno));   
               } else {
                  printf("Socket descriptor %d at position %d closed in server child for serving client %s\n", servIpInfo[interFaceCount].sockfd, interFaceCount, ipOfClient);  
               } 
            }
            interFaceCount++; 
         } 
  
         // Check if the client is local to the server
        // printf("IP of client is %s\n", ipOfClient);
         struct sockaddr_in subNetClient; 
         char s[40]; 
         inet_pton( AF_INET , ipOfClient , &clientAddr.sin_addr.s_addr); 
         for (interFaceCount = 0 ;interFaceCount < totalInterfaces; interFaceCount++) {
            //Convert the subnet address and the Network mask from the structure to network format 
            inet_pton( AF_INET , servIpInfo[interFaceCount].netMaskIp, &maskforSubnet.sin_addr.s_addr);
            inet_pton( AF_INET , servIpInfo[interFaceCount].subnet_addr, &subNetAddr.sin_addr.s_addr);
            if(strcmp(servIpInfo[interFaceCount].serverIp, ipOfClient ) == 0) {
               printf("Server %s and client are on the same host %s\n",servIpInfo[interFaceCount].serverIp,ipOfClient);
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
               printf("Client %s and Server %s are on a local Network\n",ipOfClient,servIpInfo[interFaceCount].serverIp);
               flagDontRoute = 1; 
               break;  
            }
         }
         
         if (isLocal != 0 && isSameHost != 0) {
            printf("Client %s and server are on different networks \n",ipOfClient);  
         }

         /*Rohan: Oct 17: Creating new socket in server to handle file transfer*/
         connectfd = socket(AF_INET, SOCK_DGRAM, 0);
         bzero(&serverAddr, sizeof(serverAddr));
         serverAddr.sin_family = AF_INET;
         serverAddr.sin_port = htons(0);
         inet_pton(AF_INET, servIpInfo[interFaceCount].serverIp, &serverAddr.sin_addr);
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
        // printf("IPserver: %s  ServerPort : %d\n",returned_servIp, returned_portnum);
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
         
         //sleep(90);
         /*Sending the datagram to client with ephemeral port number and file size as payload*/
         servInfo.serverPort = htons(returned_portnum);
         requestedFileFd = open(clientRequestedFile, O_RDONLY);
         servInfo.sentFileLength = htonl(lseek(requestedFileFd, 0L, SEEK_END));
         servInfo.originalRecvWindow = 0; 
         if((bytesSent = sendto(servIpInfo[count].sockfd, &servInfo, sizeof(servInfo), 0, (struct sockaddr *)&cliInfo, sizeof(cliInfo))) < 0) {
            printf("error in sending server child's ephemeral port number to client %s. Error is %s\n",ipOfClient, strerror(errno));
         } else {
            printf("Succesfully sent server's ephemeral port to client %s.\n",ipOfClient); 
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
               printf("Received ack from client for the server port and file. Sending file now .\n");
               ClientOriginalWindow = servInfoAck.originalRecvWindow;
               printf("Original Client window of the client %s is %d\n",ipOfClient, servInfoAck.originalRecvWindow);
               file_send_count = 0;
            } else {
               // What happened ?
               file_send_count++;
               if(file_send_count == 12)
               goto done;
            } 
         } 
         close(servIpInfo[count].sockfd);
         if(connect(connectfd, (SA *)&connect_clientAddr, sizeof(connect_clientAddr)) < 0) {
            close(connectfd);
            printf("error in connetion: %s\n",strerror(errno));
            exit(1);
         }
         
         HandleFileTransmission(connectfd, requestedFileFd, servInfo.sentFileLength, ClientOriginalWindow, ipOfClient); 
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
   printf("\n Alok");
   printf("\n totalNumSegments %d", totalNumSegments); 
   fileSegment = calloc(1, totalNumSegments * sizeof *fileSegment);
   for (i = 0; i < totalNumSegments; i++) {
      printf("\nHi%d",i); 
      printf("\n%d",totalNumSegments);
      fileSegment[i].segmentNum = i;
      fileSegment[i].segmentOffset = i * SEGMENTDATASIZE;
      if (i == (totalNumSegments - 1)) {
         fileSegment[i].totalBytesInSegment = fileLength % SEGMENTDATASIZE;
      } else {
         fileSegment[i].totalBytesInSegment = SEGMENTDATASIZE;

       }
   }
   printf("\nAnurag");
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
 

void  HandleFileTransmission(int sockfd, int fd, int fileLength, unsigned int ClientOriginalWindow, char *cliIP)
{
   int totalNumSegments = GetSegmentCount(fileLength);  
   int currentWindow =  SEGMENTSIZE;
   int index = 0;
   int totalToSend, totalToRecv = 0;
   int lastAdvtWindow =ClientOriginalWindow; //serverInput.sendingWindowSize;
   fd_set rset;
   int numRead;
   // Fix this to some value
   struct timeval timeout = {2, 0};
   int k, i, clientRequest;
   int lastAckReceived = -1;
   int duplicateAck = 0;
   ClientAck ack;
   static struct rtt_info rttinfo;
   static int rttinit = 0;
   sigset_t set_alarm;
   uint32_t tsend, trecv;
   unsigned int ssthresh, cwindow;
   ssthresh = ClientOriginalWindow;
   cwindow = 1;
   printf("File length %d being sent to the client %s. Segmentsize %d Number of segments in the file being sent %d\n", fileLength,cliIP, SEGMENTDATASIZE, totalNumSegments);
  
   if(rttinit == 0)
   {
       rtt_init(&rttinfo);  /*called for first time*/
       rttinit = 1;
       rtt_d_flag = 1;
   }

   struct itimerval timeout_rtt;
printf("we are one");
   InitializeFileSegmentInfo(fileLength);
   currentWindow = 1;
  printf("\nBye");
    
   while (index < totalNumSegments) {
      #if 0
      #endif
   printf("\nHi"); 
      /*Hardcoding as we have to do slow start also for conjestion control*/
     // totalToSend = (currentWindow < lastAdvtWindow) ? currentWindow : lastAdvtWindow;
       totalToSend = ( lastAdvtWindow < cwindow )? lastAdvtWindow : cwindow; 
      printf("Server child: %d. Segments sent in the current cycle to client %s  is %d\n",getpid(),cliIP,totalToSend); 
      for(i = 0; i < totalToSend; i++)
      {
          SendSegment(sockfd, fd, index);
          index++;
      }

      totalToRecv = 0;
      printf("\nServer Child:%d Id of last segment sent to the client %s is  %d\n",getpid(),cliIP,index);
      
      tsend = rtt_ts(&rttinfo);
      timeout_rtt.it_interval.tv_sec = 0;
      timeout_rtt.it_interval.tv_usec = 0;

      timeout_rtt.it_value.tv_sec = rtt_start(&rttinfo) / 1000;
      timeout_rtt.it_value.tv_usec = (rtt_start(&rttinfo) % 1000) * 1000;

      setitimer(ITIMER_REAL, &timeout_rtt, 0);
      signal(SIGALRM, signal_alarm);
      
      if(sigsetjmp(jmpbuf, 1) != 0)
      {
          if(rtt_timeout(&rttinfo) < 0)
          {
              printf("12 Consecutive sends to the client %s failed.Server child %d exiting\n",cliIP,getpid());
              close(sockfd);
              break;
          }
          else
          {
          /*add code for conjestion control in case of timeout here*/
              printf("Server child :%d. Timeout occured..sending the missing segment : %d to client %s\n",getpid(),ack.currentRequest,cliIP);
              SendSegment(sockfd, fd, ack.currentRequest);
          }
          timeout_rtt.it_interval.tv_sec = 0;
          timeout_rtt.it_interval.tv_usec = 0;

          timeout_rtt.it_value.tv_sec = rtt_start(&rttinfo) / 1000;
          timeout_rtt.it_value.tv_usec = (rtt_start(&rttinfo) % 1000) * 1000;
          setitimer(ITIMER_REAL, &timeout_rtt, 0);
          printf("Server child : %d. Resetting the itimer for the client %s\n",getpid(),cliIP);
          cwindow = 1;
          printf("Server child :%d. Modifying conjestion window for timeout for the client %s. cwindow = %d\n",getpid(),cliIP,cwindow);
          if(ssthresh >= 2*ssthresh_min)
          {
              ssthresh = ssthresh/2;
          }
          else
          {
              ssthresh = ssthresh_min;
          }
      }  

      for(;;)
      {
          printf("Server child : %d. Calling read to read ack from client %s\n",getpid(),cliIP);
          sigprocmask(SIG_UNBLOCK, &set_alarm, NULL);
          numRead = read(sockfd, &ack, sizeof ack);
          sigprocmask(SIG_BLOCK, &set_alarm, NULL);
          if (numRead == sizeof ack) {
             printf("Server child :%d. Client %s send acknowledgement asking for segment %d. Current advertised window is %d\n",getpid(),cliIP, ack.currentRequest, ack.advtWindow);
             currentWindow = ack.advtWindow;
          }
          else if(numRead == -1)
          {
              printf("error in read error: %s\n",strerror(errno));
          }
          /*Rohan: checking for end condition*/
          if(ack.lastack_sent == 1)
          {
              //printf("last ack received\n");
              break;
          }
    
          if(ack.advtWindow == 0)
          {
              printf("Server child : %d. Received window 0 from client %s. Start probing\n",getpid(),cliIP);
              clientWindowProbe(sockfd, &ack, fd);
              currentWindow = ack.advtWindow;
              break;
          }

          if(lastAckReceived == ack.currentRequest)
          {
              printf("Server child : %d. Duplicate ack received from the client %s for segment %d. Duplicate ack count for this segment is : %d\n",getpid(), cliIP, lastAckReceived, duplicateAck+1);
              duplicateAck++;
          }
          else
          {
              printf("Server child :%d. New ack received for segment %d from the client %s\n",getpid(), ack.currentRequest, cliIP);
              printf("Server Child : %d. Value of ssthresh is %d for the client %s\n",getpid(), ssthresh, cliIP); 
              totalToRecv++;
             // if(totalToRecv == totalToSend)
              // break;
              /*calculate and reset timeout*/
              timeout_rtt.it_interval.tv_sec = 0;
              timeout_rtt.it_interval.tv_usec = 0;

              timeout_rtt.it_value.tv_sec = 0;
              timeout_rtt.it_value.tv_usec = 0;

             // setitimer(ITIMER_REAL, &timeout_rtt, 0);
              trecv = rtt_ts(&rttinfo);
              rtt_stop(&rttinfo, tsend - trecv);
              printf("Server child : %d. Timeout value: = %d millisecods for the client %s\n",getpid(), rttinfo.rtt_rto, cliIP);
              rtt_newpack(&rttinfo);
              lastAckReceived = ack.currentRequest;
              duplicateAck = 0;
              currentWindow = ack.advtWindow;
              printf("Server child : %d. Value of Current Window is %d\n",getpid(),currentWindow); 
              if(cwindow < ssthresh && cwindow < lastAdvtWindow) //currentWindow)lastAdvtWindow)
              {
                  cwindow++;
                  printf("Server child: %d. Conjestion window modified to: %d\n",getpid(),cwindow);
                 
                  timeout_rtt.it_value.tv_sec = rtt_start(&rttinfo) / 1000;
                  timeout_rtt.it_value.tv_usec = (rtt_start(&rttinfo) % 1000) * 1000;

                  setitimer(ITIMER_REAL, &timeout_rtt, 0);
              }
              if( totalToRecv == totalToSend){ 
                 //totalToRecv=0;
                 lastAdvtWindow = ack.advtWindow; 
                 break;  
              }  
          }
            
          if(duplicateAck == 2)
          {
              printf("Server child : %d. Three Duplicate ack received from client %s for segment %d. Fast retransmit\n",getpid(), cliIP, ack.currentRequest );
              if(currentWindow > ssthresh)
              currentWindow = ssthresh;
              cwindow = cwindow / 2;
              printf("Server child : %d. Half the conjestion window due to receipt of three duplicate acks\n",getpid());
              printf("Server child: %d. Conjestion window  for the client %s is %d\n",getpid(),cliIP,cwindow);
              /*fast retransmit*/
              SendSegment(sockfd, fd, lastAckReceived);
              /*calculate and reset timeout*/
              cwindow = cwindow + 3;
              printf("Server Child %d :After sending the missing segment in fast retransmit mode congestion window increased by three and modified to %d\n",getpid(),cwindow); 
              timeout_rtt.it_interval.tv_sec = 0;
              timeout_rtt.it_interval.tv_usec = 0;

              //timeout_rtt.it_value.tv_sec = 0;
              //timeout_rtt.it_value.tv_usec = 0;

            //  setitimer(ITIMER_REAL, &timeout_rtt, 0);
              trecv = rtt_ts(&rttinfo);
              rtt_stop(&rttinfo, tsend - trecv);
              printf("\nServer Child %d: timeout value in millisec = %d\n",getpid(), rttinfo.rtt_rto);
              rtt_newpack(&rttinfo);
              timeout_rtt.it_value.tv_sec = rtt_start(&rttinfo) / 1000;
              timeout_rtt.it_value.tv_usec = (rtt_start(&rttinfo) % 1000) * 1000;
             setitimer(ITIMER_REAL, &timeout_rtt, 0); 
           }       
      }
    
      if(cwindow >= ssthresh && cwindow < lastAdvtWindow)
      {
          cwindow++;
          printf("Server child : %d.New ack received. New Conjestion window for client %s is %d\n",getpid(), cliIP,cwindow);
      }
      /*Rohan: end condition*/
      if(ack.lastack_sent == 1)
      {
          PassiveClose(sockfd,cliIP);
          printf("Server child :%d Last ack received\n",getpid());
          break;
      }

      sigprocmask(SIG_UNBLOCK, &set_alarm, NULL);
      printf("Server child : %d. Reseting the timeout for client %s\n",getpid(),cliIP);
      timeout_rtt.it_interval.tv_sec = 0;
      timeout_rtt.it_interval.tv_usec = 0;

      timeout_rtt.it_value.tv_sec = 0;
      timeout_rtt.it_value.tv_usec = 0;
      lastAdvtWindow = ack.advtWindow;
      setitimer(ITIMER_REAL, &timeout_rtt, 0);
      trecv = rtt_ts(&rttinfo);
      rtt_stop(&rttinfo, tsend - trecv);
      printf("Server child :%d. New timeout value for client %s. Value is %d milliseconds\n",getpid(),cliIP, rttinfo.rtt_rto);
      rtt_newpack(&rttinfo);
   }
   sigprocmask(SIG_UNBLOCK, &set_alarm, NULL);

   //printf("reseting the timeout\n");
   timeout_rtt.it_interval.tv_sec = 0;
   timeout_rtt.it_interval.tv_usec = 0;

   timeout_rtt.it_value.tv_sec = 0;
   timeout_rtt.it_value.tv_usec = 0;

   setitimer(ITIMER_REAL, &timeout_rtt, 0);
   trecv = rtt_ts(&rttinfo);
   rtt_stop(&rttinfo, tsend - trecv);
   printf("Server child :%d Timeout value: millisec = %d\n", rttinfo.rtt_rto);
   rtt_newpack(&rttinfo);
   printf("\nServer child :%d Deinitialize file segment made for the client %s\n",getpid(), cliIP);
   DeinitFileSegmentInfo();
   printf("Server child :%d File transfer complete for client:  %s\n",getpid(),cliIP);
 
} 

static void signal_alarm(int signo)
{
    siglongjmp(jmpbuf, 1);
}

int chkRequestRetransmitted(char *ipOfClient,
                            int portNo)
{
   int i = 0;
    //printf("\nSze of Cinfo %d", sizeof cInfo);
   // printf("\nBye"); 
   //iterate to find if the client port and Ip is already present in our array of structures if present display the appropriate message and do not fork off a child for this request
   for(i = 0; i < sizeof cInfo / sizeof cInfo[0]; i++) {
      if(cInfo[i].inUse && (strcmp(cInfo[i].clientIp, ipOfClient) == 0) && (portNo == cInfo[i].clientPort)) {
           printf("Received a retransmitted request from the client with IP: %s and port %d\n",ipOfClient, portNo); 
           return 1;
      }   
   }
   return 0;
}

void AddClientToKnownClientList(char *ipOfClient, int portNo)
{
   int i = 0;
  //printf("\nHi"); 
  for( i=0; i < sizeof cInfo / sizeof cInfo[0]; i++) {
   //insert the port and IP address in the first  empty location of our array of structures
      if ((cInfo[i].inUse == 0)) {
         strcpy( cInfo[i].clientIp, ipOfClient);
         cInfo[i].clientPort = portNo;
         cInfo[i].inUse = 1;
         return; 
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
   printf("*******************Beginning Interface List****************************\n");  
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
   printf("*******************Ending Interface List**********************************\n"); 
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

int clientWindowProbe( int sockfd, ClientAck *ack, int fd)
{
   struct timeval timeout;
   int initialWait = 3;
   timeout.tv_usec = 0;
   timeout.tv_sec = initialWait * 2;
   initialWait = initialWait * 2;
   int numRead;
   int n;
   fd_set fset;
   int network_timeout = 0;

// this initial sleep has been introduced so that we do not immediately send the probing data packet again after the advertised window has been received
   sleep(3);

   while(1){

      if(network_timeout == 11)
      {
         printf("Probe sequence sent 12 times with no response from client\n");
         printf("Terminating the file transfer........\n");
         close(sockfd);
         exit(0);
      }else{
        printf("Sending a probe sequence in the form of a packet  with sequence number %d\n",ack->currentRequest);
        SendSegment(sockfd, fd, ack->currentRequest);
      }

      //Now apply select and wait in a blocking read untill either the select returns due to timeout or 
      FD_ZERO( &fset );
      FD_SET( sockfd, &fset);
      n = Select(sockfd+1, &fset, NULL, NULL, &timeout);

      if(n < 0)
      {
          printf("error in select: %s\n",strerror(errno));
          break;
      }
      else if(FD_ISSET(sockfd, &fset))
      {
          numRead = Read(sockfd, ack, sizeof *ack);
          printf("Received reply from the client for the probe sent with packet number %d. Current window size is %d\n",ack->currentRequest, ack->advtWindow);

          if(ack->advtWindow != 0)
          {
            printf("new window received from client: %d\n",ack->advtWindow);
            return;
          }
          else
          {
              initialWait = initialWait * 2;
              if(initialWait * 2 >= 60)
              {
                  timeout.tv_usec = 0;
                  timeout.tv_sec = 60;
              }
              else
              {
                  timeout.tv_usec = 0;
                  timeout.tv_sec = initialWait;
              }

          }
       }
       else
       {
           network_timeout++;
           initialWait = initialWait * 2;
           if(initialWait * 2 >= 60)
           {
               timeout.tv_usec = 0;
               timeout.tv_sec = 60;
           }
           else
           {
               timeout.tv_usec = 0;
               timeout.tv_sec = initialWait;
           }

       }

   }
   return 0;
}

void PassiveClose(int sockfd, char *s)
{
   int numRead;
   ClientAck ack, fin;
   fd_set rset;
   int numRetry = 2;
   struct timeval timeout = { 3, 0};
   int maxFd;
   int selected;

   memset(&ack, 0, sizeof (ack));
   memset(&fin, 0, sizeof (fin));
  
   FD_ZERO(&rset); 
   numRead = Read(sockfd, &ack, sizeof(ack));
   if (ntohl(ack.currentRequest) != FIN_REQUEST) {
      printf("Unknown Ack type ignoring.\n");
      return;
   }
   printf("Server Child: %d Received FIN from client %s. Passive closing on server side.\n",getpid(),s);
   while(numRetry) {
      FD_SET(sockfd, &rset);
      ack.currentRequest = htonl(ACK_REQUEST);
      printf("Server Child:%d Sending ACK of received client FIN to client %s.\n",getpid(),s); 
      Write(sockfd, &ack, sizeof (ack));
      printf("Server Child:%d Sending FIN to client %s.\n",getpid(),s);
      fin.currentRequest = htonl(FIN_REQUEST);
      Write(sockfd, &fin, sizeof (fin));
      maxFd = sockfd + 1;
      timeout.tv_sec = 3;
      timeout.tv_usec = 0;
      selected = Select(maxFd, &rset, NULL, NULL, &timeout);
      if (selected == 0) {
         /* Client ack timed out. Retry now */
         printf("Server Child:%d Client Ack timedout. Retrying now.\n",getpid());
         numRetry--;
         continue;
      }
      if (FD_ISSET(sockfd, &rset)) {
         numRead = Read(sockfd, &ack, sizeof (ack));
         if (ntohl(ack.currentRequest) == ACK_REQUEST) {
            printf("Server Child:%d Read ack back from the client %s for the sent FIN.\n",getpid(),s);
            break;
         }
      } 
   }
   if (numRetry == 0) {
      printf("Server child:%d Passive close failed Num retries:2\n",getpid()); 
   }
   printf("Server Child:%d Passive closing complete for client %s.\n",getpid(),s);    
}
