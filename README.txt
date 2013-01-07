README

Team Members: 
1) Avijit Bansal 108739390 avbansal
2) Neha Bhatnagar 108750977 nbhatnagar



Modifications:

1) How did we ensure that only unicast addresses are bound: 
The function get_ifi_info_plus does not pick up multicast and broadcast addresses which are stored in separate fields, hence all the sockets are bound to unicast addresses.


2) Implementation of the array of structures :

Structure used for list of server addresses :

struct serverAddrList {
	int sockFD;
	struct sockaddr_in  ipAddr;
	struct sockaddr_in  netMaskAddr;
	struct sockaddr_in  subnetAddr;
}

The structure consists of 4 fields :
sockFD - (integer value) The value of the socket file descriptor
ipAddr - (sockaddr_in) Stores the IP Address bound to the socket
netMaskAddr -(sockaddr_in) Stores the Network Mask for the IP address
subnetAddr -(sockaddr_in) Stores the Subnet Address 

This structure helps in obtaining information for each of the IP addresses of all the interfaces which are bound to distinct socket.




3) Modifications made for timeout mechanism :
Since the calculations had to be made at the milliseconds level , changes were made in the rtt.c and unprtt.h to adjust accordingly.

To start with , the variables were changed from float to int. For that the changes that were made for the rtt_info structure:

struct rtt_info {
  int		rtt_rtt;	/* most recent measured RTT, in mseconds */
  int		rtt_srtt;	/* smoothed RTT estimator, in mseconds */
  int		rtt_rttvar;	/* smoothed mean deviation, in mseconds */
  int		rtt_rto;	/* current RTO to use, in mseconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint32_t	rtt_base;	/* # sec since 1/1/1970 at start */
}

All the fields were converted to int from float since we can obtain the calculations using integer arithmetic.

#define	RTT_RXTMIN      1000	/* min retransmit timeout value, in mseconds */
#define	RTT_RXTMAX     3000	/* max retransmit timeout value, in mseconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

Minimum retransmit timeout value is set to 1000 msec
Maximum retransmit timeout value is set to 3000 msec
And the maximum number of retransmits are set to 12

The return time of the below mentioned function was also changed from int to float, so that we can correctly find out the milliseconds and seconds values while setting up setitimer
float		 rtt_start(struct rtt_info *);

In rtt.c, changes had to be made to handle milliseconds properly.Reason for changing to milliseconds is that small measured RTTs can show up on a scale of 0 if calculated in seconds. To have a better accuracy on timings , milliseconds has been choses.
Below are mentioned changes made in individual functions along with the reason why they were made.

i) In rtt_init ,
ptr->rtt_rttvar = 0.75; was changed to ptr->rtt_rttvar = 3000; 
We converted the value to milliseconds and store the scaled value of RTT_VAR rather than the unscaled value in the form of seconds.

ii)In rtt_ts ,
Changed
ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000); 
into 
ts = ((tv.tv_sec - ptr->rtt_base)*1000) + (tv.tv_usec);
As we are working with milliseconds so no need to divide by 1000

iii) In rtt_start,
Changed to
float
rtt_start(struct rtt_info *ptr)
{	
	float x = rtt_minmax(RTT_RTOCALC(ptr))/1000.0;
	return(x);		
}
Since we want the time to be in the range of 1000 to 3000 msec. And dividing the calcualted milliseconds to seconds, and using it to claucalte the seconds and milliseconds while setting up the setitimer.

iv) In rtt_stop,
The function definition now looks like
ptr->rtt_rtt = ms rather than ptr->rtt_rtt = ms / 1000.0 as we are now measuring in milliseconds.

ptr->rtt_srtt=(ptr->rtt_srtt+delta)>>3 instead of ptr->rtt_srtt += delta / 8;	 as we are working with integers ans storing the scaled value of SRTT, so shifting the integer 3 bits to the right, would give us equivalent to dividing by 8

ptr->rtt_rtt -= ptr->rtt_rttvar>>2 instead of ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4; for the similar reason as stated above. This would be equivalent to dividing by 4.




