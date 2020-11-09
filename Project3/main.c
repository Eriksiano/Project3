/* Author:Eriksiano
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
#include <time.h>
#include <ctype.h>

// - Function declarations:
void report_loop();
void signal_handler_loop(int);
void signal_generator_loop();
void srand(unsigned);

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
#define fail(msg) {\
                    perror(msg);\
                    return EXIT_FAILURE; }

typedef unsigned int uint;

sem_t *mutex_sem;
uint   child_loop = 1;



/**
 * Create the shared memory for counters and initialize them to zero
 * 
 * @return int
 */
int init_counters()
{
    printf( "Initializing shared memory for counters\n" );

    int shm_fd = shm_open( COUNTER_FILE, O_CREAT | O_RDWR, 0666 );
    if ( shm_fd == -1 ) fail( "shm_open()" );

    int result = ftruncate( shm_fd, COUNTER_AMOUNT );
    if ( result == -1 ) fail( "ftruncate()" );

    int *counter_address = ( int * ) mmap( 0, COUNTER_AMOUNT, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0 );
 
    for ( register uint i = 0; i < COUNTER_AMOUNT; i++ )
    {
        counter_address[i] = 0;
    }

    munmap( counter_address, COUNTER_AMOUNT) ;
    close( shm_fd );

    return 1;
}

/**
 * Increments the counter specified by the param 'index'
 * Acceptable 'index' value 0...3
 * 
 * @param  int index
 * @return int
 */
int inc_counter( int index )
{
    if ( index < 0 || index >= COUNTER_AMOUNT ) return -1;

    sem_trywait( mutex_sem );

    // ----------------------------------------------
    // - Enter critical section
    // ----------------------------------------------

    pid_t pid = getpid();

    printf( "\t\tProcess %i ENTERING inc_counter() critical section...\n", pid );

    int shm_fd = shm_open( COUNTER_FILE, O_RDWR, 0666 );

    if ( shm_fd == -1 ) 
    {
        sem_post( mutex_sem );
        fail( "shm_open()" );
    }

    int *counter_address = (int *)mmap( 0, COUNTER_AMOUNT, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0 );

    printf( "Increment counter %i, updated value: %i\n", index, ++counter_address[index] );

    munmap( counter_address, COUNTER_AMOUNT );
    close( shm_fd );

    printf("\t\tProcess %i LEAVING inc_counter() critical section...\n", pid);
 
    // ----------------------------------------------
    // - Leave critical section
    // ----------------------------------------------

    sem_post( mutex_sem );

    return 1;
}

/**
 * Returns the value of a counter specified by the param 'index'
 * Acceptable range 0...3
 * 
 * @param  int index
 * @return int 
 */
int read_counter( int index )
{
    if ( index < 0 || index >= COUNTER_AMOUNT ) return -1;

    int shm_fd = shm_open( COUNTER_FILE, O_RDONLY, 0666 );
    if( shm_fd == -1 )
    {
        fail( "shm_open()" );
    }

    int *counter_address = (int *)mmap( 0, COUNTER_AMOUNT, PROT_READ, MAP_SHARED, shm_fd, 0 );
    int result = counter_address[index];

    munmap( counter_address, COUNTER_AMOUNT );
    close( shm_fd );

    return result;
}

/**
 * Remove the shared memory file from the os
 */
void remove_counters()
{
    printf( "Remove shared memory file %s\n", COUNTER_FILE );
    shm_unlink( COUNTER_FILE );
}


// -------------------------------------------------------------------
// -
// - Signal handler callbacks
// - 
// -------------------------------------------------------------------

/**
 * SIGUSR1 receive handler
 * 
 * @param int signum
 */
void sigusr1_rx_handler( int signum )
{
    inc_counter( RX_COUNTER_SIGUSR1 );  
}

/**
 * SIGUSR2 receive handler
 * 
 * @param int signum
 */
void sigusr2_rx_handler( int signum )
{
    inc_counter( RX_COUNTER_SIGUSR2 );
}


/**
 * The Application entry point
 * 
 */
int main()
{

    // -------------------------------------------------------------------
    // - Application requires random values in few cases, 
    // - Initializing the random generator seed with the current time
    // - To ensure truly random values
    // -------------------------------------------------------------------
    srand( time( NULL ) );


    // --------------------------------------------------------------------
    // - Initialize the semaphore
    // --------------------------------------------------------------------
    if ( ( mutex_sem = sem_open( SEM_NAME, O_CREAT, 0666, 0 ) ) == SEM_FAILED )
    {
        fail("sem_open()");
    }




    // -------------------------------------------------------------------
    // - Create the shared memory and intializing its value
    // -------------------------------------------------------------------
    printf( "MAIN: Creating the shared memory file for counters\n\n\n" );
    init_counters();


    // --------------------------------------------------------------------
    // - Create a signal mask for the main process
    // --------------------------------------------------------------------

    pid_t pid;
    uint  process;
    sigset_t mask, oldmask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    // -------------------------------------------------------------------
    // - Create the reporting process
    // -------------------------------------------------------------------

    printf( "Spawning the reporting process\n\n" );
    if ( fork() == 0 ) report_loop();

    // -------------------------------------------------------------------
    // - Create the four signal handler processes
    // -------------------------------------------------------------------

    printf( "Spawning the four signal handling processes\n\n" );
    process = RX_PROCESS_AMOUNT;

    while ( process-- > 0 )
    {
        printf( "Creating signal handler process %i type %i\n", RX_PROCESS_AMOUNT - process, process & 1 );
        if ( fork() == 0 ) signal_handler_loop( process & 1 );
    }

    // -------------------------------------------------------------------
    // - Create the three signal generator processes
    // -------------------------------------------------------------------

    printf( "Spawning the three signal generating processes\n\n" );
    process = TX_PROCESS_AMOUNT;

    while ( process-- > 0 )
    {
        printf( "Creating signal generator process %i\n", TX_PROCESS_AMOUNT - process );
        if ( fork() == 0 ) signal_generator_loop();
    }

    // -------------------------------------------------------------------
    // - Waiting for the child processes to exit
    // ------------------------------------------------------------------

    printf("MAIN: Waiting for the Child processes to complete...\n");
    pid_t wpid;
    int status = 0;

    while( (wpid = wait(&status)) > 0 )
    {
        printf( "\tMAIN: Child %i completed, status: %i\n\n", wpid, status );
    }

    printf( "MAIN: All child processes completed, main %i\n\n", getpid() );

    remove_counters();

    return 1;
}


