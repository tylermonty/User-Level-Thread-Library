//#include "t_lib.h"
#include "ud_thread.h"
#include "signal.h"

#define _XOPEN_SOURCE >= 500

tcb *running;
tcb *ready;

void t_init() //initialize thread library
{
  tcb *tmp;
  tmp = malloc(sizeof(tcb));
  tmp->thread_id = 0;
  tmp->thread_priority = 1;
  tmp->thread_context = (ucontext_t *) malloc(sizeof(ucontext_t));
  tmp->next = NULL;
  getcontext(tmp->thread_context);    /* let tmp be the context of main() */
  running = tmp;
}

void t_create(void (*fct)(int), int id, int pri) //create new thread
{
  size_t sz = 0x10000;

  tcb *new_tcb = malloc(sizeof(tcb));
  new_tcb->thread_context = (ucontext_t *) malloc(sizeof(ucontext_t));
  getcontext(new_tcb->thread_context);
/***
  uc->uc_stack.ss_sp = mmap(0, sz,
       PROT_READ | PROT_WRITE | PROT_EXEC,
       MAP_PRIVATE | MAP_ANON, -1, 0);
***/
  new_tcb->thread_context->uc_stack.ss_sp = malloc(sz);
  new_tcb->thread_context->uc_stack.ss_size = sz;
  new_tcb->thread_context->uc_stack.ss_flags = 0;
  makecontext(new_tcb->thread_context, (void (*)(void)) fct, 1, id);
  new_tcb->thread_id = id;
  new_tcb->thread_priority = pri;
  new_tcb->next = NULL;
  enqueue(&ready, new_tcb);
}

void t_yield() //thread relinquishes CPU, placed at end of ready queue
{
  if (ready){ //only yield if ready queue has at least one node
    tcb *new_running = dequeue(&ready); //get node with highest priority
    tcb *old_running = running;
    new_running->next = NULL;
    old_running->next = NULL;
    enqueue(&ready, old_running); //place thread back onto queue
    running = new_running;
    swapcontext(old_running->thread_context, new_running->thread_context);
  }
}

void t_shutdown(){ //shutdown thread library by freeing all allocated memory
  if (ready){
    tcb *cur = ready;
    while(cur){
	     tcb *tmp = cur->next;
       free(cur->thread_context);
       free(cur);
	     cur = tmp;
    }
  }
  free(ready);
  free(running->thread_context);
  free(running);
}

void t_terminate(){ //terminate calling thread
  tcb *tmp = running;
  free(tmp->thread_context->uc_stack.ss_sp);
  free(tmp->thread_context);
  free(tmp);
  running = dequeue(&ready);
  if (running){
    setcontext(running->thread_context);
  }
}

tcb * enqueue(tcb **queue, tcb *node){
  if (!*queue){ //if queue is empty, insert as first element
    *queue = node;
    node->next = NULL;
  }
  else{ //find where to insert node based on priority
    if ((*queue)->thread_priority > node->thread_priority){ //replace head of queue
      node->next = *queue;
      *queue = node;
    }else{
      tcb *cur = *queue;
      while (cur->next && cur->next->thread_priority <= node->thread_priority){
        cur = cur->next;
      }//now cur->next has lower priority than node or cur->next is null
      node->next = cur->next;
      cur->next = node;
	   }
  }
  return *queue;
}

tcb *dequeue(tcb **queue){ //dequeue first node in queue
  tcb *tmp = *queue;
  if (*queue){//if any value in queue
    *queue = (*queue)->next;//set head of queue to next node.
    tmp->next = NULL;//no value to return so we return a null tcb*
  }
  return tmp;
}

int sem_init(sem_t **sp, int sem_count){
	*sp = malloc(sizeof(sem_t));
	(*sp)->count = sem_count;
	(*sp)->q = NULL;
}

void sem_wait(sem_t *sp){
  sighold();
  if(sp->count > 0)
    sp->count -= 1;
  else{
    tcb * tmp = running;
    enqueue(&(sp->q), tmp);
    sigrelse();
    //t yield without enqueue
    if (ready){ //only yield if ready queue has at least one node
      tcb *new_running = dequeue(&ready); //get node with highest priority
      tcb *old_running = running;
      new_running->next = NULL;
      old_running->next = NULL;
      running = new_running;
      swapcontext(old_running->thread_context, new_running->thread_context);
    }
  }
  sigrelse();
}

void sem_signal(sem_t *sp){
  sighold();
  tcb* tmp = dequeue(&(sp->q));
  if(tmp){
    enqueue(&ready, tmp);
  }else{
    sp->count += 1;
  }
  sigrelse();
}

void sem_destroy(sem_t **sp){
  tcb* tmp;
  tcb* cur = (*sp)->q;
	while(cur){
    tmp = cur->next;
    //free(cur->thread_context->uc_stack.ss_sp);
    //free(cur->thread_context);
    //free(cur);
	enqueue(&ready, cur);
    cur = tmp;
	}
  free((*sp)->q);
  free(*sp);
}

/*
void printList(t_queue *queue){ //print thread library list
  printf("Thread currently runing: %d\n", running->thread_id);
  if (queue){ //check if queue is empty before printing
    tcb *cur = queue->head;
    while(cur){
      printf("%d\n", cur->thread_id);
      cur = cur->next;
    }
    printf("\n");
  }
  else{
    printf("queue is empty\n");
  }
}
*/
