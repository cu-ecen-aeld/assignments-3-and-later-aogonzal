#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
   
    // have func args. Need to wait "wait_to_obtain_ms" ms
    //call system call usleep() to wait
    //LSP pg381
    int rc = usleep(1000 * (thread_func_args->wait_to_obtain_ms));
    //throw error if return code is -1. 
    if(rc == -1)
    {
    	ERROR_LOG("Error while waiting before obtaining mutex");
    	thread_func_args->thread_complete_success = false;
    }    
    else
    {   // Wait successful, obtain mutex
    	//LSP pg236
    	rc = pthread_mutex_lock(thread_func_args->mutex);
        //throw error if return code is -1. 
        if(rc != 0)
    	{
    		ERROR_LOG("Error obtaining mutex lock. rc = %d",rc);
    		thread_func_args->thread_complete_success = false;
    	}
    	else
            	{
    		// Mutex obtained, wait to release
    		rc = usleep(1000 * (thread_func_args->wait_to_release_ms));
                //throw error if return code is -1. 
    		if(rc == -1)
    		{
    			ERROR_LOG("Error while waiting after releasing mutex");
    			thread_func_args->thread_complete_success = false;
    		}
    		else
    		{
    			// Waited successfully, now release mutex
    			//LSP pg236
    			rc = pthread_mutex_unlock(thread_func_args->mutex);
    			if(rc != 0)
    			{
    				ERROR_LOG("Error releasing mutex. rc = %d", rc);
    				thread_func_args->thread_complete_success = false;
    			}
    		}
    	}
    }
    
    //return thread_param;
    return thread_func_args;
}
        

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
     
    // Use malloc() to allocate memory for thread data
    //initialize thread data
    struct thread_data* threadfunc_args = (struct thread_data*)(malloc(sizeof(struct thread_data)));
    
    threadfunc_args->wait_to_obtain_ms = wait_to_obtain_ms;
    threadfunc_args->wait_to_release_ms = wait_to_release_ms;
    threadfunc_args->thread_complete_success = true; // Default case is true
    threadfunc_args->mutex = mutex; // Set mutex in the structure
    
    // Create the thread
    int rc = pthread_create (thread, NULL, threadfunc, threadfunc_args);
    if(rc != 0)
    {
    	ERROR_LOG("Error creating the thread. rc = %d", rc);
    	free(threadfunc_args);
    	return false;
    }
    
    return true;
}
