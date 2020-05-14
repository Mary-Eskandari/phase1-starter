/*Phase 1b Process Management.
*/


#include "phase1Int.h"
#include "usloss.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

typedef struct PCB {
    int             cid;                // context's ID
    int             cpuTime;            // process's running time
    char            name[P1_MAXNAME + 1]; // process's name
    int             priority;           // process's priority
    P1_State        state;              // state of the PCB
    // more fields here
    int         tag;                    // process's tag
    int         parent;                 // parent PID
    int         children[P1_MAXPROC];   // childen PIDs
    int         numChildren;            // # of children
    int         sid;                    // semaphore on which process is blocked, if any
    int         numQuit;
    int         Quitchildren[P1_MAXPROC];
    int         status;
    int            (*startFunc)(void*);//func
    void* startArg;                     //func's arguement
} PCB;

typedef struct queue {
    int         pid;                    //the ID of the process
    struct queue* next;                 //next pointer
} queue;

static PCB processTable[P1_MAXPROC];   // the process table
static queue* ready_queue;             //head of the ready list
int total_count;                       //count process has been forked
int forked_pid;

void P1ProcInit(void)
{
    P1ContextInit();
    ready_queue = (queue*)malloc(sizeof(queue));
    ready_queue->pid = -1;
    ready_queue->next = NULL;
    for (int i = 0; i < P1_MAXPROC; i++) {
        processTable[i].state = P1_STATE_FREE;
        // initialize the rest of the PCB
        processTable[i].cid = -1;
        processTable[i].cpuTime = -1;
        processTable[i].priority = -1;
        //processTable[i].name = "";
        processTable[i].tag = -1;
        processTable[i].parent = -1;
        //processTable[i].children = NULL;
        processTable[i].numChildren = 0;
        processTable[i].numQuit = 0;
        //processTable[i].Quitchildren=NULL;
        processTable[i].status = -6;
        //build the linked list


    }
    // initialize everything else
    total_count = 0;
    forked_pid = 0;
}
void P1MoveTail(int Pid) {
    //This function is to move the current running process into the tail of the queue
    queue* temp1 = ready_queue;
    queue* temp2;
    queue* temp3;
    while (temp1->next != NULL) {
        if (temp1->pid == Pid) {
            temp2->next = temp1->next;
            temp3 = temp1;
        }
        temp2 = temp1;
        temp1 = temp1->next;
    }
    temp1 = temp3;
    temp1->next = NULL;
}

void P1AddtoQueue(int pid) {
    queue* temp = ready_queue;
    if (temp->pid==-1) {
        temp->pid = pid;
    }
    else{
        while (temp->next != NULL) {
            temp = temp->next;
        }
        queue* new_node = (queue*)malloc(sizeof(queue));
        new_node->next = NULL;
        new_node->pid = pid;
        temp->next = new_node;
    }
}

void P1RemoveQueue(int pid) {
    queue* temp1 = ready_queue;
    queue* temp2 = temp1;
    if (temp1->pid== pid) {
        temp1->pid = -1;
    }
    else {
        while (temp1->next != NULL) {
            if (temp1->pid == pid) {
             temp2->next = temp1->next;
             }
            temp2 = temp1;
            temp1 = temp1->next;
        }
        }
}

int inKernel()
{
    // 0 if in kernel mode, !0 if in user mode
    return ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()));
}

int P1_GetPid(void)
{
    int k;//k is to find the current running progress
    int find_flag = 0;
    for (k = 0; k < P1_MAXPROC; k++) {
        if (processTable[k].state == P1_STATE_RUNNING) {
            break;//loop to find the pid of current running progress
        }
    }
    if (processTable[k].state == P1_STATE_RUNNING) {
        return k;
        find_flag = 1;
    }
    if (find_flag != 1) {
        return -1;
    }
    return -1;
}

static void launch()
{
    int return_value = processTable[forked_pid].startFunc(processTable[forked_pid].startArg);
    P1_Quit(return_value);


}


