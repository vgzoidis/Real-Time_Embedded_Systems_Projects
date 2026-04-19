/*
 *	File	: prod-cons.c
 *	Title	: Producers/Consumers using pthreads
 *	Disc	: My solution to the sine calculating producers / infinite loop consumers problem
 *	Author: Zoidis Vasileios
 *	Date	: 19 April 2026
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>

#define QUEUESIZE 10
#define LOOP 10000 // A large number of iterations instead of sleeping

struct workFunction {
  void * (*work)(void *);
  void * arg;
};

void *producer (void *args);
void *consumer (void *args);
void *calculate_sine(void *arg);

typedef struct {
  struct workFunction buf[QUEUESIZE];
  struct timeval timeBuf[QUEUESIZE]; // Track time in parallel array 
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;

queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, struct workFunction in, struct timeval t_in);
void queueDel (queue *q, struct workFunction *out, struct timeval *t_out);

// Global statistics
long long total_wait_time_us = 0;
long total_consumed = 0;
pthread_mutex_t stats_mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t done_cond = PTHREAD_COND_INITIALIZER;

int p_threads = 2; // Default producers
int q_threads = 2; // Default consumers

int main (int argc, char *argv[])
{
  queue *fifo;
  pthread_t *pro, *con;

  if (argc > 1) p_threads = atoi(argv[1]);
  if (argc > 2) q_threads = atoi(argv[2]);

  printf("Running with %d producers and %d consumers.\n", p_threads, q_threads);

  fifo = queueInit ();
  if (fifo ==  NULL) {
    fprintf (stderr, "main: Queue Init failed.\n");
    exit (1);
  }

  pro = (pthread_t *)malloc(p_threads * sizeof(pthread_t));
  con = (pthread_t *)malloc(q_threads * sizeof(pthread_t));

  for (int i = 0; i < q_threads; i++) {
    pthread_create (&con[i], NULL, consumer, fifo);
  }
  for (int i = 0; i < p_threads; i++) {
    pthread_create (&pro[i], NULL, producer, fifo);
  }

  // Wait until all produced elements are consumed
  pthread_mutex_lock(&stats_mut);
  while (total_consumed < p_threads * LOOP) {
    pthread_cond_wait(&done_cond, &stats_mut);
  }
  pthread_mutex_unlock(&stats_mut);

  printf("All %ld items consumed.\n", total_consumed);
  printf("Average waiting time in queue: %.2f microseconds.\n", (double)total_wait_time_us / total_consumed);

  queueDelete (fifo);
  free(pro);
  free(con);
  
  return 0;
}

void *calculate_sine(void *arg) {
  double *angles = (double *)arg;
  double sum = 0.0;
  for (int i = 0; i < 10; ++i) {
    sum += sin(angles[i]);
  }
  free(arg); // Free the memory allocated by the producer
  return NULL;
}

void *producer (void *q)
{
  queue *fifo;
  int i;

  fifo = (queue *)q;

  for (i = 0; i < LOOP; i++) {
    struct workFunction wf;
    wf.work = calculate_sine;
    
    // Prepare arguments for out sine function
    double *angles = (double *)malloc(10 * sizeof(double));
    for (int j = 0; j < 10; j++) {
        angles[j] = (double)(rand() % 360) * M_PI / 180.0;
    }
    wf.arg = angles;

    pthread_mutex_lock (fifo->mut);
    while (fifo->full) {
      printf ("producer: queue FULL.\n");                 // Wait for an empty slot
      pthread_cond_wait (fifo->notFull, fifo->mut);
    }
    
    // Mark time just before putting into the queue
    struct timeval enqueue_time;
    gettimeofday(&enqueue_time, NULL);
    
    queueAdd (fifo, wf, enqueue_time);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notEmpty);
  }
  return (NULL);
}

void *consumer (void *q)
{
  queue *fifo;

  fifo = (queue *)q;

  while(1) {
    struct workFunction wf;
    struct timeval dequeue_time;
    struct timeval enqueue_time;

    pthread_mutex_lock (fifo->mut);
    while (fifo->empty) {
      printf ("consumer: queue EMPTY.\n");                // Wait for items
      pthread_cond_wait (fifo->notEmpty, fifo->mut);
    }
    queueDel (fifo, &wf, &enqueue_time);
    
    // Mark time just after removing from the queue (before execution)
    gettimeofday(&dequeue_time, NULL);
    
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notFull);

    // Calculate wait time
    long long wait_us = (dequeue_time.tv_sec - enqueue_time.tv_sec) * 1000000LL +
                        (dequeue_time.tv_usec - enqueue_time.tv_usec);

    pthread_mutex_lock(&stats_mut);
    total_wait_time_us += wait_us;
    total_consumed++;
    if (total_consumed == p_threads * LOOP) {
        pthread_cond_signal(&done_cond);
    }
    pthread_mutex_unlock(&stats_mut);

    // Execute the function
    if (wf.work) {
        wf.work(wf.arg);
    }
  }
  return (NULL);
}

queue *queueInit (void)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
	
  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queueAdd (queue *q, struct workFunction in, struct timeval t_in)
{
  q->buf[q->tail] = in;
  q->timeBuf[q->tail] = t_in;
  q->tail++;

  if (q->tail == QUEUESIZE)
    q->tail = 0;

  if (q->tail == q->head)
    q->full = 1;

  q->empty = 0;

  return;
}

void queueDel (queue *q, struct workFunction *out, struct timeval *t_out)
{
  *out = q->buf[q->head];
  *t_out = q->timeBuf[q->head];

  q->head++;

  if (q->head == QUEUESIZE)
    q->head = 0;

  if (q->head == q->tail)
    q->empty = 1;

  q->full = 0;

  return;
}