/**
 * Returns a random number between 10'000...100'000
 * 
 * @return int
 */
int get_sleep_time()
{
    return 10000 + rand() % 90000;
}

/**
 * Returns a random number between 0 and 1
 * 
 * @return int
 */
int get_random_signum()
{
    return rand() % 2;
}

/**
 * Displays the current time in the terminal
 */
void print_report()
{
    time_t now = time( NULL );
    char*  time_string = ctime( &now );

    time_string[strlen( time_string ) - 1] = '\0';

    printf( "\tCurrent time: %s\n", time_string );
}

void sigusr_report_handler( int signum  ) 
{
    printf("\tProcess %i sigusr handler, signal %i\n", getpid(), signum );
}

/**
 * Report loop function for the report process
 * 
 */ 
void report_loop()
{
    uint counter = 0;
    int sig_caught;
    sigset_t mask;

    // ----------------------------------------------------
    // - Make the process to respod to SIGUSR1 & SIGUSR2
    // ----------------------------------------------------

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    printf( "Report process enters the loop\n" );
    while( child_loop )
    {
        sigwait( &mask, &sig_caught );
        if ( sig_caught == SIGUSR1 || sig_caught == SIGUSR2 )
        {
            if ( ++counter > 10 )
            {
                print_report();
                counter = 0;
            }
        }
    }

    printf( "Report process exited loop\n" );
    exit( 1 );    
}

/**
 * Loop function for signal handler processes
 * Param denotes the group
 * 
 * @param int group
 */
void signal_handler_loop( int group )
{
    pid_t pid = getpid();

    printf( "Signal handler %i spawned in group %i\n", pid, group );

    // ----------------------------------------------------
    // - Make the group 1 respond to SIGUSR1 and
    // - And the orher group to SIGUSR2
    // ----------------------------------------------------

    sigset_t mask;
    sigemptyset( &mask );
    sigprocmask( SIG_BLOCK, &mask, NULL );

    if ( group == 1 )
    {
        signal( SIGUSR1, sigusr1_rx_handler) ;
        sigaddset( &mask, SIGUSR1 );
    }
    else
    {
        signal( SIGUSR2, sigusr2_rx_handler) ;
        sigaddset( &mask, SIGUSR2 );
    }

    sigprocmask( SIG_BLOCK, &mask, NULL );

    // ------------------------------------------------------
    // - Listen to the signals
    // -------------------------------------------------------

    while( child_loop )
    {
        sigsuspend( &mask );
    }

    printf( "Signal handler exited the loop\n" );
    exit( 1 );
}

/**
 * Loop function for the signal generator processes
 * 
 *
 */
void signal_generator_loop()
{
    pid_t pid = getpid();
    uint loop = 0;
    uint total_count   = 0;
    uint sigusr1_count = 0;
    uint sigusr2_count = 0;

    printf( "\tSignal generator child process %i starts...\n", pid );

    // -------------------------------------------------------------
    // - Enter the loop for 30 seconds OR 100'000 signal emissions
    // --------------------------------------------------------------

    while ( total_count++ < 100000 )
    {
        uint sleep_time = get_sleep_time();
        usleep( sleep_time );

        // ---------------------------------------------------------
        // - Get the random signal number between SIGUSR1 & SIGUSR2
        // ---------------------------------------------------------

        uint signum = get_random_signum();
        
        printf( "\t\tGenerator(%i): sleep time %ius, emits signal %i\n\n", pid, sleep_time, signum );

        if ( signum == 1 )
        {
            inc_counter( TX_COUNTER_SIGUSR1 );
            kill( 0, SIGUSR1 );
        }
        else
        {
            inc_counter( TX_COUNTER_SIGUSR2 );
            kill( 0, SIGUSR2 );
        }
    }
    
    // --------------------------------------------------
    // - unset the other child processes loop condition
    // - In order to exit their loops
    // --------------------------------------------------

    child_loop = 0;

    // -----------------------------------------------------
    // - Display that the proces has completed its task
    // - And exit
    // ---------------------------------------------------
    printf( "Terminating the process %i\n", pid );

    exit(1);
}


