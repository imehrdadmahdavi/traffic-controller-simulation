#include "header.h"
#include "list.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX 2147483647

// Packet Data Scturcture
typedef struct packet_details {
  double inter_arrival_time;
  int tokens;
  double service;
  int count;
  double start_timeinq1;
  double start_timeinq2;
  double timeinq1;
  double timeinq2;
} packet;

// List for Q1, Q2, and Bucket
List Q1;
List Bucket;
List Q2;

// Program's parameters
double lambda = 0.00;
double mu = 0.00;
double r = 0.00;
int n = 0;
int P = 0;
int B = 0;

// Mutex, condition variable, and threads initialization
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
pthread_t packet_arrival_thread;
pthread_t token_arrival_thread;
pthread_t server1_thread;
pthread_t server2_thread;
pthread_t monitoring;

// Signal set initializations
sigset_t set;

// Variables used to keep track of time
double packet_arrival = 0.00;
double token_arrival = 0.00;
double service_time = 0.00;

// Variables used in calculating statistics
double Total_piat = 0.00;
double Total_pst = 0.00;
double Total_time_q1 = 0.00;
double Total_time_q2 = 0.00;
double Total_time_s1 = 0.00;
double Total_time_s2 = 0.00;
double Total_timeinsystem = 0.00;
double Total_sqtimeinsystem = 0.00;

// Define timestamp
struct timeval timer;

// Time variables
double t = 0.00;             // Start time
double end = 0.00;           // End time
double inter_arrival = 0.00; // Packet inter-arrival time

int TOK = 0;            // Current number of tokens in a bucket
int token_no = 0;       // Total tokens
int token_dropped = 0;  // Number of tokens dropped
int packet_dropped = 0; // Number of packets dropped because of P > B

// After pressing Ctrl+C
int removed_packets = 0; // Number of packets removed for Q1, Q2
int server_count = 0;    // Number of completed/serviced packets

// Flags for wake up cond_wait at server
int stop = 0;
int flag = 0;

// Flag when all packets are dropped
int drop_flag = 0;

// Flags to keep count of leaving packests at each server facility
int last_q1 = 0;
int last_q2 = 0;

// Packets calculated after being dopped: n-dropped
int actual_packet = 0;
int q1 = 0;

// File descriptor (to open the input file)
FILE *fp = NULL;

// Function pointers
void *arrival_trace_mode();         // Packet arrival if file is used
void *arrival_deterministic_mode(); // Packet arrival thread if file not used
void *token_deposit();              // Token deposit thread
void *s1();                         // server 1
void *s2();                         // server 2
void *monitor();                    // Monitor is used to handle Ctrl+C
void statistics();                  // Cal statistics
void drop(); // Used to show removed packets when terminated abruptly

