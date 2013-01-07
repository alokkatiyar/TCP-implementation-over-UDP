#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>


typedef struct ClientInput {
   char serverIP[INET_ADDRSTRLEN];
   int serverPort;
   char fileName[512];
   int receivingWindowSize;
   int seedValue;
   float pDataLoss;
   int mean;
} ClientInput;

typedef struct ServerInput {
   int serverPort;
   int sendingWindowSize;
} ServerInput;


ClientInput clientInput;
ServerInput serverInput;

void readClientInput(const char *fileName,
                     ClientInput *clientInput);
void printClientInput();


void readServerInput(const char *fileName,
                     ServerInput *serverInput);
void printServerInput();

int main(int argc, char *argv[])
{
    readClientInput("client.in", &clientInput);
    printClientInput();
    readServerInput("server.in", &serverInput);
    printServerInput();
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
