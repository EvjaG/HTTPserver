/*  INCLUDES    */
#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
/*  globals     */
#define USEMESSAGE "Usage: threadpool <pool-size> <max-number-of-jobs>\n"

/*  DECLARATIONS */
void errorDestroy(char* msg,threadpool* threadp);
void* sleeper_wake_function(void* p);
typedef struct sleeperwakestruct {
    int* toStop;
    threadpool* tpool;
} sleeperwakestruct;


/*  FUNCTIONS   */
threadpool* create_threadpool(int num_threads_in_pool)
{
    if(num_threads_in_pool>MAXT_IN_POOL||num_threads_in_pool<=0)
    {
        // perror(USEMESSAGE);
        return NULL;
    };
    threadpool* toReturn = (threadpool*) calloc(1,sizeof(threadpool));//allocate mem for threadpool
    if(!toReturn)
        {
           errorDestroy("",NULL);
            return NULL;
        }
    //assign and allocate threads
    toReturn->num_threads   =   num_threads_in_pool;

    //set queue pointers to null
    toReturn->qhead=toReturn->qtail=NULL;
    if((pthread_mutex_init(&(toReturn->qlock),NULL)) < 0)
    {
       errorDestroy("Failed to create mutex",toReturn);
        return NULL;
    }
    if((pthread_cond_init(&(toReturn->q_empty),NULL)) < 0)
    {
       errorDestroy("Can't create cont_t",toReturn);
        return NULL;
    }
    if((pthread_cond_init(&(toReturn->q_not_empty), NULL)) < 0)
    {
       errorDestroy("Can't create cont_t",toReturn);
        return NULL;
    }

    toReturn->threads = (pthread_t*) malloc(sizeof(pthread_t)*toReturn->num_threads);
    if(!(toReturn->threads))
    {
       errorDestroy("Failed to create thread array",toReturn);
        return NULL;
    }

    //create threads
    for(int i=0;i<num_threads_in_pool;i++)
    {
        pthread_create(&(toReturn->threads[i]),NULL,do_work,toReturn);
    }

    return toReturn;
}

void destroy_threadpool(threadpool* destroyme)
{
    if(destroyme==NULL)
        return;
        //initialize sleeper thread
    
    //sleeper thread creation
    pthread_t sleeper;
	int joiningDone=0;

    sleeperwakestruct* forsleeperwake = (sleeperwakestruct*) malloc(sizeof(sleeperwakestruct));
    forsleeperwake->toStop=&joiningDone;
    forsleeperwake->tpool=destroyme;
    

    pthread_create(&(sleeper),NULL,sleeper_wake_function,forsleeperwake);
    destroyme->dont_accept=1;
    if(destroyme->qsize>0)
        pthread_cond_wait(&(destroyme->q_empty),&(destroyme->qlock));
    destroyme->shutdown=1;
    // pthread_cond_broadcast(&(destroyme->q_not_empty));
    pthread_mutex_unlock(&(destroyme->qlock));
    for(int i=0;i<destroyme->num_threads;i++)
    {
        pthread_mutex_unlock(&(destroyme->qlock));
        pthread_cond_signal(&(destroyme->q_not_empty));
        pthread_join(destroyme->threads[i],NULL);
        pthread_mutex_unlock(&(destroyme->qlock));
        pthread_cond_signal(&(destroyme->q_not_empty));
            // perror("%d joined\n",i);//****
    }
    joiningDone=1;
    pthread_mutex_unlock(&(destroyme->qlock));
    pthread_join(sleeper,NULL);
    // perror("all threads done!!\n");//****
    free(destroyme->threads);
    // sleep(4);
    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_empty));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    free(forsleeperwake);
    free(destroyme);
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
    if(from_me->dont_accept)
    {
        errorDestroy("Can't accept more jobs, destructor activated",NULL);
        return;
    }
    work_t* toPut = (work_t*) malloc (sizeof(work_t));
    if(toPut==NULL)
    {
        errorDestroy("Can't create work_t",NULL);
        return;
    }
    toPut->routine=dispatch_to_here;
    toPut->arg=arg;
    toPut->next=NULL;

    pthread_mutex_lock((&from_me->qlock));
    if(from_me->qhead==NULL)
    {
        from_me->qhead=toPut;
        from_me->qtail=toPut;
    } else {
        from_me->qtail->next=toPut;
        from_me->qtail=from_me->qtail->next;
    }
    from_me->qsize++;
    pthread_mutex_unlock((&from_me->qlock));

    pthread_cond_signal(&(from_me->q_not_empty));
}

