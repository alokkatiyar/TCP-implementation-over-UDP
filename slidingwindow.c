# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <sys/socket.h>
# include <sys/types.h>
# include "unpifiplus.h"
# include <stddef.h>

#include "utility.h"

extern ClientInput clientInput;

typedef struct SlidingWindow {
   int inUse;
   char segmentData[SEGMENTSIZE];
} SlidingWindow;

static SlidingWindow * slidingWindow;
static int currentIndex;
pthread_mutex_t syncWindowAccess = PTHREAD_MUTEX_INITIALIZER; 

void SlidingWindowInit()
{
    slidingWindow = (SlidingWindow *) calloc(1, clientInput.receivingWindowSize * sizeof (SlidingWindow));
    pthread_mutex_init(&syncWindowAccess, NULL);
}

void SlidingWindowDestroy()
{
   if (slidingWindow) {
      free(slidingWindow);
      slidingWindow = NULL;
   }
   pthread_mutex_destroy(&syncWindowAccess);
}


int SlidingWindowGetNextAckIndex()
{
   int i;
   for (i = 0; i < clientInput.receivingWindowSize; i++) {
      if (!slidingWindow[(currentIndex + i) % clientInput.receivingWindowSize].inUse) {
         return currentIndex + i;
      }
   } 
}

int SlidingWindowGetAvailableWindowSize()
{
  // int k = clientInput.receivingWindowSize + currentIndex - SlidingWindowGetNextAckIndex();
  // printf("\n Hi %d ",k);
  // printf("\n %d",clientInput.receivingWindowSize);
    return clientInput.receivingWindowSize + currentIndex - SlidingWindowGetNextAckIndex();
   

}

void SlidingWindowAddSegmentToWindow(char *buf, int index)
{
   int indexInWindow = index - currentIndex;
   if (!(indexInWindow < 0 || indexInWindow >= clientInput.receivingWindowSize)) {
       
      int indexInWindow = index % clientInput.receivingWindowSize;
      if (!slidingWindow[indexInWindow].inUse) {
         slidingWindow[indexInWindow].inUse = 1;
         memcpy(slidingWindow[indexInWindow].segmentData, buf, SEGMENTSIZE); 
      }
   } 
}

void SlidingWindowFetchAndRemoveSegmentFromWindow(char *outBuf, int *outIndex) {
   int indexToRemove = currentIndex % clientInput.receivingWindowSize;
   if (slidingWindow[indexToRemove].inUse) {
      printf("Sliding window index to remove %d\n", indexToRemove);fflush(stdout);
      memcpy(outBuf, slidingWindow[indexToRemove].segmentData, SEGMENTSIZE);
      slidingWindow[indexToRemove].inUse = 0;
      *outIndex = currentIndex;
      currentIndex++;
      return;
   } else {
      *outIndex = -1;
      return;
   }
}
