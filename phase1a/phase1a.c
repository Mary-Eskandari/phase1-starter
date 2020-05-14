#include "phase1Int.h"
#include "usloss.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>


#define NO_PROCESS  0
#define READY       1
#define RUNNING     2
#define FINISHED    3
#define BLOCKED     4


extern  USLOSS_PTE  *P3_AllocatePageTable(int cid);
extern  void        P3_FreePageTable(int cid);

typedef struct Context {
    void            (*startFunc)(void *);
    void            *startArg;
    USLOSS_Context  context;
    int                 PID;                    /* Process Identifier */
    char                *stack;                 /* Process Stack */
    int                 status;                 /* Current Status of Process */
    int                 prev_context;		/*stores previous running context*/
} Context;

static Context   contexts[P1_MAXPROC];

static int currentCid = -1;




/*
 * Helper function to call func passed to P1ContextCreate with its arg.
 */
static void launch(void)
{
    USLOSS_Console("cid=%d launch is started\n",currentCid);
    contexts[currentCid].status = RUNNING;
    assert(contexts[currentCid].startFunc != NULL);
    contexts[currentCid].startFunc(contexts[currentCid].startArg);
    contexts[currentCid].status = FINISHED;

    
}

void P1ContextInit(void)
{
    // initialize contexts
    for (int i = 0; i < P1_MAXPROC; i++) {
        contexts[i].status = NO_PROCESS;
        contexts[i].PID = i;
         
	}
 }

int P1ContextCreate(void (*func)(void *), void *arg, int stacksize, int *cid) {
    int result = P1_SUCCESS;
    // find a free context and initialize it
    int i;
    for(i = 0; i < P1_MAXPROC; i++) {
        if(contexts[i].status == NO_PROCESS) {
            cid = &i;
            break;

         }
         else{
         result = P1_TOO_MANY_CONTEXTS;
          }
     }
        contexts[*cid].startFunc = func;
        contexts[*cid].startArg = arg;
        contexts[*cid].status = READY;
	// allocate the stack, specify the startFunc, etc.
        if (stacksize<USLOSS_MIN_STACK){
        result=P1_INVALID_STACK;
        }
        contexts[*cid].stack=malloc(stacksize);
        contexts[*cid].prev_context=currentCid;
        currentCid=*cid;
        USLOSS_ContextInit(&(contexts[*cid].context), contexts[*cid].stack, stacksize, P3_AllocatePageTable(*cid), launch);

    return result;
}

int P1ContextSwitch(int cid) {
    // switch to the specified context
    int result = P1_SUCCESS;
    if (cid>P1_MAXPROC || cid<0){
     result=P1_INVALID_CID;
     }
     if (cid==contexts[cid].prev_context){
     result=P1_INVALID_CID;
     }
    USLOSS_ContextSwitch(&contexts[contexts[cid].prev_context].context, &contexts[cid].context);
    return result;
}

int P1ContextFree(int cid) {
    // free the stack and mark the context as unused
    int result = P1_SUCCESS;
    if (cid>P1_MAXPROC || cid<0){
     result=P1_INVALID_CID;
     }
     if (contexts[cid].status==RUNNING){
     result=P1_CONTEXT_IN_USE;
     }
    P3_FreePageTable(cid);
    free(contexts[cid].stack);
    contexts[cid].status = NO_PROCESS;
     
    return result;
}


void 
P1EnableInterrupts(void) 
{    
    // set the interrupt bit in the PSR
       int enable=FALSE;
       if (USLOSS_PsrGet()==3){
       enable=TRUE;
	}
	else{
	enable=USLOSS_PsrSet( USLOSS_PsrGet() + USLOSS_PSR_CURRENT_INT );
	}
}

/*
 * Returns true if interrupts were enabled, false otherwise.
 */
int 
P1DisableInterrupts(void) 
{
    // set enabled to TRUE if interrupts are already enabled
    // clear the interrupt bit in the PSR
    int enabled = FALSE;
    if (USLOSS_PsrGet()==3){
     enabled=TRUE;
     int dis;
      dis=USLOSS_PsrSet( USLOSS_PsrGet() - USLOSS_PSR_CURRENT_INT );
     }
    return enabled;
}