4) Implementation of TCP mechanisms:

README

Team Members: 
1) Avijit Bansal 108739390 avbansal
2) Neha Bhatnagar 108750977 nbhatnagar



Modifications:

1) How did we ensure that only unicast addresses are bound: 
The function get_ifi_info_plus does not pick up multicast and broadcast addresses which are stored in separate fields, hence all the sockets are bound to unicast addresses.


2) Implementation of the array of structures :

Structure used for list of server addresses :

struct serverAddrList {
	int sockFD;
	struct sockaddr_in  ipAddr;
	struct sockaddr_in  netMaskAddr;
	struct sockaddr_in  subnetAddr;
}

The structure consists of 4 fields :
sockFD - (integer value) The value of the socket file descriptor
ipAddr - (sockaddr_in) Stores the IP Address bound to the socket
netMaskAddr -(sockaddr_in) Stores the Network Mask for the IP address
subnetAddr -(sockaddr_in) Stores the Subnet Address 

This structure helps in obtaining information for each of the IP addresses of all the interfaces which are bound to distinct socket.




3) Modifications made for timeout mechanism :
Since the calculations had to be made at the milliseconds level , changes were made in the rtt.c and unprtt.h to adjust accordingly.

To start with , the variables were changed from float to int. For that the changes that were made for the rtt_info structure:

struct rtt_info {
  int		rtt_rtt;	/* most recent measured RTT, in mseconds */
  int		rtt_srtt;	/* smoothed RTT estimator, in mseconds */
  int		rtt_rttvar;	/* smoothed mean deviation, in mseconds */
  int		rtt_rto;	/* current RTO to use, in mseconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint32_t	rtt_base;	/* # sec since 1/1/1970 at start */
}

All the fields were converted to int from float since we can obtain the calculations using integer arithmetic.

#define	RTT_RXTMIN      1000	/* min retransmit timeout value, in mseconds */
#define	RTT_RXTMAX     3000	/* max retransmit timeout value, in mseconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

Minimum retransmit timeout value is set to 1000 msec
Maximum retransmit timeout value is set to 3000 msec
And the maximum number of retransmits are set to 12

The return time of the below mentioned function was also changed from int to float, so that we can correctly find out the milliseconds and seconds values while setting up setitimer
float		 rtt_start(struct rtt_info *);

In rtt.c, changes had to be made to handle milliseconds properly.Reason for changing to milliseconds is that small measured RTTs can show up on a scale of 0 if calculated in seconds. To have a better accuracy on timings , milliseconds has been choses.
Below are mentioned changes made in individual functions along with the reason why they were made.

i) In rtt_init ,
ptr->rtt_rttvar = 0.75; was changed to ptr->rtt_rttvar = 3000; 
We converted the value to milliseconds and store the scaled value of RTT_VAR rather than the unscaled value in the form of seconds.

ii)In rtt_ts ,
Changed
ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000); 
into 
ts = ((tv.tv_sec - ptr->rtt_base)*1000) + (tv.tv_usec);
As we are working with milliseconds so no need to divide by 1000

iii) In rtt_start,
Changed to
float
rtt_start(struct rtt_info *ptr)
{	
	float x = rtt_minmax(RTT_RTOCALC(ptr))/1000.0;
	return(x);		
}
Since we want the time to be in the range of 1000 to 3000 msec. And dividing the calcualted milliseconds to seconds, and using it to claucalte the seconds and milliseconds while setting up the setitimer.

iv) In rtt_stop,
The function definition now looks like
ptr->rtt_rtt = ms rather than ptr->rtt_rtt = ms / 1000.0 as we are now measuring in milliseconds.

ptr->rtt_srtt=(ptr->rtt_srtt+delta)>>3 instead of ptr->rtt_srtt += delta / 8;	 as we are working with integers ans storing the scaled value of SRTT, so shifting the integer 3 bits to the right, would give us equivalent to dividing by 8

ptr->rtt_rtt -= ptr->rtt_rttvar>>2 instead of ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4; for the similar reason as stated above. This would be equivalent to dividing by 4.




