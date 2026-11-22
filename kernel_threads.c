
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"

// need a wrapper here, like start_main_thread()
/*
void start_main_thread()
{
  int exitval;

  Task call =  CURPROC->main_task;
  int argl = CURPROC->argl;
  void* args = CURPROC->args;

  exitval = call(argl,args);
  Exit(exitval);
}
  Maybe curproc here too?? Nope, I need the curthread!
*/
void start_thread() {
  int exitval;
  TCB* cur = cur_thread();
  Task call = cur->ptcb->task;
  int argl = cur->ptcb->argl;
  void *args = cur->ptcb->args;

  exitval = call(argl, args);
  ThreadExit(exitval);

}

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{

  if (task == NULL) {
    return NOTHREAD;
  }
  PCB* curproc = CURPROC;
  TCB* tcb = spawn_thread(curproc,start_thread);
  PTCB* ptcb = acquire_PTCB(tcb,task, argl, args);

  rlist_push_back(&curproc->ptcb_list, &ptcb->ptcb_node);
	curproc->thread_count++;
  wakeup(tcb);
  return (Tid_t)ptcb;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	PCB* curproc = CURPROC;
  rlnode* ptcb_node = rlist_find(&curproc->ptcb_list, (PTCB*)tid, NULL);

  if (ptcb_node == NULL) {
    return -1;
  }

  PTCB* ptcb = ptcb_node->ptcb;
  
  // fetch curthread??

  if (ptcb == cur_thread()->ptcb || ptcb->detached == 1) {
    return -1;
  }

  // Check this, noot sure
  ptcb->refcount++;
  while (ptcb->exited == 0 && ptcb->detached == 0) {
    kernel_wait(&ptcb->exit_cv, SCHED_USER);
  }
  ptcb->refcount--;

  if (ptcb->detached == 1)
    return -1;

  /* Success! Copy the thread's exit value if requested */
  if (exitval != NULL){
    
    *exitval = ptcb->exitval;
  }
  
  /* If no other joiner is still waking up, release the PTCB */
  if (ptcb->refcount == 0)
    release_PTCB(ptcb);
  return 0;


}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	PCB* curproc = CURPROC;
  rlnode* ptcb_node = rlist_find(&curproc->ptcb_list, (PTCB*)tid, NULL);

  if (ptcb_node == NULL || ptcb_node->ptcb->exited == 1) {
    return -1;
  }

  PTCB* ptcb = ptcb_node->ptcb;
  ptcb->detached = 1;

  // IMPORTANT! Wake up things(threads) sleeping in ThreadJooin
  kernel_broadcast(&ptcb->exit_cv);

  return 0;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

  PCB* curproc = CURPROC;
  PTCB* ptcb = cur_thread()->ptcb;

  ptcb->exitval = exitval;
  ptcb->exited = 1;
  ptcb->tcb = NULL;
  kernel_broadcast(&ptcb->exit_cv);
  curproc->thread_count--;

  if (curproc->thread_count == 0) {
    cleanup_process(curproc);
  }
  kernel_sleep(EXITED, SCHED_USER);
}

