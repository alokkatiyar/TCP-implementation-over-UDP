int clientWindowProbe( int sockfd, ClientAck *ack)
{
   timeval timeout;
   int intialWait = 3;
   timeout.tv_usec = 0;
   timeout.tv_sec = initialWait * 2;
   initialWait = initialWait * 2;
   int numTimesSent = 0;
   int numRead;
   int n;
   ClientAck resp;
   fd_set fset;
   // this initial sleep has been introduced so that we do not immediately send the probing data packet again after the advertised window has been received
   sleep(3);
   while( true ){ 
      // send here the last asked segment if the number of times the packet has been sent is less than 12
      if(numTimesSent >= 12){
         printf("\nProbe sequence sent twelve time with no response from client");
         printf("\nTerminating the file transfer........");
         close(sockfd);
         exit(0);
      }else{
        printf("\nSending a probe sequence in the form of a packet  with sequence number %d",ack->currentRequest); 
        SendSegment(sockfd, fd, ack->currentRequest);
        numTimesSent ++;
      }
      //Now apply select and wait in a blocking read untill either the select returns due to timeout or 
      FD_ZERO( &fset );
      FD_SET( sockfd, &fset);
      n = Select(sockfd+1, &fset, NULL, NULL, &timeout);
      numRead = Read(sockfd, &resp, sizeof resp);
      printf("\nReceived reply from the client for the probe sent with packet number %d. Current window size is %d",ack->currentRequest, resp.advtWindow);
     // if the response says window size is not equal to zero then return else again sit in the blocking select with timeout value double the previous timeout
      if( resp.advtWindow != 0){
         return;
      }else{
         if ( initialWait * 2 <= 60 )
         {
            timeout.tv_usec = 0;
            timeout.tv_sec = initialWait * 2;
            initialWait = initialWait * 2;  
         }else{
            timeout.tv_usec = 0;
            timeout.tv_sec = 60;
         } 
       }     
    }
      
 }