4) Implementation of TCP mechanisms:

README

Team Members: 
1) Avijit Bansal 108739390 avbansal
2) Neha Bhatnagar 108750977 nbhatnagar



Modifications:

1) How did we ensure that only unicast addresses are bound: 
The function get_ifi_info_plus does not pick up multicast and broadcast addresses which are stored in separate fields, hence all the sockets are bound to unicast addresses.


2) Implementation of the array of structures :

Structure used for list of server addresses :

struct serverAddrList {
	int sockFD;
	struct sockaddr_in  ipAddr;
	struct sockaddr_in  netMaskAddr;
	struct sockaddr_in  subnetAddr;
}

The structure consists of 4 fields :
sockFD - (integer value) The value of the socket file descriptor
ipAddr - (sockaddr_in) Stores the IP Address bound to the socket
netMaskAddr -(sockaddr_in) Stores the Network Mask for the IP address
subnetAddr -(sockaddr_in) Stores the Subnet Address 

This structure helps in obtaining information for each of the IP addresses of all the interfaces which are bound to distinct socket.




3) Modifications made for timeout mechanism :
Since the calculations had to be made at the milliseconds level , changes were made in the rtt.c and unprtt.h to adjust accordingly.

To start with , the variables were changed from float to int. For that the changes that were made for the rtt_info structure:

struct rtt_info {
  int		rtt_rtt;	/* most recent measured RTT, in mseconds */
  int		rtt_srtt;	/* smoothed RTT estimator, in mseconds */
  int		rtt_rttvar;	/* smoothed mean deviation, in mseconds */
  int		rtt_rto;	/* current RTO to use, in mseconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint32_t	rtt_base;	/* # sec since 1/1/1970 at start */
}

All the fields were converted to int from float since we can obtain the calculations using integer arithmetic.

#define	RTT_RXTMIN      1000	/* min retransmit timeout value, in mseconds */
#define	RTT_RXTMAX     3000	/* max retransmit timeout value, in mseconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

Minimum retransmit timeout value is set to 1000 msec
Maximum retransmit timeout value is set to 3000 msec
And the maximum number of retransmits are set to 12

The return time of the below mentioned function was also changed from int to float, so that we can correctly find out the milliseconds and seconds values while setting up setitimer
float		 rtt_start(struct rtt_info *);

In rtt.c, changes had to be made to handle milliseconds properly.Reason for changing to milliseconds is that small measured RTTs can show up on a scale of 0 if calculated in seconds. To have a better accuracy on timings , milliseconds has been choses.
Below are mentioned changes made in individual functions along with the reason why they were made.

i) In rtt_init ,
ptr->rtt_rttvar = 0.75; was changed to ptr->rtt_rttvar = 3000; 
We converted the value to milliseconds and store the scaled value of RTT_VAR rather than the unscaled value in the form of seconds.

ii)In rtt_ts ,
Changed
ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000); 
into 
ts = ((tv.tv_sec - ptr->rtt_base)*1000) + (tv.tv_usec);
As we are working with milliseconds so no need to divide by 1000

iii) In rtt_start,
Changed to
float
rtt_start(struct rtt_info *ptr)
{	
	float x = rtt_minmax(RTT_RTOCALC(ptr))/1000.0;
	return(x);		
}
Since we want the time to be in the range of 1000 to 3000 msec. And dividing the calcualted milliseconds to seconds, and using it to claucalte the seconds and milliseconds while setting up the setitimer.

iv) In rtt_stop,
The function definition now looks like
ptr->rtt_rtt = ms rather than ptr->rtt_rtt = ms / 1000.0 as we are now measuring in milliseconds.

ptr->rtt_srtt=(ptr->rtt_srtt+delta)>>3 instead of ptr->rtt_srtt += delta / 8;	 as we are working with integers ans storing the scaled value of SRTT, so shifting the integer 3 bits to the right, would give us equivalent to dividing by 8

ptr->rtt_rtt -= ptr->rtt_rttvar>>2 instead of ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4; for the similar reason as stated above. This would be equivalent to dividing by 4.




