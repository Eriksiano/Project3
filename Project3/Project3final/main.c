//
// How to use:
// Compile with:
// "make app"
// Run with
// "./app"
// 
// "./app reset" forces the application to reset the counters
//
// otherwise the application creates three processes that emits
// 100'000 signals total
// of SIGUSR1 and SIGUSR2
// ---
// Use CTRL-C to force exit
// CTRL-C May be needed to end the signal handling and the reporting processes
//

#include "header.h"

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

    //printf( "\t\tProcess %i ENTERING inc_counter() critical section...\n", pid );

    int shm_fd = shm_open( COUNTER_FILE, O_RDWR, 0666 );

    if ( shm_fd == -1 ) 
    {
        sem_post( mutex_sem );
        fail( "shm_open()" );
    }

    int *counter_address = (int *)mmap( 0, COUNTER_AMOUNT, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0 );

    // - Increment the value

    ++counter_address[ index ];
    //printf( "Increment counter %i, updated value: %i\n", index, ++counter_address[index] );

    munmap( counter_address, COUNTER_AMOUNT );
    close( shm_fd );

    //printf("\t\tProcess %i LEAVING inc_counter() critical section...\n", pid);
 
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

void sigusr_report_handler( int signum  ) 
{
    printf("\tProcess %i sigusr handler, signal %i\n", getpid(), signum );
}

/**
 * SIGINT Callback, resets the counters and stops the child process loops,
 * Sleeps for a second and exits the progarm
 * 
 * @param int signum 
 */
void sigint_handler( int signum )
{
    remove_counters();
    child_loop = 0;
    usleep( 1000000 );
    printf("\nExiting\n");
    exit( EXIT_SUCCESS );
}

// --------------------------------------------------------------------------------
// -
// - Child process loop utility functions
// -
// --------------------------------------------------------------------------------

/**
 * Returns the current timestamp in microseconds
 * 
 * @return uint
 */
uint get_timestamp()
{
    struct timeval tv;
    gettimeofday( &tv, NULL );

    return tv.tv_sec * (uint)1000000 + tv.tv_usec;
}

/**
 * Returns the average time interval from a list of timestamps
 * 
 * @param  uint[] timelist
 * @param  uint   count
 * @return uint
 */