int P1_Fork(char* name, int (*func)(void*), void* arg, int stacksize, int priority, int tag, int* pid)
{
    int result = P1_SUCCESS;
    int disable_flag;


    if (!inKernel())
    {
        USLOSS_Console("P1_fork(): called while in user mode");
        USLOSS_IllegalInstruction();
    }
    // disable interrupts
    disable_flag = P1DisableInterrupts();
    // check all parameters
    if (tag == 0 ||tag == 1) {
        
    }
    else {
        return result = P1_INVALID_TAG;
    }
    //USLOSS_Console("check tag success!!");

    if ((priority < 1 || priority >5) && total_count != 0) {
        return result = P1_INVALID_PRIORITY;
    }
    if ((priority < 1 || priority >6) && total_count == 0) {
        return result = P1_INVALID_PRIORITY;
    }
    //USLOSS_Console("priority check success!!");
    if (stacksize < USLOSS_MIN_STACK) {
        return result = P1_INVALID_STACK;
    }
    //USLOSS_Console("USLOSS_MIN_STACK check success!!");
    int i;
    for (i = 0; i < P1_MAXPROC; i++) {
        if (strcmp(processTable[i].name, name) == 0) {
            return result = P1_DUPLICATE_NAME;
        }
    }
    if (name == NULL) {
        return result = P1_NAME_IS_NULL;
    }
    if (strlen(name) > P1_MAXNAME) {
        return result = P1_NAME_TOO_LONG;
    }

    //USLOSS_Console("check success!!");
    int j;
    int flag = 0;
    for (j = 0; j < P1_MAXPROC; j++) {
        if (processTable[j].state == P1_STATE_FREE) {
            flag = 1;//find the first free progress to allocate
            *pid = j;
            break;
        }
    }
    //USLOSS_Console("find the pid:%d\n",*pid);
    if (flag == 0) {
        return result = P1_TOO_MANY_PROCESSES;//did find any room for new progress
    }

    // allocate and initialize PCB
    forked_pid = *pid;
    processTable[*pid].startFunc = func;
    processTable[*pid].startArg = arg;
    strcpy(processTable[*pid].name, name);
    processTable[*pid].priority = priority;
    processTable[*pid].state = P1_STATE_READY;
    processTable[*pid].tag = tag;
    // create a context using P1ContextCreate
    assert(P1ContextCreate(launch, arg, stacksize, &(processTable[*pid].cid)) == P1_SUCCESS);
    //USLOSS_Console("Creat successed!\n");


    //add the runnable process to the linklist

    P1AddtoQueue(*pid);
    //USLOSS_Console("Add successed!\n");
    // if this is the first process or this process's priority is higher than the 
    //    currently running process call P1Dispatch(FALSE)

    int current_run = P1_GetPid();
    if (current_run != -1) {
        processTable[current_run].children[processTable[current_run].numChildren] = *pid;
        processTable[*pid].parent = current_run;
        processTable[current_run].numChildren++;
    }
    if (current_run == -1 || (priority < processTable[current_run].priority && current_run != -1)) {
        //USLOSS_Console("to Dispatch!\n");
        P1Dispatch(FALSE);
    }
    total_count++;
    // re-enable interrupts if they were previously enabled
    P1EnableInterrupts();
    return result;
}

void
P1_Quit(int status)
{
    int disable_flag;
    if (!inKernel())
    {
        //USLOSS_Console("P1_fork(): called while in user mode");
        USLOSS_IllegalInstruction();
    }
    // disable interrupts
    disable_flag = P1DisableInterrupts();
    int current_running = P1_GetPid();
    //USLOSS_Console("Current running pid:%d", current_running);
    //first process quit with the children process
    if (current_running == 0 && processTable[current_running].numChildren != 0) {
        USLOSS_Console("First process quitting with children, halting.\n");
        USLOSS_Halt(1);
    }

    // remove from ready queue, set status to P1_STATE_QUIT
    //USLOSS_Console("before REMOVE.\n");
    P1RemoveQueue(current_running);
    //USLOSS_Console("AFTER REMOVE.\n");
    assert(P1SetState(current_running, P1_STATE_QUIT, 0) == P1_SUCCESS);
    // if first process verify it doesn't have children, otherwise give children to first process
    if (processTable[current_running].numChildren != 0) {
        int i;
        // adding all the the current_running children into the first process
        for (i = 0; i < processTable[current_running].numChildren; i++) {
            processTable[0].children[processTable[0].numChildren + i] = processTable[current_running].children[i];
        }
        processTable[0].numChildren += i;
        processTable[0].numChildren += 1;

    }

    // add ourself to list of our parent's children that have quit
    processTable[processTable[current_running].parent].Quitchildren[processTable[processTable[current_running].parent].numQuit] = current_running;
    processTable[processTable[current_running].parent].numQuit++;

    // if parent is in state P1_STATE_JOINING set its state to P1_STATE_READY
    if (processTable[processTable[current_running].parent].state == P1_STATE_JOINING) {
        assert(P1SetState(processTable[current_running].parent, P1_STATE_READY, 0) == P1_SUCCESS);
    }
    processTable[current_running].status = status;
    P1Dispatch(FALSE);
    // should never get here

    assert(0);
}