/***********************************Utils**************************************/
// Trace mode (file is used)
// The arrival thread would sleep so that it can wake up at a time such that the
// inter-arrival time of the first packet would match according to the first
// record in a tracefile
void *arrival_trace_mode() {
  ListElem *elem = NULL;  // Q1
  ListElem *elem2 = NULL; // Q2
  packet *current = NULL;

  double packet_time = 0.00;
  double time_q1 = 0.00; // Time spent in q1
  double t1 = 0.00, t2 = 0.00, t3 = 0.00, t4 = 0.00, t5 = 0.00; // Timestamp
  double overhead = 0.00;                                       // overhead time

  int i = 0, j = 0, k = 0;
  int times = 0; // Updated packet arrival

  char *value[3]; // file reading token
  char *token;
  char send_buf[1024]; // file input

  actual_packet = n;

  // Reading the file
  while ((fgets(send_buf, sizeof(send_buf), fp)) != NULL) {

    j = 0;
    token = NULL;
    token = strtok(send_buf, " \t\n");
    value[j] = strdup(token);

    // splitting
    while (token) {
      j++;
      token = strtok(NULL, " \t\n");
      if (token) {
        value[j] = strdup(token);
      }
    }
    if (j != 3) {
      fprintf(stderr, "file format is wrong in the line: %d\n", i + 2);
      exit(-1);
    }

    // error condition
    packet_arrival = atof(value[0]) / 1000.00;
    if (atof(value[0]) == 0) {
      if (strcmp("0", value[0]) == 0) {
        fprintf(stderr, "Packet arrival=0\n");
        exit(-1);
      } else {
        fprintf(stderr,
                "entered packet arrival is not a real value in line %d\n",
                i + 2);
        exit(-1);
      }
    }

    if (packet_arrival < 0) {
      fprintf(stderr, "Packet arrival is negative\n");
      exit(-1);
    }

    P = atoi(value[1]);
    if (atof(value[1]) == 0) {
      if (strcmp("0", value[1]) == 0) {
        fprintf(stderr, "P=0\n");
        exit(-1);
      } else {
        fprintf(stderr, "entered P is not a real value in line %d\n", i + 2);
        exit(-1);
      }
    }
    if (P < 0) {
      fprintf(stderr, "P is negative\n");
      exit(-1);
    }
    service_time = atof(value[2]) / 1000.00;

    if (atof(value[2]) == 0) {
      if (strcmp("0", value[2]) == 0) {
        fprintf(stderr, "service_time=0\n");
        exit(-1);
      } else {
        fprintf(stderr, "entered service_time is not a real value in line %d\n",
                i + 2);
        exit(-1);
      }
    }
    if (service_time < 0) {
      fprintf(stderr, "service_time is negative\n");
      exit(-1);
    }

    if (t2 == 0.00)
      packet_time = 0.00;
    else
      packet_time = (t2 - t) * 1000; // Holds the prev entry time

    // Bookkeeping
    times = packet_arrival * 1000 - overhead; // Updating new interarrival
    gettimeofday(&timer, NULL);
    // tv_sec = the number of seconds since 1/1/1970
    // tv_usec = the number of microseconds in the current second
    t1 = timer.tv_sec + timer.tv_usec / 1000000.00;
    if (times > 0.00) {
      usleep(times * 1000); // Sleep only if inter_arrival is greater
    } else {
      usleep(0);
    }
    gettimeofday(&timer, NULL);
    t2 = (timer.tv_sec + timer.tv_usec / 1000000.0);
    overhead = ((t2 - t) * 1000 - (t1 - t) * 1000) - times; // cal overhead
    inter_arrival =
        (t2 - t) * 1000 - packet_time; //  current entry-previous entry
    Total_piat =
        Total_piat + inter_arrival; // Total interarrival for all packets

    // Packet info
    packet *newpacket;
    newpacket = (packet *)malloc(sizeof(packet));
    newpacket->inter_arrival_time = packet_arrival * 1000;
    newpacket->tokens = P;
    newpacket->service = service_time * 1000;
    newpacket->count = i + 1;

    if (newpacket->tokens <= B) // if less than the token Bucket
    {

      pthread_mutex_lock(&m);
      // The value printed for "inter-arrival time" must equal to the timestamp
      // of the "p1 arrives" event minus the timestamp of the "arrives" event
      // for
      // the previous packet.
      printf("%012.03fms: p%d arrives, needs %d tokens, inter-arrival time = "
             "%0.03fms\n",
             (t2 - t) * 1000, newpacket->count, newpacket->tokens,
             inter_arrival);
      pthread_mutex_unlock(&m);

      gettimeofday(&timer, NULL);
      t3 = timer.tv_sec + timer.tv_usec / 1000000.00;

      newpacket->start_timeinq1 = (t3 - t) * 1000;
      newpacket->start_timeinq2 = 0.00;
      newpacket->timeinq1 = 0.00;
      newpacket->timeinq2 = 0.00;

      pthread_mutex_lock(&m);             // lock
      ListAppend(&Q1, (void *)newpacket); // add to Q1
      q1++;
      printf("%012.03fms: p%d enters Q1\n", (t3 - t) * 1000, newpacket->count);
      elem = ListFirst(&Q1);
      current = (packet *)(elem->obj);
      if (current->tokens <=
          TOK) // remove the required tokens from token bucket
      {
        for (k = 0; k < current->tokens; k++) {
          elem2 = ListFirst(&Bucket);
          TOK--;
          ListUnlink(&Bucket, elem2);
        }
        ListUnlink(&Q1, elem); // removes from q1
        last_q1++;
        gettimeofday(&timer, NULL);
        t4 = timer.tv_sec + timer.tv_usec / 1000000.00;
        // The value printed for "time in Q1" must equal to the timestamp of the
        // "p1 leaves Q1" event minus the timestamp of
        // the "p1 enters Q1" event.
        printf("%012.03fms: p%d leaves Q1, time in Q1 = %0.03fms, token bucket "
               "now has %d tokens\n",
               (t4 - t) * 1000, current->count,
               (t4 - t) * 1000 - current->start_timeinq1, TOK);
        time_q1 = (t4 - t) * 1000 - current->start_timeinq1;
        current->timeinq1 = time_q1;
        gettimeofday(&timer, NULL);
        t5 = timer.tv_sec + timer.tv_usec / 1000000.00;
        current->start_timeinq2 = (t5 - t) * 1000;
        ListAppend(&Q2, (void *)current); // enters q2
        printf("%012.03fms: p%d enters Q2\n", (t5 - t) * 1000, current->count);
        pthread_cond_broadcast(&cv); // wake the server up
      }
      pthread_mutex_unlock(&m); // unlock

    } else {
      // Packets dropped
      pthread_mutex_lock(&m); // lock
      printf("%012.03fms: p%d arrives, needs %d tokens, inter-arrival time = "
             "%0.03fms, dropped\n",
             (t2 - t) * 1000, newpacket->count, newpacket->tokens,
             inter_arrival);
      pthread_mutex_unlock(&m);

      packet_dropped++;
      actual_packet--;

      if (packet_dropped == n) {
        stop = 1;
        pthread_cond_broadcast(&cv);
        ListUnlinkAll(&Bucket);
        pthread_cancel(token_arrival_thread);
      }

      if (ListLength(&Q1) == 0 && newpacket->count == n) {
        stop = 1;
        pthread_cond_broadcast(&cv);
        ListUnlinkAll(&Bucket);
        pthread_cancel(token_arrival_thread);
      }
    }
    i++; // increment
  }

  pthread_exit(NULL);
  return 0;
}
/***********************************/
// Deterministic mode (file is not used)
// The arrival thread would sleep so that it can wake up at a time such that the
// inter-arrival time of the firs packet would match according to the default or
// input lambda
void *arrival_deterministic_mode() {
  ListElem *elem = NULL;  // Q1
  ListElem *elem2 = NULL; // Q2
  packet *current = NULL;

  double packet_time = 0.00; // Previous entry time
  double time_q1 = 0.00;     // Time spent at Q1

  double t1 = 0.00, t2 = 0.00, t3 = 0.00, t4 = 0.00, t5 = 0.00; // timestamp
  double overhead = 0.00;                                       // overhead

  int i = 0, k = 0;
  int times = 0; // Updated packet inter arrival time

  actual_packet = n;

  for (i = 0; i < n; i++) {
    if (t2 == 0.00)
      packet_time = 0.00;
    else
      packet_time = (t2 - t) * 1000;

    // Cal overhead, for bookkeeping
    times = packet_arrival * 1000 - overhead;
    gettimeofday(&timer, NULL);
    t1 = timer.tv_sec + timer.tv_usec / 1000000.00;
    if (times > 0.00) {
      usleep(times * 1000);
    } else {
      usleep(0);
    }
    gettimeofday(&timer, NULL);
    t2 = (timer.tv_sec + timer.tv_usec / 1000000.0);
    overhead = ((t2 - t) * 1000 - (t1 - t) * 1000) - times;
    inter_arrival =
        (t2 - t) * 1000 - packet_time; // present entry-previous entry
    Total_piat = Total_piat + inter_arrival;
    // Packet info
    packet *newpacket;
    newpacket = (packet *)malloc(sizeof(packet));
    newpacket->inter_arrival_time = packet_arrival * 1000;
    newpacket->tokens = P;
    newpacket->service = service_time * 1000;
    newpacket->count = i + 1;

    if (newpacket->tokens <= B) // if P less than token bucket capacity
    {
      pthread_mutex_lock(&m);
      printf("%012.03fms: p%d arrives, needs %d tokens, inter-arrival time = "
             "%0.03fms\n",
             (t2 - t) * 1000, newpacket->count, newpacket->tokens,
             inter_arrival);
      pthread_mutex_unlock(&m);

      gettimeofday(&timer, NULL);
      t3 = timer.tv_sec + timer.tv_usec / 1000000.00;

      newpacket->start_timeinq1 = (t3 - t) * 1000;
      newpacket->start_timeinq2 = 0.00;
      newpacket->timeinq1 = 0.00;
      newpacket->timeinq2 = 0.00;

      pthread_mutex_lock(&m);
      ListAppend(&Q1, (void *)newpacket); // enter q1
      q1++;
      printf("%012.03fms: p%d enters Q1\n", (t3 - t) * 1000, newpacket->count);
      elem = ListFirst(&Q1);
      current = (packet *)(elem->obj);
      if (current->tokens <= TOK) {
        // to receive the required tokens from bucket
        for (k = 0; k < current->tokens; k++) {
          elem2 = ListFirst(&Bucket);
          TOK--;
          ListUnlink(&Bucket, elem2);
        }
        ListUnlink(&Q1, elem); // remove from q1
        last_q1++;
        gettimeofday(&timer, NULL);
        t4 = timer.tv_sec + timer.tv_usec / 1000000.00;
        printf("%012.03fms: p%d leaves Q1, time in Q1 = %0.03fms, token bucket "
               "now has %d tokens\n",
               (t4 - t) * 1000, current->count,
               (t4 - t) * 1000 - current->start_timeinq1, TOK);
        time_q1 = (t4 - t) * 1000 - current->start_timeinq1;
        current->timeinq1 = time_q1;
        gettimeofday(&timer, NULL);
        t5 = timer.tv_sec + timer.tv_usec / 1000000.00;
        current->start_timeinq2 = (t5 - t) * 1000;
        ListAppend(&Q2, (void *)current);
        printf("%012.03fms: p%d enters Q2\n", (t5 - t) * 1000,
               current->count);      // enters q2
        pthread_cond_broadcast(&cv); // signal server to wake up
      }
      pthread_mutex_unlock(&m); // unlock

    } else { // packets drpped

      pthread_mutex_lock(&m);
      printf("%012.03fms: p%d arrives, needs %d tokens, inter-arrival time = "
             "%0.03fms, dropped\n",
             (t2 - t) * 1000, newpacket->count, newpacket->tokens,
             inter_arrival);
      pthread_mutex_unlock(&m);

      packet_dropped++;
      actual_packet--;
      if (packet_dropped == n) {
        stop = 1;
        pthread_cond_broadcast(&cv);
        ListUnlinkAll(&Bucket);
        pthread_cancel(token_arrival_thread);
      }

      if (ListLength(&Q1) == 0 && newpacket->count == n) {
        stop = 1;
        pthread_cond_broadcast(&cv);
        ListUnlinkAll(&Bucket);
        pthread_cancel(token_arrival_thread);
      }
    }
  }

  pthread_exit(NULL);
  return 0;
}
/***********************************/
// the token depositing thread would sleep so that it can wake up at a time such
// that the inter-arrival time between consecutive tokens is 1/r seconds and
// would try to deposit one token into the token bucket each time it wakes up.
void *token_deposit() {
  ListElem *elem = NULL;  // Q1
  ListElem *elem2 = NULL; // Q2
  packet *current = NULL; // Q2 obj

  double inter_token_arrival = 0.00;
  double token_time = 0.00;
  double time_q1 = 0.00;
  double t6 = 0.00, t7 = 0.00, t8 = 0.00, t9 = 0.00; // timestamp
  double overhead_t = 0.00;                          // for bookkeeping
  int times = 0; // token arrival updated time
  int k = 0;

  while (1) {
    // Token arrival
    gettimeofday(&timer, NULL);
    t6 = timer.tv_sec + timer.tv_usec / 1000000.00;
    times = token_arrival * 1000 - overhead_t;
    if (times > 0.00) {
      usleep(times * 1000);
    } else {
      usleep(0);
    }
    gettimeofday(&timer, NULL);
    t9 = (timer.tv_sec + timer.tv_usec / 1000000.0);
    overhead_t = ((t9 - t) * 1000 - (t6 - t) * 1000) - times;
    token_time = (t7 - t6) * 1000;
    inter_token_arrival = inter_token_arrival + token_time;

    if (TOK < B) // Check the capacity
    {
      pthread_mutex_lock(&m); // lock

      TOK++;                                 // Increment current token list
      token_no++;                            // Total token
      ListAppend(&Bucket, (void *)token_no); // add in the bucket

      if (TOK == 1)
        printf("%012.03fms: token %d arrives, token bucket now has %d token\n",
               (t9 - t) * 1000, token_no, TOK);
      if (TOK > 1)
        printf("%012.03fms: token %d arrives, token bucket now has %d tokens\n",
               (t9 - t) * 1000, token_no, TOK);

      if (ListLength(&Q1) != 0) {

        elem = ListFirst(&Q1);
        current = (packet *)(elem->obj);

        if (current->tokens <= TOK) // Q1 takes required tokens
        {
          for (k = 0; k < current->tokens; k++) {
            elem2 = ListFirst(&Bucket);
            TOK--;
            ListUnlink(&Bucket, elem2);
          }
          ListUnlink(&Q1, elem); // remove from q1
          last_q1++;
          gettimeofday(&timer, NULL);
          t7 = timer.tv_sec + timer.tv_usec / 1000000.00;
          printf("%012.03fms: p%d leaves Q1, time in Q1 = %0.03fms, token "
                 "bucket now has %d tokens\n",
                 (t7 - t) * 1000, current->count,
                 (t7 - t) * 1000 - current->start_timeinq1, TOK);
          time_q1 =
              (t7 - t) * 1000 - current->start_timeinq1; // Total time in q1
          current->timeinq1 = time_q1;
          gettimeofday(&timer, NULL);
          t8 = timer.tv_sec + timer.tv_usec / 1000000.00;
          current->start_timeinq2 = (t8 - t) * 1000;
          ListAppend(&Q2, (void *)current); // enters q2
          printf("%012.03fms: p%d enters Q2\n", (t8 - t) * 1000,
                 current->count);
          pthread_cond_broadcast(&cv);
        }
      }
      pthread_mutex_unlock(&m);
      // Tokens dropped
    } else {
      pthread_mutex_lock(&m);
      token_no++;
      token_dropped++;
      printf("%012.03fms: token %d arrives, dropped\n", (t9 - t) * 1000,
             token_no);
      pthread_mutex_unlock(&m);
    }

    if (last_q1 == actual_packet) {
      break;
    }
  }

  pthread_exit(NULL);
  return 0;
}
/***********************************/
// server1
void *s1() {
  double t10 = 0.00, t11 = 0.00, t12 = 0.00; // timestamp
  double s_time = 0.00;                      // Service time for a packet
  double servicing = 0.00;                   // Total service time
  double time_q2 = 0.00;                     // Time spent in q2
  double timeinsys = 0.00;                   // timeinsys for each packet
  ListElem *elem = NULL;                     // Q2
  packet *current = NULL;                    // Q2 obj

  while (1) {

    pthread_mutex_lock(&m);
    while (ListLength(&Q2) == 0) // Conditional wait
    {
      if (stop == 1) {
        flag = 1;
        break;
      }

      pthread_cond_wait(&cv, &m);
    }

    if (flag == 1) {
      pthread_mutex_unlock(&m); // Break loop
      break;
    }

    pthread_mutex_unlock(&m);

    pthread_mutex_lock(&m);

    if (ListLength(&Q2) != 0) {
      elem = ListFirst(&Q2);
      current = (packet *)(elem->obj);
      s_time = current->service;
      ListUnlink(&Q2, elem); // leaves q2
      last_q2++;
      gettimeofday(&timer, NULL);
      t10 = timer.tv_sec + timer.tv_usec / 1000000.00;
      /*
      The value printed for "time in Q2" must equal to the timestamp of the "p1
      leaves Q2" event minus the timestamp of
      the "p1 enters Q2" event.
      */
      printf("%012.03fms: p%d leaves Q2, time in Q2 = %.03fms\n",
             (t10 - t) * 1000, current->count,
             (t10 - t) * 1000 - current->start_timeinq2);
      time_q2 = (t10 - t) * 1000 - current->start_timeinq2;
      current->timeinq2 = time_q2;
    }

    pthread_mutex_unlock(&m);

    gettimeofday(&timer, NULL);
    t11 = timer.tv_sec + timer.tv_usec / 1000000.00;

    // The value printed for "requesting ???ms of service" must be the requested
    // service time (which must be an integer) of the corresponding packet.
    printf("%012.03fms: p%d begins service at s1, requesting %0.03fms of "
           "service\n",
           (t11 - t) * 1000, current->count, current->service);
    usleep(s_time * 1000); // servicing
    gettimeofday(&timer, NULL);
    t12 = timer.tv_sec + timer.tv_usec / 1000000.00;
    // The value printed for "service time" must equal to the timestamp of the
    // "p1 departs from S1" event minus the timestamp of the "p1 begins service
    // at S1" event (and it should be larger than the requested service time
    // printed for the "begin service" event).
    printf("%012.03fms: p%d departs from s1, service time = %.03fms, time in "
           "system = %0.03fms\n",
           (t12 - t) * 1000, current->count,
           (t12 - t) * 1000 - (t11 - t) * 1000,
           (t12 - t) * 1000 - current->start_timeinq1);
    servicing = (t12 - t) * 1000 - (t11 - t) * 1000;
    Total_pst = Total_pst + servicing; // Total service time
    Total_time_s1 = Total_time_s1 + servicing;
    Total_time_q1 = Total_time_q1 + current->timeinq1; // Total time in Q1
    Total_time_q2 = Total_time_q2 + current->timeinq2; // Total time in Q2
    timeinsys = (t12 - t) * 1000 - current->start_timeinq1;
    // The value printed for "time in system" must equal to the timestamp of the
    //"p1 departs from S1" event minus the timestamp of the "p1 arrives" event;
    Total_timeinsystem = Total_timeinsystem + timeinsys; // Total time in system
    Total_sqtimeinsystem =
        Total_sqtimeinsystem +
        (timeinsys * timeinsys); // Total squared time in system
    server_count++;              // Completed packets

    pthread_mutex_lock(&m);
    if (last_q2 == actual_packet || stop == 1 || q1 == 0) {
      stop = 1;
      pthread_cancel(token_arrival_thread);
      if (!ListEmpty(&Bucket))
        ListUnlinkAll(&Bucket);
      pthread_cond_broadcast(&cv);
      pthread_mutex_unlock(&m);
      break;
    }

    pthread_mutex_unlock(&m);
  }
  pthread_exit(NULL);
  return 0;
}
/***********************************/
// server2
void *s2() {
  double t13 = 0.00, t14 = 0.00, t15 = 0.00; // Timestamp
  double s_time = 0.00;                      // service time
  double servicing = 0.00; // Total service time real for a packet
  double time_q2 = 0.00;   // Time in q2
  double timeinsys = 0.00; // Time in sys real value
  ListElem *elem = NULL;   // Q2
  packet *current = NULL;  // Q2 obj

  while (1) {

    pthread_mutex_lock(&m); // lock

    while (ListLength(&Q2) == 0) {
      if (stop == 1) {
        flag = 1;
        break;
      }

      if (packet_dropped == n) {
        flag = 1;
        break;
      }

      pthread_cond_wait(&cv, &m); // Conditional wait
    }

    if (flag == 1) {
      pthread_mutex_unlock(&m);
      break;
    }
    pthread_mutex_unlock(&m);

    pthread_mutex_lock(&m);

    if (ListLength(&Q2) != 0) {
      elem = ListFirst(&Q2);
      current = (packet *)(elem->obj);
      s_time = current->service;
      ListUnlink(&Q2, elem); // remove from q2
      last_q2++;
      gettimeofday(&timer, NULL);
      t13 = timer.tv_sec + timer.tv_usec / 1000000.00;
      printf("%012.03fms: p%d leaves Q2, time in Q2 = %.03fms\n",
             (t13 - t) * 1000, current->count,
             (t13 - t) * 1000 - current->start_timeinq2);
      time_q2 = (t13 - t) * 1000 - current->start_timeinq2;
      current->timeinq2 = time_q2;
    }

    pthread_mutex_unlock(&m);

    gettimeofday(&timer, NULL);
    t14 = timer.tv_sec + timer.tv_usec / 1000000.00;
    printf("%012.03fms: p%d begins service at s2, requesting %0.03fms of "
           "service\n",
           (t14 - t) * 1000, current->count, current->service);
    usleep(s_time * 1000);
    gettimeofday(&timer, NULL);
    t15 = timer.tv_sec + timer.tv_usec / 1000000.00;
    printf("%012.03fms: p%d departs from s2, service time = %.03fms, time in "
           "system = %0.03fms\n",
           (t15 - t) * 1000, current->count,
           (t15 - t) * 1000 - (t14 - t) * 1000,
           (t15 - t) * 1000 - current->start_timeinq1);
    servicing = (t15 - t) * 1000 - (t14 - t) * 1000;   // real time service
    Total_pst = Total_pst + servicing;                 // Total service time
    Total_time_q1 = Total_time_q1 + current->timeinq1; // total time in q1
    Total_time_q2 = Total_time_q2 + current->timeinq2; // Total time in q2
    Total_time_s2 = Total_time_s2 + servicing;         // Total time in server2
    timeinsys = (t15 - t) * 1000 - current->start_timeinq1;
    Total_timeinsystem = Total_timeinsystem + timeinsys; // total time in system
    Total_sqtimeinsystem =
        Total_sqtimeinsystem + (timeinsys * timeinsys); // Total sq time in sys
    server_count++;                                     // completed packets

    pthread_mutex_lock(&m);

    if (last_q2 == actual_packet || stop == 1) {
      stop = 1;
      pthread_cancel(token_arrival_thread);
      if (!ListEmpty(&Bucket))
        ListUnlinkAll(&Bucket);
      pthread_cond_broadcast(&cv);
      pthread_mutex_unlock(&m);
      break;
    }

    pthread_mutex_unlock(&m);
  }

  pthread_exit(NULL);
  return 0;
}
/***********************************/
void statistics() {

  double avg_piat = 0.00;
  double avg_pst = 0.00;
  double avg_q1 = 0.00;
  double avg_q2 = 0.00;
  double avg_s1 = 0.00;
  double avg_s2 = 0.00;
  double avg_tis = 0.00;
  double avg_sqtis = 0.00;
  double var_tis = 0.00;
  double std_tis = 0.00;
  double token_drop_prob = 0.00;
  double packet_drop_prob = 0.00;
  int flag_var = 0;
  double std_0 = 0.00;

  avg_piat = (Total_piat / n) / 1000; // average packet inter-arrival time
  avg_pst = (Total_pst / server_count) / 1000; // average packet service time

  token_drop_prob = ((double)token_dropped / token_no);
  packet_drop_prob = ((double)packet_dropped / (n - removed_packets));

  avg_q1 =
      (Total_time_q1 / ((end - t) * 1000)); // average number of packets in Q1
  avg_q2 =
      (Total_time_q2 / ((end - t) * 1000)); // average number of packets in Q2
  avg_s1 =
      (Total_time_s1 / ((end - t) * 1000)); // average number of packets in S1
  avg_s2 =
      (Total_time_s2 / ((end - t) * 1000)); // average number of packets in S2

  avg_tis = (Total_timeinsystem /
             server_count); // average time a packet spent in system
  avg_sqtis = (Total_sqtimeinsystem /
               server_count); // average squared time a packet spent in system
  var_tis = avg_sqtis -
            (avg_tis * avg_tis); // var of average time a packet spent in system
  if (var_tis == 0.00 || var_tis < 0.00)
    flag_var = 1;
  std_tis = sqrt(
      var_tis); // standard deviation of average time a packet spent in system

  printf("Statistics:\n\n");
  if (n)
    printf("     average packet inter-arrival time = %0.6g\n", avg_piat);
  else
    printf("     average packet inter-arrival time = N/A, No packets arrived "
           "at this facility\n");
  if (server_count)
    printf("     average packet service time = %0.6g\n\n", avg_pst);
  else
    printf(
        "     average packet service time = N/A, No packets were serviced\n\n");
  if ((end - t) * 1000) {
    printf("     average number of packets in Q1 = %0.6g\n", avg_q1);
    printf("     average number of packets in Q2 = %0.6g\n", avg_q2);
    printf("     average number of packets at S1 = %0.6g\n", avg_s1);
    printf("     average number of packets at S2 = %0.6g\n\n", avg_s2);
  } else {
    printf("     average number of packets in Q1 = N/A, Total Emulation time "
           "is zero\n");
    printf("     average number of packets in Q2 = N/A, Total Emulation time "
           "is zero\n");
    printf("     average number of packets at S1 = N/A, Total Emulation time "
           "is zero\n");
    printf("     average number of packets at S2 = N/A, Total Emulation time "
           "is zero\n\n");
  }
  if (server_count) {
    printf("     average time a packet spent in system = %0.6g\n",
           avg_tis / 1000);
    if (flag_var == 0)
      printf("     standard deviation for time spent in system = %0.6g\n\n",
             std_tis / 1000);
    else
      printf("     standard deviation for time spent in system = %0.6g\n\n",
             std_0);
  } else {
    printf("     average time a packet spent in system = N/A, no packets were "
           "processed at the facilities s1 and s2\n");
    printf("     standard deviation for time spent in system = N/A, no packets "
           "were processed at the facilities s1 and s2\n\n");
  }
  if (token_no)
    printf("     token drop probability = %0.6g\n", token_drop_prob);
  else
    printf("     token drop probability = N/A, No tokens generated at this "
           "facility\n");
  if (n - removed_packets)
    printf("     packet drop probability = %0.6g\n", packet_drop_prob);
  else
    printf("     packet drop probability = N/A, No packets arrived at this "
           "facility\n");
}
/***********************************/
// When abruptly terminated
void drop() {
  int l = 0, m = 0;
  double t16 = 0.00;
  double t17 = 0.00;
  int listq1 = 0;
  int listq2 = 0;
  ListElem *elem = NULL;
  ListElem *elem1 = NULL;
  packet *current = NULL;
  packet *current1 = NULL;

  listq1 = ListLength(&Q1);
  listq2 = ListLength(&Q2);

  for (l = 0; l < listq1; l++) // Remove q1
  {
    elem = ListFirst(&Q1);
    current = (packet *)(elem->obj);
    gettimeofday(&timer, NULL);
    t16 = timer.tv_sec + timer.tv_usec / 1000000.00;
    printf("%012.03fms: p%d removed from Q1\n", (t16 - t) * 1000,
           current->count);
    ListUnlink(&Q1, elem);
  }
  for (m = 0; m < listq2; m++) // remove q2
  {
    elem1 = ListFirst(&Q2);
    current1 = (packet *)(elem1->obj);
    gettimeofday(&timer, NULL);
    t17 = timer.tv_sec + timer.tv_usec / 1000000.00;
    printf("%012.03fms: p%d removed from Q2\n", (t17 - t) * 1000,
           current1->count);
    ListUnlink(&Q2, elem1);
  }
}
/***********************************/
// Sinal catch for Ctrl+C
void *monitor() {
  int sig = 0;
  sigwait(&set, &sig);
  pthread_mutex_lock(&m);
  stop = 1;
  pthread_cond_broadcast(&cv);
  pthread_cancel(packet_arrival_thread);
  pthread_cancel(token_arrival_thread);
  removed_packets = ListLength(&Q1) + ListLength(&Q2);
  printf("\n");
  drop_flag = 1;
  if (!ListEmpty(&Bucket))
    ListUnlinkAll(&Bucket);
  pthread_mutex_unlock(&m);
  // Unblocking SIGINT (only in this thread)
  pthread_sigmask(SIG_UNBLOCK, &set, NULL);
  pthread_exit(NULL);
  return 0;
}