uint get_avg_interval( uint time_list[], uint count )
{
    if ( count < 1 ) return 0;

    uint avg = 0;

    for ( register uint i = 0; i < count - 1; i++ )
    {
        avg += time_list[ i + 1 ] - time_list[ i ];
    }

    return avg / count;
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
void print_report( uint sigusr1_avg_us, uint sigusr2_avg_us )
{
    // ---------------------------
    // - Report the system time
    // ---------------------------

    time_t now = time( NULL );
    char*  time_string = ctime( &now );

    time_string[ strlen( time_string ) - 1 ] = '\0';

    printf( "\tCurrent time: %s\n", time_string );

    printf( "\tAverage interval between SIGUSR1 emissions: %ius\n", sigusr1_avg_us );
    printf( "\tAverage interval between SIGUSR2 emissions: %ius\n", sigusr2_avg_us );

    // ---------------------------
    // - Report the counter values
    // ---------------------------

    printf( "Generator counter SIGUSR1: %i\n", read_counter( TX_COUNTER_SIGUSR1 ) );
    printf( "Generator counter SIGUSR2: %i\n", read_counter( TX_COUNTER_SIGUSR2 ) );
    printf( "Receiver  counter SIGUSR1: %i\n", read_counter( RX_COUNTER_SIGUSR1 ) );
    printf( "Receiver  counter SIGUSR2: %i\n", read_counter( RX_COUNTER_SIGUSR2 ) );

}



/**
 * Report loop function for the report process
 * 
 */ 
int report_loop()
{
    uint counter = 0;
    int sig_caught;
    sigset_t mask;
    uint sigusr1_count = 0;
    uint sigusr2_count = 0;
    uint sigusr1_timeval[10], sigusr2_timeval[10]; 

    // ----------------------------------------------------
    // - Make the process to respod to SIGUSR1 & SIGUSR2
    // ----------------------------------------------------

    sigemptyset( &mask);
    sigaddset( &mask, SIGUSR1 );
    sigaddset( &mask, SIGUSR2 );
    sigprocmask( SIG_BLOCK, &mask, NULL );

    printf( "\tReport process enters the loop\n" );

    while( child_loop )
    {
        sigwait( &mask, &sig_caught );

        if ( sig_caught == SIGUSR1 || sig_caught == SIGUSR2 )
        {
            // --------------------------------------------------------
            // - SIGUSR1 Caught, add the timestamp to the list
            // --------------------------------------------------------
            if ( sig_caught == SIGUSR1 )
            {
                //printf( "\tReporting loop looping, sigusr1 count %i\n", sigusr1_count);
                sigusr1_timeval[ sigusr1_count++ ] = get_timestamp();
            }

            // --------------------------------------------------------
            // - SIGUSR2 Caught, add the timestamp to the list
            // --------------------------------------------------------
            if ( sig_caught == SIGUSR2 )
            {
                //printf( "\tReporting loop looping, sigusr1 count %i\n", sigusr2_count);
                sigusr2_timeval[ sigusr2_count++ ] = get_timestamp();
            }

            // --------------------------------------------------------
            // - Ten signals caught, display a report
            // - And reset the counters
            // --------------------------------------------------------
            if ( ++counter > 10 )
            {
                
                print_report
                (
                    get_avg_interval( sigusr1_timeval, sigusr1_count ),
                    get_avg_interval( sigusr2_timeval, sigusr2_count )
                );

                sigusr1_count = 0;
                sigusr2_count = 0;
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
 * @param  int group
 * @return int exit success
 */
int signal_handler_loop( int group )
{
    printf( "\tSignal handler %i spawned in group %i\n", getpid(), group );

    // ----------------------------------------------------
    // - Make the group 1 respond to SIGUSR1 and
    // - And the orher group to SIGUSR2
    // ----------------------------------------------------

    sigset_t mask, oldmask;
    sigemptyset( &mask );

    if ( group == 1 )
    {
        signal( SIGUSR1, sigusr1_rx_handler );
        sigaddset( &mask, SIGUSR1 );
        sigprocmask( SIG_BLOCK, &mask, &oldmask );
    }
    else
    {
        signal( SIGUSR2, sigusr2_rx_handler );
        sigaddset( &mask, SIGUSR2 );
        sigprocmask( SIG_BLOCK, &mask, &oldmask );
    }

    // ------------------------------------------------------
    // - Listen to the signals
    // -------------------------------------------------------
    int sig_caught;

    while( child_loop )
    {
        sigwait( &mask, &sig_caught );

        if ( sig_caught == SIGUSR1 && group == 1)
        {
            inc_counter( RX_COUNTER_SIGUSR1 );
            //printf( "\tGroup %i caught SIGUSR1\n", group );
        }

        if ( sig_caught == SIGUSR2 && group == 0)
        {
            inc_counter( RX_COUNTER_SIGUSR2 );
            //printf( "\tGroup %i caught SIGUSR2\n", group );
        }

    }

    sigprocmask( SIG_UNBLOCK, &mask, NULL );
    printf( "Signal handler exited the loop\n" );
    exit( EXIT_SUCCESS );
}

/**
 * Loop function for the signal generator processes
 * 
 *
 */
int signal_generator_loop()
{
    pid_t pid = getpid();

    printf( "\tSignal generator child process %i starts...\n", pid );

    // -------------------------------------------------------------
    // - Enter the loop for 30 seconds OR 100'000 signal emissions
    // --------------------------------------------------------------

    while ( total_emissions++ < MAX_GENERATOR_LOOP )
    {
        // ---------------------------------------------------------
        // - Get the random sleep time
        // ---------------------------------------------------------
        usleep( get_sleep_time() );

        // ---------------------------------------------------------
        // - Get the random signal number between SIGUSR1 & SIGUSR2
        // ---------------------------------------------------------
        if ( get_random_signum() )
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

    // --------------------------------------------------------------
    // - Emit a few SIGUSR1 and SIGUSR2 to ensure the termination
    // - Of Signal reporting and signal handling processes
    // --------------------------------------------------------------

    for ( register uint i = 0; i < 10; i++ )
    {
        usleep(40000);
        kill( 0, SIGUSR1 );
        kill( 0, SIGUSR2 );
    }

    // -----------------------------------------------------
    // - Display that the proces has completed its task
    // - And exit
    // ---------------------------------------------------
    printf( "Terminating the process %i\n", pid );

    exit( EXIT_SUCCESS );
}

/**
 * The Application entry point
 * 
 * @param  int      argc command line argument count
 * @param  char **  argv command line argument vector
 */
int main( int argc, char *argv[] )
{
    pid_t pid;
    uint  process;
    sigset_t mask, oldmask;
    child_loop = 1;


    // --------------------------------------------------------------------------------
    // -
    // - Check here for command line arguments, if found, run special sub program
    // - for 'reset' (Force reset counters)
    // -
    // --------------------------------------------------------------------------------
    if ( argc > 1)
    {
        if ( strcmp( argv[1], "reset" ) == 0 )
        {
            printf( "Attempting to forcefully reset the counter file data\n") ;
            remove_counters();
            init_counters();

            printf( "Counter values:\n" );
            for ( register int i = 0; i < COUNTER_AMOUNT; i++ )
            {
                printf( "\tCounter %i: %i\n", (i + 1), read_counter( i ) );
            }

            printf( "\n\nExiting\n\n");
            return EXIT_SUCCESS;
        }

    }

    // -------------------------------------------------------------------
    // - Attach the custom SIGINT handler to perform counter clean-up
    // -------------------------------------------------------------------
    signal( SIGINT, sigint_handler );

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
        fail( "sem_open()" );
    }

    // --------------------------------------------------------------------
    // - Create a signal mask for the main process
    // --------------------------------------------------------------------
    sigemptyset( &mask );
    sigaddset( &mask, SIGUSR1 );
    sigaddset( &mask, SIGUSR2 );
    sigprocmask( SIG_BLOCK, &mask, &oldmask );


    // -------------------------------------------------------------------
    // - Create the shared memory and intializing its value
    // -------------------------------------------------------------------
    printf( "MAIN: Creating the shared memory file for counters\n\n\n" );
    init_counters();



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
    // -------------------------------------------------------------------
    printf( "MAIN: Waiting for the Child processes to complete...\n" );
    pid_t wpid;
    int status = 0;

    while( ( wpid = wait( &status ) ) > 0 )
    {
        printf( "\tMAIN: Child %i completed, status: %i\n\n", wpid, status );
    }

    printf( "MAIN: All child processes completed, main %i\n\n", getpid() );

    remove_counters();

    return 1;
}





