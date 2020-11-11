#ifndef _HEADER_H_
#define _HEADER_H_

/* Author:Eriksiano Kapaj
 * Date: 29-10-2020
 * Description:
 * Parent process
 * Spawns eight processes, in three categories:
 *  - Three signal generating processes
 *  - Four signal handling processes
 *  - One reporting process
 * Parent controls executing time.
 * Two signals will be used for IPC: SIGUSR1 and SIGUSR2 
 */
#include <fcntl.h>      /* for O_* constants */
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h> 
#include <sys/time.h>  
#include <time.h>
#include <ctype.h>

#define SEM_NAME           "/counter-semaphore"
#define COUNTER_AMOUNT     4
#define COUNTER_FILE       "/counters"
#define RX_COUNTER_SIGUSR1 0
#define RX_COUNTER_SIGUSR2 1
#define TX_COUNTER_SIGUSR1 2
#define TX_COUNTER_SIGUSR2 3
#define RUNTIME_IN_SECONDS 30
#define TX_PROCESS_AMOUNT  3
#define RX_PROCESS_AMOUNT  4
#define MAX_GENERATOR_LOOP 100000
#define fail(msg) {\
                    perror(msg);\
                    return EXIT_FAILURE; }

typedef unsigned int uint;

sem_t *mutex_sem;
uint   child_loop = 1;
uint   total_emissions = 0;

// -------------------------------------------
// - Function declarations
// -------------------------------------------

int  init_counters();
int  inc_counter( int index );
int  read_counter( int index );
void remove_counters();
void sigusr1_rx_handler( int signum );
void sigusr2_rx_handler( int signum );
void sigusr_report_handler( int signum  );
void sigint_handler( int signum );
uint get_timestamp();
uint get_vg_interval( uint time_list[], uint count );
int  get_sleep_time();
int  get_random_signum();
void print_report();
int  report_loop();
int  signal_handler_loop( int group );
int  signal_generator_loop();
void srand( unsigned );


#endif /* _HEADER_H_ */