int
P1GetChildStatus(int tag, int* cpid, int* status)
{
    int result = P1_SUCCESS;
    int current = P1_GetPid();
    if (tag!=0&&tag!=1) {
        return result= P1_INVALID_TAG;
    }


    if (processTable[current].numQuit==0&& processTable[current].numChildren != 0) {
        return result = P1_NO_QUIT;
    }
    int i;
    int NO_flag = 1;

    for (i = 0; i < processTable[current].numQuit; i++) {
        if (processTable[processTable[current].Quitchildren[i]].tag==tag) {
            NO_flag = 0;
            break;
        }
    }
    if (processTable[current].numChildren == 0|| NO_flag) {
        return result = P1_NO_CHILDREN;
    }
    *cpid = processTable[current].Quitchildren[i];
    *status = processTable[*cpid].status;
    assert(P1ContextFree(processTable[*cpid].cid) == P1_SUCCESS);
    // do stuff here
    return result;
}

int
P1SetState(int pid, P1_State state, int sid)
{
    int result = P1_SUCCESS;
    if (pid < 0 || pid >= P1_MAXPROC) {
        return result = P1_INVALID_PID;
    }
    if (state != P1_STATE_READY && state != P1_STATE_JOINING && state != P1_STATE_BLOCKED && state != P1_STATE_QUIT) {
        return result = P1_INVALID_STATE;
    }
    if (state == P1_STATE_JOINING) {
        int i;
        for (i = 0; i < P1_MAXPROC; i++) {
            if (processTable[processTable[pid].children[i]].state == P1_STATE_QUIT)
                return result = P1_CHILD_QUIT;
        }
    }
    // do stuff here
    processTable[pid].state = state;

    return result;
}

void
P1Dispatch(int rotate)
{
    // select the highest-priority runnable process
    int        highest = 7;
    int        highest_id = -1;
    int        find_highest = 0;

    int i;
    for (i = 0; i < P1_MAXPROC; i++) {
        if (processTable[i].priority < highest && processTable[i].state == P1_STATE_READY) {
            highest_id = i;
            find_highest = 1;
        }
    }
    int current_running = P1_GetPid();
    //USLOSS_Console("into the dispatch!\n");
    //USLOSS_Console("the highest pid is%d\n", highest_id);
    //run the highest 
    if (current_running ==-1) {
        //USLOSS_Console("the first ContextSwitch!\n");
        processTable[0].state= P1_STATE_RUNNING;
        assert(P1ContextSwitch(processTable[0].cid) == P1_SUCCESS);
    }
    if (find_highest && highest_id != current_running && processTable[highest_id].priority > processTable[current_running].priority) {
        assert(P1SetState(current_running, P1_STATE_READY, 0) == P1_SUCCESS);
        processTable[highest_id].state = P1_STATE_RUNNING;
        //USLOSS_Console("higher priority ContextSwitch!\n");
        assert(P1ContextSwitch(processTable[highest_id].cid) == P1_SUCCESS);
    }if (highest_id == -1&& current_running==-1) {
        //USLOSS_Console("No runnable processes, halting.\n");
        USLOSS_Halt(0);
    }

    //if rotate, then find the first runnable process
    if (rotate == 1) {
        int find_rotate_runnable = 0;
        queue* temp = ready_queue;
        while (temp->next!=NULL) {
            if (processTable[temp->pid].priority == processTable[current_running].priority) {
                find_rotate_runnable = 1;
                break;
            }
            temp = temp->next;
        }
        if (find_rotate_runnable) {
            //find the same priority runnable process
            //move current running process to ready state, this runnable into running state
            P1MoveTail(current_running);
            assert(P1SetState(current_running, P1_STATE_READY, 0) == P1_SUCCESS);
            processTable[temp->pid].state= P1_STATE_RUNNING;
            //USLOSS_Console("ContextSwitch!\n");
            assert(P1ContextSwitch(temp->pid) == P1_SUCCESS);
        }
        else {
            USLOSS_Console("No runnable processes, halting.\n");
            USLOSS_Halt(0);
        }
    }


    // call P1ContextSwitch to switch to that process
    P1EnableInterrupts();


}

int
P1_GetProcInfo(int pid, P1_ProcInfo* info)
{
    int         result = P1_SUCCESS;
    // fill in info here
    if (processTable[pid].state == P1_STATE_FREE || pid<0 || pid>P1_MAXPROC) {
        return result = P1_INVALID_PID;
    }
    strcpy(info[pid].name, processTable[pid].name);
    info[pid].state = processTable[pid].state;
    info[pid].sid = processTable[pid].sid;
    info[pid].priority = processTable[pid].priority;
    info[pid].tag = processTable[pid].tag;
    info[pid].cpu = processTable[pid].cpuTime;
    info[pid].parent = processTable[pid].parent;
    info[pid].numChildren = processTable[pid].numChildren;
    int i;
    for (i = 0; i < info[pid].numChildren; i++) {
        info[pid].children[i] = processTable[pid].children[i];
    }

    return result;
}