void* do_work(void* p)
{
    work_t* curr;
    threadpool* toUse= (threadpool*)p;
    // perror("Entered do_work from a thread\n");//****
    while(!(toUse->shutdown))
    {
        if(!(toUse->qsize))
        {
            // perror("going to sleep\n");//****
            pthread_cond_wait(&(toUse->q_not_empty),&(toUse->qlock));
        }
        // perror("Woke up from sleep\n");//****
        if((toUse->shutdown)==1)
        {
            // perror("Exit from inside loop\n");//****
            pthread_mutex_unlock(&(toUse->qlock));
            pthread_cond_signal(&(toUse->q_not_empty));
            break;
        }
        // pthread_mutex_lock(&(toUse->qlock));
        // if(curr!=NULL)
        //     free(curr);
        curr=toUse->qhead;
        if(curr==NULL)
        {
            pthread_mutex_unlock(&(toUse->qlock));
            if(!(toUse->shutdown))
            {
                // perror("continue loop\n");//****
                continue;
            }
            else {
                pthread_mutex_unlock(&(toUse->qlock));
                pthread_cond_signal(&(toUse->q_not_empty));
                break;
            }
        }
        toUse->qhead=toUse->qhead->next;
        if(toUse->dont_accept && toUse->qhead==NULL)
        {
            // perror("Waking up destructor1\n");//****
            pthread_cond_signal(&(toUse->q_empty));
        }
        toUse->qsize--;
        pthread_mutex_unlock(&(toUse->qlock));
        // pthread_cond_signal()
        curr->routine(curr->arg);
        free(curr);

        // perror("restarting loop\n");//****
        if(toUse->shutdown)
            break;
    }
    // perror("exiting thread!\n");//**** 
    pthread_mutex_unlock(&(toUse->qlock));
    pthread_cond_signal(&(toUse->q_not_empty));
    return NULL;

}

/* MY FUNCTIONS */
void errorDestroy(char* msg,threadpool* threadp)
{
    perror(msg);
    if(threadp!=NULL)
    {
        destroy_threadpool(threadp);
    }
}

void* sleeper_wake_function(void* p)
{
    threadpool* tp = ((threadpool*)((sleeperwakestruct*)p)->tpool);
    // pthread_cond_t toSleepOn= tp->sleeper_wake;
    int* shutdownComplete = ((sleeperwakestruct*)p)->toStop;

    while(!(*shutdownComplete))
    {
        pthread_mutex_unlock(&(tp->qlock));
        pthread_cond_broadcast(&(tp->q_not_empty));
    }		
	

    return NULL;
}

// /*TEST  FUNCTIONS*/
// int f1(void* f)
// {
//     int* p = (int*)f;
//     perror("Number you printed is:\t%d\n",*p);
//     return 0;
// }

// int f2(void* f)
// {
//     int* p = (int*)f;
//     perror("Number you printed is:\t%d\n",(*p)*(*p));
//     return 0;
// }

// /*    MAIN      */
// int main()
// {

//     //create threadpool
//     threadpool *toCheck = create_threadpool(200);
//     //create argument for f1
//     int* intCheck = (int*)calloc(1,sizeof(int));
//     *intCheck=5;
    
//     //add to job pool
//     dispatch(toCheck,f1,intCheck);
//     // dispatch(toCheck,f2,intCheck);


//     //destroy
//     destroy_threadpool(toCheck);
//     free(intCheck);
//     return 0;
// }