/************************************Main**************************************/
int main(int argc, char *argv[]) {

  int i = 0, j = 0;
  int trace = 0;

  char filename[1024];
  char send_buf[1024];
  char num[1024];
  char *token;
  int od = 0;

  int status = 0;
  struct stat st_buf;

  // Initializing lists
  memset(&Q1, 0, sizeof(List));
  memset(&Bucket, 0, sizeof(List));
  memset(&Q2, 0, sizeof(List));
  ListInit(&Q1);
  ListInit(&Bucket);
  ListInit(&Q2);

  // To access a directory
  struct dirent *pDirent;
  DIR *FD;

  // Initialize signal catching
  sigemptyset(&set);
  // Set SIGINT signal to 1 to block it
  sigaddset(&set, SIGINT);

  // Block all threads
  sigprocmask(SIG_BLOCK, &set, 0);

  // Default parameters
  lambda = 1.00;
  mu = 0.35;
  r = 1.50;
  B = 10;
  P = 3;
  n = 20;

  // Command line arguments are read
  for (i = 0; i < argc; i++) {

    if (i % 2 != 0) {
      if (argv[i][0] == '-') {
        if (strcmp(argv[i], "-t") == 0) {
          if (argv[i + 1] == NULL) {
            fprintf(stderr, "No Argument entered for the filename\n");
            exit(-1);
          }
          strcpy(filename, argv[i + 1]);
          trace = 1;

        } else if (strcmp(argv[i], "-n") == 0) {
          if (argv[i + 1] == NULL) {
            fprintf(stderr, "No Argument entered for number of packets\n");
            exit(-1);
          }
          n = atoi(argv[i + 1]);
          if (n == 0) {
            if (strcmp("0", argv[i + 1]) == 0) {
              fprintf(stderr, "Number of packets n=0\n");
              exit(-1);
            } else {
              fprintf(
                  stderr,
                  "Argument entered for number of packets is not an integer\n");
              exit(-1);
            }
          }
          if (n < 0) {
            fprintf(stderr, "num: is less than 0\n");
            exit(-1);
          }
          if (n > MAX) {
            fprintf(stderr, "num: is greater than the maximum value %d\n", MAX);
            exit(-1);
          }
        } else if (strcmp(argv[i], "-lambda") == 0) {
          if (argv[i + 1] == NULL) {
            fprintf(stderr, "No Argument entered for lambda\n");
            exit(-1);
          }
          lambda = atof(argv[i + 1]);
          if (lambda == 0) {
            if (strcmp("0", argv[i + 1]) == 0) {
              fprintf(stderr, "Packet arrival rate lambda=0\n");
              exit(-1);
            } else {
              fprintf(stderr,
                      "Argument entered for lambda is not a real value\n");
              exit(-1);
            }
          }
          if (lambda < 0) {
            fprintf(stderr, "lambda: is less than 0\n");
            exit(-1);
          }
        } else if (strcmp(argv[i], "-mu") == 0) {
          if (argv[i + 1] == NULL) {
            fprintf(stderr, "No Argument entered for mu\n");
            exit(-1);
          }
          mu = atof(argv[i + 1]);
          if (mu == 0) {
            if (strcmp("0", argv[i + 1]) == 0) {
              fprintf(stderr, "Service rate mu=0\n");
              exit(-1);
            } else {
              fprintf(stderr, "Argument entered for mu is not a real value.\n");
              exit(-1);
            }
          }
          if (mu < 0) {
            fprintf(stderr, "mu: is less than 0\n");
            exit(-1);
          }
        } else if (strcmp(argv[i], "-r") == 0) {
          if (argv[i + 1] == NULL) {
            fprintf(stderr, "No Argument entered for r\n");
            exit(-1);
          }
          r = atof(argv[i + 1]);
          if (r == 0) {
            if (strcmp("0", argv[i + 1]) == 0) {
              fprintf(stderr, "Token deposit rate r=0\n");
              exit(-1);
            } else {
              fprintf(stderr, "Argument entered for r is not a real value\n");
              exit(-1);
            }
          }
          if (r < 0) {
            fprintf(stderr, "r: is less than 0\n");
            exit(-1);
          }
        } else if (strcmp(argv[i], "-B") == 0) {
          if (argv[i + 1] == NULL) {
            fprintf(stderr, "No Argument entered for B\n");
            exit(-1);
          }
          B = atoi(argv[i + 1]);
          if (B == 0) {
            if (strcmp("0", argv[i + 1]) == 0) {
              fprintf(stderr, "Max number of tokens in a bucket B=0\n");
              exit(-1);
            } else {
              fprintf(stderr, "Argument entered for max number of tokens is "
                              "not an integer\n");
              exit(-1);
            }
          }
          if (B < 0) {
            fprintf(stderr, "B: is less than 0\n");
            exit(-1);
          }
          if (B > MAX) {
            fprintf(stderr, "B: is greater than the maximum value %d\n", MAX);
            exit(-1);
          }
        } else if (strcmp(argv[i], "-P") == 0) {
          if (argv[i + 1] == NULL) {
            fprintf(stderr, "No Argument entered for P\n");
            exit(-1);
          }
          P = atoi(argv[i + 1]);
          if (P == 0) {
            if (strcmp("0", argv[i + 1]) == 0) {
              fprintf(stderr, "number of tokens per packet P=0\n");
              exit(-1);
            } else {
              fprintf(stderr, "Argument entered for number of tokens per "
                              "packet is not an integer\n");
              exit(-1);
            }
          }
          if (P < 0) {
            fprintf(stderr, "P: is less than 0\n");
            exit(-1);
          }
          if (P > MAX) {
            fprintf(stderr, "P: is greater than the maximum value %d\n", MAX);
            exit(-1);
          }
        } else {
          fprintf(stderr,
                  "The command arguments provided by the user is invalid\nThe "
                  "arguments can either be:\n -n, -lambda, -mu, -P, -r, -B or "
                  "-t and their respective real values\nQuit Execution\n");
          exit(-1);
        }
      } else {
        fprintf(stderr,
                "The command arguments provided by the user is invalid\n");
        exit(-1);
      }
    }
  }

  /* At emulation time 0, all 4 threads (arrival thread, token depositing thread
  , and servers S1 and S2 threads) get started. */

  // Trace mode (file is used)
  if (trace == 1) {
    n = 0;
    mu = 0.00;
    lambda = 0.00;
    P = 0;
    status = stat(filename, &st_buf);
    if (status != 0) {
      fprintf(stderr, "Input file '%s': %s\n", filename, strerror(errno));
      exit(-1);
    }

    // Check if it is a directory or a file
    if (S_ISDIR(st_buf.st_mode)) {
      FD = opendir(filename); // open directory
      if (FD == NULL) {
        fprintf(stderr, "'%s':%s.\n", filename, strerror(errno));
        closedir(FD);
        exit(-1);
      }

      if ((pDirent = readdir(FD)) != NULL) {
        od = atoi(pDirent->d_name);
        if (od == 0) {
          fprintf(stderr, "Line 1 is not a number\n");
          exit(-1);
        }
      }

      closedir(FD);
      exit(-1);
    }

    // Open the file
    fp = fopen(filename, "r");
    if (fp == NULL) {
      fprintf(stderr, "Input file '%s': %s\n", filename, strerror(errno));
      exit(-1);
    }

    // Get packet number
    if (fgets(send_buf, sizeof(send_buf), fp) != NULL) {
      token = strtok(send_buf, " \t\n");
      strcpy(num, token);
      for (j = 0; j < strlen(num); j++) {
        if (!isdigit(num[j])) {
          fprintf(stderr, "Line 1 '%s': is not a number\n", send_buf);
          exit(-1);
        }
      }
      n = atoi(num);
    }

    if (n > MAX) {
      fprintf(stderr, "Line 1 num: is greater than the maximum value %d\n",
              MAX);
      exit(-1);
    }

    if (n == 0) {
      fprintf(stderr, "Number of packets n=0\n");
      exit(-1);
    }

    token_arrival = 1.00 / r;

    if (token_arrival > 10.00)
      token_arrival = 10.00;

    fprintf(stdout, "Emulation Parameters:\n");
    fprintf(stdout, "number to arrive = %d\n", n);
    fprintf(stdout, "r = %g\n", r);
    fprintf(stdout, "B = %d\n", B);
    fprintf(stdout, "tsfile = %s\n", filename);

    fprintf(stdout, "00000000.000ms: emulation begins\n");

    gettimeofday(&timer, NULL);
    t = (timer.tv_sec + timer.tv_usec / 1000000.0);

    pthread_create(&packet_arrival_thread, 0, arrival_trace_mode, NULL);

  } else { // Deterministic mode (file is not used)
    packet_arrival = 1.00 / lambda;
    token_arrival = 1.00 / r;
    service_time = 1.00 / mu;

    if (packet_arrival > 10.00)
      packet_arrival = 10.00;
    if (service_time > 10.00)
      service_time = 10.00;
    if (token_arrival > 10.00)
      token_arrival = 10.00;
    //
    fprintf(stdout, "Emulation Parameters:\n");
    fprintf(stdout, "number to arrive = %d\n", n);
    fprintf(stdout, "lambda = %g\n", lambda);
    fprintf(stdout, "mu = %g\n", mu);
    fprintf(stdout, "r = %g\n", r);
    fprintf(stdout, "B = %d\n", B);
    fprintf(stdout, "P = %d\n", P);
    fprintf(stdout, "00000000.000ms: emulation begins\n");
    gettimeofday(&timer, NULL);
    t = (timer.tv_sec + timer.tv_usec / 1000000.0);
    pthread_create(&packet_arrival_thread, 0, arrival_deterministic_mode, NULL);
  }
  pthread_create(&token_arrival_thread, 0, token_deposit, NULL);
  pthread_create(&server1_thread, 0, s1, NULL);
  pthread_create(&server2_thread, 0, s2, NULL);

  pthread_create(&monitoring, 0, monitor, 0);

  // Wait (sleep) for thread to die before leaving scope of the main() thread
  // The 2nd argument 0 means that we're not interested in the return exit code
  pthread_join(packet_arrival_thread, 0);
  pthread_join(token_arrival_thread, 0);
  pthread_join(server1_thread, 0);
  pthread_join(server2_thread, 0);

  if (drop_flag == 1)
    drop();

  gettimeofday(&timer, NULL);
  end = (timer.tv_sec + timer.tv_usec / 1000000.0);
  fprintf(stdout, "%012.03fms: emulation ends\n\n", (end - t) * 1000);

  statistics();

  return (0);
}
