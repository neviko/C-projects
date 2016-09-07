
//**********************************************************************

							//nevo sayag 200484426

//************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "threadpool.h"
	


//*******************************************************************************************************

//*******************************************************************************************************

//										C R E A T E   T H R E A D P O O L

//*******************************************************************************************************

//*******************************************************************************************************



threadpool* create_threadpool(int num_threads_in_pool)
{
	int ret;
	int i;
	

	if (num_threads_in_pool < 1 || num_threads_in_pool > MAXT_IN_POOL)
	{
		fprintf(stderr, "num_threads_in_pool is not valide \n");
		return NULL;
	}

	threadpool* tp= (threadpool*)malloc(sizeof(threadpool));
	if (tp==NULL)
	{
		perror("allocating problem in create_threadpool \n");
		free(tp);
		return NULL;
	}

	pthread_t* threadsArr =(pthread_t*)malloc(num_threads_in_pool*sizeof(pthread_t)) ;// threads array
	if(threadsArr ==NULL)
	{
		perror("allocating problem in create_threadpool\n ");
		free(tp);
		free(threadsArr);
		return NULL;
	}

	// init the threadpool
	tp->num_threads=num_threads_in_pool;
	tp->qsize=0;
	tp->threads=threadsArr;
	tp->qhead=NULL;
	tp->qtail=NULL;
	tp->shutdown=0;
	tp->dont_accept=0;


	// init mutex and pthread_cond_elements
	ret=pthread_mutex_init(&tp->qlock, NULL);
	if(ret!= 0) // innitilize mutex
	{
		fprintf(stderr, "init problem in create_threadpool  \n");
		destroy_threadpool(tp);
		return NULL;
	}
	
	ret =pthread_cond_init(&(tp->q_empty), NULL);
	if(ret !=0) 
	{
		fprintf(stderr, "init problem in create_threadpool \n");
		destroy_threadpool(tp);
		return NULL;
    }


    ret=pthread_cond_init(&(tp->q_not_empty), NULL);
    if(ret!=0) 
    {
    	fprintf(stderr, "init problem in create_threadpool\n ");
		destroy_threadpool(tp);
		return NULL;
    }
	
	for (i = 0; i < tp->num_threads; i++)
	{	
		ret= pthread_create (&(tp->threads[i]),NULL,&do_work,(void*)tp) ; /* code */
		if(ret!=0)	
		{
			fprintf(stderr, "problem while creating threads\n");
			return NULL;
		}
		
	}

	return tp;
}


//*******************************************************************************************************

//*******************************************************************************************************

//												D O    W O R K

//*******************************************************************************************************

//*******************************************************************************************************


void* do_work(void* p)
{
	int ret;
	threadpool* tp=(threadpool*)p;

	if(tp==NULL || tp->threads==NULL)	
		return NULL;

	work_t* work;
	while(1)
	{	
		ret=pthread_mutex_lock(&tp->qlock);	// lock the area
		if(ret!=0)
		{
			fprintf(stderr, "problem with pthread_mutex_lock \n");
			return NULL;
		}

		if(tp->shutdown==1)//if destroy has begun 
		{	
			ret=pthread_mutex_unlock(&tp->qlock);	
			{
				if(ret!=0)
				{
					fprintf(stderr, "problem with pthread_mutex_unlock \n");
					return NULL;
				}
			}
			return NULL;
		}

		while(tp->qsize==0)
		{			
			ret=pthread_cond_wait(&tp->q_empty,&tp->qlock);// if work queue is empty "sleep" until wake you up and unlock the mutex
			if(ret!=0)
			{
				fprintf(stderr, "problem with pthread_cond_wait \n");
				return NULL;
			}


			if(tp->shutdown==1)
			{	
				ret=pthread_mutex_unlock(&tp->qlock);
				if(ret!=0)
				{
					fprintf(stderr, "problem with pthread_mutex_lock \n");
				}
				return NULL;
			}
		}


		//take a work from the queue
		work=tp->qhead;
		tp->qsize--;

		if(tp->qsize==0)// if the queue is empty point to null
		{
			tp->qhead=NULL;
			tp->qtail=NULL;

			if(tp->dont_accept==1)	//wake up the wait_cond from destroy if destroy has begun and the queue is 0
			{

				ret=pthread_cond_signal(&tp->q_not_empty);
				if(ret!=0)
				{
					fprintf(stderr, "problem with pthread_cond_signal \n");
				}
			}
		}

		else
			tp->qhead=tp->qhead->next;// set the new head

		//init the work struct fiels  
		work->routine(work->arg);	
		free(work);

		ret=pthread_mutex_unlock(&tp->qlock);	//unlock 
		if(ret!=0)
		{
			fprintf(stderr, "problem with pthread_mutex_lock \n");
		}
	}	

	return NULL;
}


//*******************************************************************************************************

//*******************************************************************************************************

//											D I S P A T C H 

//*******************************************************************************************************

//*******************************************************************************************************


void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)//rec 2 -5:00
{
	int ret;

	threadpool* tp= from_me;

	if(tp == NULL || tp->threads == NULL || dispatch_to_here == NULL)
	{
		fprintf(stderr, "one or more of the arguments in dispatch is null\n");
		return;
	}


	ret=pthread_mutex_lock(&tp->qlock);	//lock area
	if(ret!=0)
	{
		fprintf(stderr, "problem with pthread_mutex_lock \n");
		return ;
	}

	if(tp->dont_accept==1)// if we cant have more works
	{	
		ret=pthread_mutex_unlock(&tp->qlock);
		if(ret!=0)
		{
			fprintf(stderr, "problem with pthread_mutex_unlock\n ");
			return ;
		}

		return;
	}


// allocate and add a new work into the works queue 	
	work_t* new_work=(work_t*)malloc(sizeof(work_t));
	if(new_work==NULL)
	{
		perror("problem while creating new_work \n");
		ret=pthread_mutex_unlock(&tp->qlock);
		if(ret!=0)
		{
			fprintf(stderr, "problem with pthread_mutex_unlock \n");
			return;
		}
		return;
	}

	new_work->routine=dispatch_to_here;
	new_work->arg=arg;
	new_work->next=NULL;


	if(tp->qsize==0)
	{	
		tp->qhead=new_work;
		tp->qtail=new_work;
	}

	else
	{	
		tp->qtail->next=new_work;
		tp->qtail=new_work;
	}

	tp->qsize++;//inc the qsize

	ret=pthread_cond_signal(&tp->q_empty);	//open the wait "gate" in do_work
	if(ret!=0)
	{
		fprintf(stderr, "problem with pthread_cond_signal \n");
		return ;
	}

	ret=pthread_mutex_unlock(&tp->qlock);	
	if(ret!=0)
	{
		fprintf(stderr, "problem with pthread_mutex_unlock \n");
		return ;
	}
}


//*******************************************************************************************************

//*******************************************************************************************************

//								D E S T R O Y    T H R E A D _ P O O L

//*******************************************************************************************************

//*******************************************************************************************************


void destroy_threadpool(threadpool* destroyme)
{
	int i;
	int ret;
	threadpool* tp=	destroyme;

	if(tp==NULL)
	{
		fprintf(stderr, " argument in destroy is null\n");
		return;
	}

	ret=pthread_mutex_lock(&tp->qlock);	//lock area
	if(ret!=0)
	{
		fprintf(stderr, "problem with pthread_mutex_lock \n");
		return ;
	}


	tp->dont_accept=1;	//flag to stop the dispach work
	
	if(tp->qsize!=0)
	{	
		ret=pthread_cond_wait(&tp->q_not_empty,&tp->qlock); // wait until all threads will finish them jobs
		if(ret!=0)
		{
			fprintf(stderr, "problem with pthread_cond_wait \n");
			return ;
		}
	}
	tp->shutdown=1;	// shutdown flag on, destroy has begun
	
	ret=pthread_mutex_unlock(&tp->qlock);	//unlock
	if(ret!=0)
	{
		fprintf(stderr, "problem with pthread_mutex_unlock\n ");
		return ;
	}

	ret=pthread_cond_broadcast(&tp->q_empty);	//signal all "sleeping" threads
	if(ret!=0)
	{
		fprintf(stderr, "problem with pthread_cond_broadcast \n");
		return ;
	}
	

	// wait until all threads will get here
	for(i=0;i<tp->num_threads;i++)
	{
		ret=pthread_join(tp->threads[i],NULL);
		if(ret !=0)
		{
			fprintf(stderr, "problem in destroy at pthread_join\n");
			ret=pthread_mutex_unlock(&tp->qlock);
			if(ret!=0)
			{
				fprintf(stderr, "problem with pthread_mutex_unlock \n");
				return ;
			}
			return;
		}
	}


	if(pthread_cond_destroy(&tp->q_empty)!=0)
	{
		fprintf(stderr, "problem with pthread_cond_destroy \n");
	}

  	if(pthread_cond_destroy(&tp->q_not_empty) !=0)
  	{
  		fprintf(stderr, "problem with pthread_cond_destroy \n");
  	}


    if(pthread_mutex_destroy(&tp->qlock)!=0)
    {
  		fprintf(stderr, "problem with pthread_cond_destroy \n");
    }


    //free all memory`
	free(tp->threads);	
	tp->threads=NULL;
	free(tp);	

}

//*******************************************************************************************************

//*******************************************************************************************************

//								E N D   O F   P R O G R A M 

//*******************************************************************************************************

//*******************************************************************************************************