4) Implementation of TCP mechanisms:

README

Team Members: 
1) Avijit Bansal 108739390 avbansal
2) Neha Bhatnagar 108750977 nbhatnagar



Modifications:

1) How did we ensure that only unicast addresses are bound: 
The function get_ifi_info_plus does not pick up multicast and broadcast addresses which are stored in separate fields, hence all the sockets are bound to unicast addresses.


2) Implementation of the array of structures :

Structure used for list of server addresses :

struct serverAddrList {
	int sockFD;
	struct sockaddr_in  ipAddr;
	struct sockaddr_in  netMaskAddr;
	struct sockaddr_in  subnetAddr;
}

The structure consists of 4 fields :
sockFD - (integer value) The value of the socket file descriptor
ipAddr - (sockaddr_in) Stores the IP Address bound to the socket
netMaskAddr -(sockaddr_in) Stores the Network Mask for the IP address
subnetAddr -(sockaddr_in) Stores the Subnet Address 

This structure helps in obtaining information for each of the IP addresses of all the interfaces which are bound to distinct socket.




3) Modifications made for timeout mechanism :
Since the calculations had to be made at the milliseconds level , changes were made in the rtt.c and unprtt.h to adjust accordingly.

To start with , the variables were changed from float to int. For that the changes that were made for the rtt_info structure:

struct rtt_info {
  int		rtt_rtt;	/* most recent measured RTT, in mseconds */
  int		rtt_srtt;	/* smoothed RTT estimator, in mseconds */
  int		rtt_rttvar;	/* smoothed mean deviation, in mseconds */
  int		rtt_rto;	/* current RTO to use, in mseconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint32_t	rtt_base;	/* # sec since 1/1/1970 at start */
}

All the fields were converted to int from float since we can obtain the calculations using integer arithmetic.

#define	RTT_RXTMIN      1000	/* min retransmit timeout value, in mseconds */
#define	RTT_RXTMAX     3000	/* max retransmit timeout value, in mseconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

Minimum retransmit timeout value is set to 1000 msec
Maximum retransmit timeout value is set to 3000 msec
And the maximum number of retransmits are set to 12

The return time of the below mentioned function was also changed from int to float, so that we can correctly find out the milliseconds and seconds values while setting up setitimer
float		 rtt_start(struct rtt_info *);

In rtt.c, changes had to be made to handle milliseconds properly.Reason for changing to milliseconds is that small measured RTTs can show up on a scale of 0 if calculated in seconds. To have a better accuracy on timings , milliseconds has been choses.
Below are mentioned changes made in individual functions along with the reason why they were made.

i) In rtt_init ,
ptr->rtt_rttvar = 0.75; was changed to ptr->rtt_rttvar = 3000; 
We converted the value to milliseconds and store the scaled value of RTT_VAR rather than the unscaled value in the form of seconds.

ii)In rtt_ts ,
Changed
ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000); 
into 
ts = ((tv.tv_sec - ptr->rtt_base)*1000) + (tv.tv_usec);
As we are working with milliseconds so no need to divide by 1000

iii) In rtt_start,
Changed to
float
rtt_start(struct rtt_info *ptr)
{	
	float x = rtt_minmax(RTT_RTOCALC(ptr))/1000.0;
	return(x);		
}
Since we want the time to be in the range of 1000 to 3000 msec. And dividing the calcualted milliseconds to seconds, and using it to claucalte the seconds and milliseconds while setting up the setitimer.

iv) In rtt_stop,
The function definition now looks like
ptr->rtt_rtt = ms rather than ptr->rtt_rtt = ms / 1000.0 as we are now measuring in milliseconds.

ptr->rtt_srtt=(ptr->rtt_srtt+delta)>>3 instead of ptr->rtt_srtt += delta / 8;	 as we are working with integers ans storing the scaled value of SRTT, so shifting the integer 3 bits to the right, would give us equivalent to dividing by 8

ptr->rtt_rtt -= ptr->rtt_rttvar>>2 instead of ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4; for the similar reason as stated above. This would be equivalent to dividing by 4.




4) Implementation of TCP mechanisms:

README

Team Members: 
1) Avijit Bansal 108739390 avbansal
2) Neha Bhatnagar 108750977 nbhatnagar



Modifications:

1) How did we ensure that only unicast addresses are bound: 
The function get_ifi_info_plus does not pick up multicast and broadcast addresses which are stored in separate fields, hence all the sockets are bound to unicast addresses.


2) Implementation of the array of structures :

Structure used for list of server addresses :

struct serverAddrList {
	int sockFD;
	struct sockaddr_in  ipAddr;
	struct sockaddr_in  netMaskAddr;
	struct sockaddr_in  subnetAddr;
}

The structure consists of 4 fields :
sockFD - (integer value) The value of the socket file descriptor
ipAddr - (sockaddr_in) Stores the IP Address bound to the socket
netMaskAddr -(sockaddr_in) Stores the Network Mask for the IP address
subnetAddr -(sockaddr_in) Stores the Subnet Address 

This structure helps in obtaining information for each of the IP addresses of all the interfaces which are bound to distinct socket.




3) Modifications made for timeout mechanism :
Since the calculations had to be made at the milliseconds level , changes were made in the rtt.c and unprtt.h to adjust accordingly.

To start with , the variables were changed from float to int. For that the changes that were made for the rtt_info structure:

struct rtt_info {
  int		rtt_rtt;	/* most recent measured RTT, in mseconds */
  int		rtt_srtt;	/* smoothed RTT estimator, in mseconds */
  int		rtt_rttvar;	/* smoothed mean deviation, in mseconds */
  int		rtt_rto;	/* current RTO to use, in mseconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint32_t	rtt_base;	/* # sec since 1/1/1970 at start */
}

All the fields were converted to int from float since we can obtain the calculations using integer arithmetic.

#define	RTT_RXTMIN      1000	/* min retransmit timeout value, in mseconds */
#define	RTT_RXTMAX     3000	/* max retransmit timeout value, in mseconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

Minimum retransmit timeout value is set to 1000 msec
Maximum retransmit timeout value is set to 3000 msec
And the maximum number of retransmits are set to 12

The return time of the below mentioned function was also changed from int to float, so that we can correctly find out the milliseconds and seconds values while setting up setitimer
float		 rtt_start(struct rtt_info *);

In rtt.c, changes had to be made to handle milliseconds properly.Reason for changing to milliseconds is that small measured RTTs can show up on a scale of 0 if calculated in seconds. To have a better accuracy on timings , milliseconds has been choses.
Below are mentioned changes made in individual functions along with the reason why they were made.

i) In rtt_init ,
ptr->rtt_rttvar = 0.75; was changed to ptr->rtt_rttvar = 3000; 
We converted the value to milliseconds and store the scaled value of RTT_VAR rather than the unscaled value in the form of seconds.

ii)In rtt_ts ,
Changed
ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000); 
into 
ts = ((tv.tv_sec - ptr->rtt_base)*1000) + (tv.tv_usec);
As we are working with milliseconds so no need to divide by 1000

iii) In rtt_start,
Changed to
float
rtt_start(struct rtt_info *ptr)
{	
	float x = rtt_minmax(RTT_RTOCALC(ptr))/1000.0;
	return(x);		
}
Since we want the time to be in the range of 1000 to 3000 msec. And dividing the calcualted milliseconds to seconds, and using it to claucalte the seconds and milliseconds while setting up the setitimer.

iv) In rtt_stop,
The function definition now looks like
ptr->rtt_rtt = ms rather than ptr->rtt_rtt = ms / 1000.0 as we are now measuring in milliseconds.

ptr->rtt_srtt=(ptr->rtt_srtt+delta)>>3 instead of ptr->rtt_srtt += delta / 8;	 as we are working with integers ans storing the scaled value of SRTT, so shifting the integer 3 bits to the right, would give us equivalent to dividing by 8

ptr->rtt_rtt -= ptr->rtt_rttvar>>2 instead of ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4; for the similar reason as stated above. This would be equivalent to dividing by 4.




4) Implementation of TCP mechanisms:

README

Team Members: 
1) Avijit Bansal 108739390 avbansal
2) Neha Bhatnagar 108750977 nbhatnagar



Modifications:

1) How did we ensure that only unicast addresses are bound: 
The function get_ifi_info_plus does not pick up multicast and broadcast addresses which are stored in separate fields, hence all the sockets are bound to unicast addresses.


2) Implementation of the array of structures :

Structure used for list of server addresses :

struct serverAddrList {
	int sockFD;
	struct sockaddr_in  ipAddr;
	struct sockaddr_in  netMaskAddr;
	struct sockaddr_in  subnetAddr;
}

The structure consists of 4 fields :
sockFD - (integer value) The value of the socket file descriptor
ipAddr - (sockaddr_in) Stores the IP Address bound to the socket
netMaskAddr -(sockaddr_in) Stores the Network Mask for the IP address
subnetAddr -(sockaddr_in) Stores the Subnet Address 

This structure helps in obtaining information for each of the IP addresses of all the interfaces which are bound to distinct socket.




3) Modifications made for timeout mechanism :
Since the calculations had to be made at the milliseconds level , changes were made in the rtt.c and unprtt.h to adjust accordingly.

To start with , the variables were changed from float to int. For that the changes that were made for the rtt_info structure:

struct rtt_info {
  int		rtt_rtt;	/* most recent measured RTT, in mseconds */
  int		rtt_srtt;	/* smoothed RTT estimator, in mseconds */
  int		rtt_rttvar;	/* smoothed mean deviation, in mseconds */
  int		rtt_rto;	/* current RTO to use, in mseconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint32_t	rtt_base;	/* # sec since 1/1/1970 at start */
}

All the fields were converted to int from float since we can obtain the calculations using integer arithmetic.

#define	RTT_RXTMIN      1000	/* min retransmit timeout value, in mseconds */
#define	RTT_RXTMAX     3000	/* max retransmit timeout value, in mseconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

Minimum retransmit timeout value is set to 1000 msec
Maximum retransmit timeout value is set to 3000 msec
And the maximum number of retransmits are set to 12

The return time of the below mentioned function was also changed from int to float, so that we can correctly find out the milliseconds and seconds values while setting up setitimer
float		 rtt_start(struct rtt_info *);

In rtt.c, changes had to be made to handle milliseconds properly.Reason for changing to milliseconds is that small measured RTTs can show up on a scale of 0 if calculated in seconds. To have a better accuracy on timings , milliseconds has been choses.
Below are mentioned changes made in individual functions along with the reason why they were made.

i) In rtt_init ,
ptr->rtt_rttvar = 0.75; was changed to ptr->rtt_rttvar = 3000; 
We converted the value to milliseconds and store the scaled value of RTT_VAR rather than the unscaled value in the form of seconds.

ii)In rtt_ts ,
Changed
ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000); 
into 
ts = ((tv.tv_sec - ptr->rtt_base)*1000) + (tv.tv_usec);
As we are working with milliseconds so no need to divide by 1000

iii) In rtt_start,
Changed to
float
rtt_start(struct rtt_info *ptr)
{	
	float x = rtt_minmax(RTT_RTOCALC(ptr))/1000.0;
	return(x);		
}
Since we want the time to be in the range of 1000 to 3000 msec. And dividing the calcualted milliseconds to seconds, and using it to claucalte the seconds and milliseconds while setting up the setitimer.

iv) In rtt_stop,
The function definition now looks like
ptr->rtt_rtt = ms rather than ptr->rtt_rtt = ms / 1000.0 as we are now measuring in milliseconds.

ptr->rtt_srtt=(ptr->rtt_srtt+delta)>>3 instead of ptr->rtt_srtt += delta / 8;	 as we are working with integers ans storing the scaled value of SRTT, so shifting the integer 3 bits to the right, would give us equivalent to dividing by 8

ptr->rtt_rtt -= ptr->rtt_rttvar>>2 instead of ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4; for the similar reason as stated above. This would be equivalent to dividing by 4.




4) Implementation of TCP mechanisms:

README

Team Members: 
1) Avijit Bansal 108739390 avbansal
2) Neha Bhatnagar 108750977 nbhatnagar



Modifications:

1) How did we ensure that only unicast addresses are bound: 
The function get_ifi_info_plus does not pick up multicast and broadcast addresses which are stored in separate fields, hence all the sockets are bound to unicast addresses.


2) Implementation of the array of structures :

Structure used for list of server addresses :

struct serverAddrList {
	int sockFD;
	struct sockaddr_in  ipAddr;
	struct sockaddr_in  netMaskAddr;
	struct sockaddr_in  subnetAddr;
}

The structure consists of 4 fields :
sockFD - (integer value) The value of the socket file descriptor
ipAddr - (sockaddr_in) Stores the IP Address bound to the socket
netMaskAddr -(sockaddr_in) Stores the Network Mask for the IP address
subnetAddr -(sockaddr_in) Stores the Subnet Address 

This structure helps in obtaining information for each of the IP addresses of all the interfaces which are bound to distinct socket.




3) Modifications made for timeout mechanism :
Since the calculations had to be made at the milliseconds level , changes were made in the rtt.c and unprtt.h to adjust accordingly.

To start with , the variables were changed from float to int. For that the changes that were made for the rtt_info structure:

struct rtt_info {
  int		rtt_rtt;	/* most recent measured RTT, in mseconds */
  int		rtt_srtt;	/* smoothed RTT estimator, in mseconds */
  int		rtt_rttvar;	/* smoothed mean deviation, in mseconds */
  int		rtt_rto;	/* current RTO to use, in mseconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint32_t	rtt_base;	/* # sec since 1/1/1970 at start */
}

All the fields were converted to int from float since we can obtain the calculations using integer arithmetic.

#define	RTT_RXTMIN      1000	/* min retransmit timeout value, in mseconds */
#define	RTT_RXTMAX     3000	/* max retransmit timeout value, in mseconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

Minimum retransmit timeout value is set to 1000 msec
Maximum retransmit timeout value is set to 3000 msec
And the maximum number of retransmits are set to 12

The return time of the below mentioned function was also changed from int to float, so that we can correctly find out the milliseconds and seconds values while setting up setitimer
float		 rtt_start(struct rtt_info *);

In rtt.c, changes had to be made to handle milliseconds properly.Reason for changing to milliseconds is that small measured RTTs can show up on a scale of 0 if calculated in seconds. To have a better accuracy on timings , milliseconds has been choses.
Below are mentioned changes made in individual functions along with the reason why they were made.

i) In rtt_init ,
ptr->rtt_rttvar = 0.75; was changed to ptr->rtt_rttvar = 3000; 
We converted the value to milliseconds and store the scaled value of RTT_VAR rather than the unscaled value in the form of seconds.

ii)In rtt_ts ,
Changed
ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000); 
into 
ts = ((tv.tv_sec - ptr->rtt_base)*1000) + (tv.tv_usec);
As we are working with milliseconds so no need to divide by 1000

iii) In rtt_start,
Changed to
float
rtt_start(struct rtt_info *ptr)
{	
	float x = rtt_minmax(RTT_RTOCALC(ptr))/1000.0;
	return(x);		
}
Since we want the time to be in the range of 1000 to 3000 msec. And dividing the calcualted milliseconds to seconds, and using it to claucalte the seconds and milliseconds while setting up the setitimer.

iv) In rtt_stop,
The function definition now looks like
ptr->rtt_rtt = ms rather than ptr->rtt_rtt = ms / 1000.0 as we are now measuring in milliseconds.

ptr->rtt_srtt=(ptr->rtt_srtt+delta)>>3 instead of ptr->rtt_srtt += delta / 8;	 as we are working with integers ans storing the scaled value of SRTT, so shifting the integer 3 bits to the right, would give us equivalent to dividing by 8

ptr->rtt_rtt -= ptr->rtt_rttvar>>2 instead of ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4; for the similar reason as stated above. This would be equivalent to dividing by 4.




4) Implementation of TCP mechanisms:

