/*
Program: Phase 1c Semaphores.
*/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include "usloss.h"
#include "phase1Int.h"


void initProcTable();
void initSemTable();
int inKernel();
//int BSIZE = 10;


typedef struct wait_queue {
    int         pid;                          //the waitting ID of the process
    struct wait_queue* next;                 //next pointer
} wait_queue;

typedef struct Sem
{
    char        name[P1_MAXNAME+1];
    u_int       value;
    int         sid;        // semaphore on which process is blocked, if any
    //int         BSIZE; // buffer size//
    int         blockedList;
    // more fields here
    wait_queue* queue_head;
} Sem;

static Sem sems[P1_MAXSEM];




void 
P1SemInit(void) 
{
    P1ProcInit();
    for (int i = 0; i < P1_MAXSEM; i++) {
        sems[i].name[0] = '\0';
        sems[i].value = -1;
        sems[i].sid = -1;
        sems[i].blockedList = 0; 
        // initialize rest of sem here
        sems[i].queue_head = (wait_queue*)malloc(sizeof(wait_queue));
        sems[i].queue_head->pid = -1;
        sems[i].queue_head->next = NULL;
    }
    
}

int P1_SemCreate(char *name, unsigned int value, int *sid)
{
    int result = P1_SUCCESS;
    // check for kernel mode
        if (!inKernel())
    {
        USLOSS_Console("called while in user mode");
        USLOSS_IllegalInstruction();
    }
    // disable interrupts
    int disable_flag;
    disable_flag = P1DisableInterrupts();
    
    // check parameters
    for (int i = 0; i < P1_MAXSEM; i++) {
        if (strcmp(sems[i].name, name) == 0) {
            return result = P1_DUPLICATE_NAME;
        }
    }
    if (name == NULL) {
        return result = P1_NAME_IS_NULL;
    }
    if (strlen(name) > P1_MAXNAME) {
        return result = P1_NAME_TOO_LONG;
    }


    // find a free Sem and initialize it
    int freeslot = -1;
    for (int i = 0; i<P1_MAXSEM; i++){
        if (sems[i].sid == -1){
            freeslot = i;
            break;
        }
    } 
    if (freeslot == -1){
        USLOSS_Console ("no free semaphore is available");
        return result = P1_TOO_MANY_SEMS;
    }
    *sid=freeslot;

        strcpy(sems[freeslot].name, name);
        sems[freeslot].value = value;
        sems[freeslot].sid = *sid;

    // re-enable interrupts if they were previously enabled
    P1EnableInterrupts();
    return result;
}

int P1_SemFree(int sid) 
{
    int     result = P1_SUCCESS;
    // more code here
    
    if(sems[sid].blockedList > 0) {
        return result = P1_BLOCKED_PROCESSES;
    }
     if (sid < 0 || sid >= P1_MAXPROC) {
        return result = P1_INVALID_SID;
    }
      // wipe out semaphore 
    sems[sid].sid            = -1;
    sems[sid].value          = -1;
    sems[sid].blockedList    = 0;
    sems[sid].queue_head->pid = -1;
    sems[sid].queue_head->next = NULL;
    return result;
}

int P1_P(int sid) 
{
    int result = P1_SUCCESS;
    // check for kernel mode
    if (!inKernel())
    {
        USLOSS_Console("called while in user mode");
        USLOSS_IllegalInstruction();
    }

    // disable interrupts
    int disable_flag;
    disable_flag = P1DisableInterrupts();

    if (sems[sid].name[0] == '\0') {
        return result = P1_INVALID_SID;
    }
    
    if (sems[sid].value>0){
        sems[sid].value--;
      }
    // while value == 0
    //      set state to P1_STATE_BLOCKED
    else  {
        int current_p = P1_GetPid();
        int set_flag;
        set_flag= P1SetState(current_p, P1_STATE_BLOCKED, sid);
        //add this process into the waitting queue
        wait_queue* head = sems[sid].queue_head;
        sems[sid].blockedList++;
        if (head->pid==-1) {
            head->pid = current_p;
        }
        else {
            while (head->next!=NULL) {
                head = head->next;
            }
            wait_queue* queue_new = (wait_queue*)malloc(sizeof(wait_queue));
            queue_new->pid = current_p;
            queue_new->next = NULL;
            head->next = queue_new;
        }

    }
    P1Dispatch(0);
        
    // value--
      
    // re-enable interrupts if they were previously enabled
    P1EnableInterrupts();
    return result;
}

int P1_V(int sid) 
{
    int result = P1_SUCCESS;
    if (sems[sid].name[0] == '\0') {
        return result = P1_INVALID_SID;
    }

    // check for kernel mode
    if (!inKernel())
    {
        USLOSS_Console("called while in user mode");
        USLOSS_IllegalInstruction();
    }
    
    // disable interrupts
    int disable_flag;
    disable_flag = P1DisableInterrupts();

    
    sems[sid].value++;
    // value++
    
    // if a process is waiting for this semaphore
    //      set the process's state to P1_STATE_READY
    if (sems[sid].value == 1) {
        int set_flag;
        int ready_p = sems[sid].queue_head->pid;
        set_flag=P1SetState(ready_p, P1_STATE_READY, sid);
        sems[sid].blockedList--;
        if (sems[sid].queue_head->next!=NULL) {
            sems[sid].queue_head = sems[sid].queue_head->next;
            //FIFO get the ready ID of the process
        }
        else {
            sems[sid].queue_head->pid = -1;
            //the queue is only one element
        }
        P1Dispatch(0);
    }

    // re-enable interrupts if they were previously enabled
    P1EnableInterrupts();
    return result;
}

int P1_SemName(int sid, char *name) {
    int result = P1_SUCCESS;
    // more code here
    if (sems[sid].name[0] == '\0') {
        return result = P1_INVALID_SID;
    }

    if (name==NULL) {
        return result = P1_NAME_IS_NULL;
    }
    strcpy(name,sems[sid].name);
    return result;
}

int inKernel()
{
    // 0 if in kernel mode, !0 if in user mode
    return ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()));
}
