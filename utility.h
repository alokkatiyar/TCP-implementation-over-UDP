typedef struct ClientInput {
   char serverIP[INET_ADDRSTRLEN];
   int serverPort;
   char fileName[512];
   int receivingWindowSize;
   int seedValue;
   float pDataLoss;
   int mean;
   char fileNameReceived[512];
} ClientInput;


typedef struct ServerAck {
   // Sent Server Port
   unsigned int serverPort;
   // Size of the file that server will send
   unsigned int sentFileLength;
   // client original window size
   unsigned int originalRecvWindow;
} ServerAck;

#define FIN_REQUEST (~0)     // 0xffffffff
#define ACK_REQUEST (~0 - 1) // 0xfffffffe

typedef enum ActiveCloseStates {
  FIN_WAIT1 = 0,
  FIN_WAIT2,
  CLOSING,
  TIME_WAIT
} ActiveCloseStates; 

typedef enum PassiveCloseStates {
  CLOSE_WAIT = 0,
  LAST_ACK
} PassiveCloseStates;


typedef struct ClientAck {
   // What does client need
   unsigned int currentRequest;
   // How much space do we have left in the window
   unsigned int advtWindow;
   unsigned int lastack_sent;
} ClientAck;

typedef struct ConsumerThreadParam {
   unsigned int fileLength;
   int writeFileFd;
   int sockfd;
}  ConsumerThreadParam;

typedef struct FileSegment {
   int segmentNum;
   int segmentOffset;
   int totalBytesInSegment;
   int totalTransmissionCount;
} FileSegment;

extern FileSegment *fileSegment;

#define SEGMENTSIZE 512
#define SEGMENTDATASIZE (SEGMENTSIZE - sizeof (unsigned int))
typedef struct Segment {
   unsigned int segmentNum;
   char segmentData[SEGMENTDATASIZE]; 
} Segment;

extern ClientInput clientInput;
extern pthread_mutex_t syncWindowAccess;

void SlidingWindowInit();
void SlidingWindowDestroy();
int SlidingWindowGetNextAckIndex();
int SlidingWindowGetAvailableWindowSize();
void SlidingWindowAddSegmentToWindow(char *buf, int index);
void SlidingWindowFetchAndRemoveSegmentFromWindow(char *outbuf, int *outIndex);
