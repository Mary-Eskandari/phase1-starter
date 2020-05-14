/*
Phase 1d
Program: Phase 1d divice interrupt.
*/
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "usloss.h"
#include "phase1.h"
#include "phase1Int.h"



#define USLOSS_CLOCK_INT    0   /* clock */
#define USLOSS_ALARM_INT    1   /* alarm */
#define USLOSS_DISK_INT     2   /* disk */
#define USLOSS_TERM_INT     3   /* terminal */ 
#define USLOSS_MMU_INT      4   /* MMU */
#define USLOSS_SYSCALL_INT  5   /* syscall */
#define USLOSS_ILLEGAL_INT  6   /* illegal instruction */

#define USLOSS_CLOCK_DEV    USLOSS_CLOCK_INT    
#define USLOSS_ALARM_DEV    USLOSS_ALARM_INT
#define USLOSS_DISK_DEV     USLOSS_DISK_INT
#define USLOSS_TERM_DEV     USLOSS_TERM_INT

#define USLOSS_CLOCK_UNITS  1
#define USLOSS_ALARM_UNITS  1
#define USLOSS_DISK_UNITS   2
#define USLOSS_TERM_UNITS   4

#define TAG_KERNEL 0

typedef void *P1_Semaphore;
int clockStatus, alarmNum;
USLOSS_DeviceRequest diskRequest[2];
int terminalStatus[4];
int clockTick;

int clock_sid, alarm_sid;
int terminals_sid[4];
int disks_sid[2];

static int ABRT[4][USLOSS_MAX_UNITS];


static void DeviceHandler(int type, void *arg);
static void SyscallHandler(int type, void *arg);
static void IllegalInstructionHandler(int type, void *arg);

static int sentinel(void *arg);


int inKernel()
{
    // 0 if in kernel mode, !0 if in user mode
    return ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()));
}

void 
startup(int argc, char **argv)
{
    int pid;
    P1SemInit();
    P1ProcInit();

    char* clock_name = "clock";
    char* alarm_name = "alarm";
    char* disks_name1 = "disk1";
    char* disks_name2 = "disk2";
    char* terminals_name1 = "terminals1";
    char* terminals_name2 = "terminals2";
    char* terminals_name3 = "terminals3";
    char* terminals_name4 = "terminals4";
    // initialize device data structures
    clockTick = 0;
    USLOSS_IntVec[USLOSS_CLOCK_INT]     = DeviceHandler;
    USLOSS_IntVec[USLOSS_CLOCK_DEV]     = DeviceHandler; 
    
    USLOSS_IntVec[USLOSS_ALARM_INT]     = DeviceHandler; 
    USLOSS_IntVec[USLOSS_ALARM_DEV]     = DeviceHandler; 
    
    USLOSS_IntVec[USLOSS_TERM_INT]      = DeviceHandler; 
    USLOSS_IntVec[USLOSS_TERM_DEV]      = DeviceHandler; 
    
    USLOSS_IntVec[USLOSS_SYSCALL_INT]   = SyscallHandler; 
    
    USLOSS_IntVec[USLOSS_DISK_INT]      = DeviceHandler; 
    USLOSS_IntVec[USLOSS_DISK_DEV]      = DeviceHandler; 
    USLOSS_IntVec[USLOSS_ILLEGAL_INT] = IllegalInstructionHandler;
    
    assert(P1_SemCreate(clock_name, 0, &clock_sid)== P1_SUCCESS);
    assert(P1_SemCreate(alarm_name, 0, &alarm_sid) == P1_SUCCESS);
    assert(P1_SemCreate(disks_name1, 0, &disks_sid[0]) == P1_SUCCESS);
    assert(P1_SemCreate(disks_name2, 0, &disks_sid[1]) == P1_SUCCESS);
    assert(P1_SemCreate(terminals_name1, 0, &terminals_sid[0]) == P1_SUCCESS);
    assert(P1_SemCreate(terminals_name2, 0, &terminals_sid[1]) == P1_SUCCESS);
    assert(P1_SemCreate(terminals_name3, 0, &terminals_sid[2]) == P1_SUCCESS);
    assert(P1_SemCreate(terminals_name4, 0, &terminals_sid[3]) == P1_SUCCESS);

    //totalSemaphore = 0;
    // put device interrupt handlers into interrupt vector
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = SyscallHandler;

    /* create the sentinel process */
    int rc = P1_Fork("sentinel", sentinel, NULL, USLOSS_MIN_STACK, 6 , 0, &pid);
    assert(rc == P1_SUCCESS);

    P1Dispatch(0);

    // should not return
    assert(0);
    return;

} /* End of startup */

int 
P1_WaitDevice(int type, int unit, int *status) 
{
    int     result = P1_SUCCESS;

    if (ABRT[type][unit] == TRUE){
        return result = P1_WAIT_ABORTED;
    }
    // disable interrupts
    int disable_flag;
    disable_flag = P1DisableInterrupts();
    assert(disable_flag== TRUE || disable_flag == FALSE);
    // check kernel mode
    if (!inKernel())
    {
        USLOSS_Console("called while in user mode");
        USLOSS_IllegalInstruction();
    }
    // P device's semaphore
    switch(type)
    {
        case USLOSS_CLOCK_DEV:
            if(unit == 0)
            {
                assert(P1_P(clock_sid) == P1_SUCCESS);
                *status = clockStatus;
                //USLOSS_DeviceInput(USLOSS_CLOCK_DEV, unit, *status);
            }
            else
            {
                return result = P1_INVALID_UNIT;
            }
            break;
            
        case USLOSS_ALARM_DEV:
            if(unit == 0)
            {
                assert(P1_P(alarm_sid) == P1_SUCCESS);
                *status = alarmNum;
                //USLOSS_DeviceInput(USLOSS_ALARM_DEV, unit, *status);
            }
            else
            {
                return result = P1_INVALID_UNIT;
            }
            break;
            
        case USLOSS_TERM_DEV:
            if(unit >= 0 && unit <= 3)
            {
                assert(P1_P(terminals_sid[unit]) == P1_SUCCESS);
                *status = terminalStatus[unit];
                //USLOSS_DeviceInput(USLOSS_TERM_DEV, unit, *status);
            }
            else
            {
                return result = P1_INVALID_UNIT;
            }
            break;
            
        case USLOSS_DISK_DEV:
            if(unit >= 0 && unit <= 1)
            {
                assert(P1_P(disks_sid[unit]) == P1_SUCCESS);
                *status = diskRequest[unit].opr;
                //USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, *status);
            }
            else
            {
                return result = P1_INVALID_UNIT;
            }
            break;
        default:
            USLOSS_Console("*** @P1_WaitDevice(): Invalid type\n");
            return result = P1_INVALID_TYPE;
            break;
    }

    // set *status to device's status
    int rcc;
    rcc = USLOSS_DeviceInput(type, unit, status);
    assert(rcc == USLOSS_DEV_OK);

    // restore interrupts
    P1EnableInterrupts();
    return result;
}

int 
P1_WakeupDevice(int type, int unit, int status, int abort) 
{
    int     result = P1_SUCCESS;
    
    // disable interrupts
    int disable_flag;
    disable_flag = P1DisableInterrupts();
    assert(disable_flag== TRUE|| disable_flag==FALSE);
    // check kernel mode
     if (!inKernel())
    {
        USLOSS_Console("called while in user mode");
        USLOSS_IllegalInstruction();
    }
    // save device's status to be used by P1_WaitDevice

    // save abort to be used by P1_WaitDevice
     ABRT[type][unit] = abort;
    // V device's semaphore 
    switch(type)
    {
        case USLOSS_CLOCK_DEV:
            if(unit == 0)
            {
                clockStatus = status;
                assert(P1_V(clock_sid)== P1_SUCCESS);
 
            }
            else
            {
                return result = P1_INVALID_UNIT;
            }
            break;
            
        case USLOSS_ALARM_DEV:
            if(unit == 0)
            {

                alarmNum= status;
                assert(P1_V(alarm_sid) == P1_SUCCESS);
                
            }
            else
            {
                return result = P1_INVALID_UNIT;
            }
            break;
            
        case USLOSS_TERM_DEV:
            if(unit >= 0 && unit <= 3)
            {
                terminalStatus[unit] = status;
                assert(P1_V(terminals_sid[unit]) == P1_SUCCESS);
                
            }
            else
            {
                return result = P1_INVALID_UNIT;
            }
            break;
            
        case USLOSS_DISK_DEV:
            if(unit >= 0 && unit <= 1)
            {
                diskRequest[unit].opr = status;
                assert(P1_V(disks_sid[unit]) == P1_SUCCESS);
                
            }
            else
            {
                return result = P1_INVALID_UNIT;
            }
            break;
        default:
            USLOSS_Console("*** @P1_WakeupDevice(): Invalid type\n");
            return result = P1_INVALID_TYPE;
            break;
    }
    // restore interrupts
    P1EnableInterrupts();
    return result;
}

static void
DeviceHandler(int type, void *arg) 
{
    int unit = (int) arg;
    // if clock device
    if (type == USLOSS_CLOCK_DEV){
        clockTick++;
        int timeUnits = 100 / USLOSS_CLOCK_MS;
        //USLOSS_Console("%d", timeUnits);
        if(clockTick != 0 && clockTick % timeUnits == 0){
            //if (unit != USLOSS_CLOCK_UNITS - 1){
              //  return P1_INVALID_UNIT;
            //}
            assert(USLOSS_DeviceInput(USLOSS_CLOCK_DEV, unit, &clockStatus)== USLOSS_DEV_OK);
            assert(P1_WakeupDevice(type,unit,clockStatus,ABRT[type][unit]) == P1_SUCCESS);
        }
        if(clockTick != 0 && clockTick % 4 == 0){
            P1Dispatch(1);
        }
        
    //      P1_WakeupDevice every 5 ticks
    //      P1Dispatch(TRUE) every 4 ticks
    } else{
        // else
        // P1_WakeupDevice

        switch (type)
        {
        case USLOSS_ALARM_DEV:
            if (unit == 0)
            {


                assert(USLOSS_DeviceInput(USLOSS_ALARM_DEV, unit, &alarmNum) == USLOSS_DEV_OK);
                assert(P1_WakeupDevice(type, unit, alarmNum, ABRT[type][unit]) == P1_SUCCESS);

            }
            break;

        case USLOSS_TERM_DEV:
            if (unit >= 0 && unit <= 3)
            {
                assert(USLOSS_DeviceInput(USLOSS_TERM_DEV, unit, &terminalStatus[unit]) == USLOSS_DEV_OK);
                assert(P1_WakeupDevice(type, unit, terminalStatus[unit], ABRT[type][unit]) == P1_SUCCESS);

            }
            break;

        case USLOSS_DISK_DEV:
            if (unit >= 0 && unit <= 1)
            {
                assert(USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, &diskRequest[unit].opr) == USLOSS_DEV_OK);
                assert(P1_WakeupDevice(type, unit, diskRequest[unit].opr, ABRT[type][unit]) == P1_SUCCESS);

            }
            break;
        default:
            USLOSS_Console("*** @P1_WakeupDevice(): Invalid type\n");
            break;
        }
    } 
}   

static int
sentinel(void* notused)
{
    int     pid,child_pid;
    int     rc;
    int     status, rv1, rv2;
    /* start the P2_Startup process */
    rc = P1_Fork("P2_Startup", P2_Startup, NULL, 4 * USLOSS_MIN_STACK, 2, 0, &pid);
    assert(rc == P1_SUCCESS);

    // enable interrupts
    P1EnableInterrupts();
    // while sentinel has children
    //      get children that have quit via P1GetChildStatus (either tag)
    //      wait for an interrupt via USLOSS_WaitInt
    do{

        rv1 = P1GetChildStatus(0, &child_pid, &status);
        rv2 = P1GetChildStatus(1, &child_pid, &status);

        if (rv1 == P1_NO_CHILDREN && rv2 == P1_NO_CHILDREN) {
            //both tag0,1 has no quit children
            break;
        }
        USLOSS_WaitInt();
    } while (1);

    USLOSS_Console("Sentinel quitting.\n");
    return 0;
} /* End of sentinel */

int
P1_Join(int tag, int* pid, int* status)
{
    int result = P1_SUCCESS;
    int rv;
    int parent_pid = P1_GetPid();
    int p;
    // disable interrupts
    p= P1DisableInterrupts();
    assert(p == 0 || p == 1);
    // kernel mode

    // check the kernel mode or set
    if (!inKernel())
    {
        USLOSS_Console("called while in user mode");
        USLOSS_IllegalInstruction();
    }
    /* USLOSS_PsrSet(USLOSS_PsrGet()||1);
    */
    if (tag!=0) {
        if (tag != 1) {
            return result = P1_INVALID_TAG;
        }
    }


    // do
    //     use P1GetChildStatus to get a child that has quit  
    //     if no children have quit
    //        set state to P1_STATE_JOINING vi P1SetState
    //        P1Dispatch(FALSE)
    // until either a child quit or there are no more children
    do{
        rv = P1GetChildStatus(tag, pid, status);
        if (rv == P1_NO_QUIT) {
            result = P1SetState(parent_pid, P1_STATE_JOINING, 0);
            P1Dispatch(FALSE);
        }
        if (rv== P1_NO_CHILDREN ||rv== P1_SUCCESS) {
            break;
        }
    } while (1);


	 return rv;
}


static void
SyscallHandler(int type, void *arg) 
{
    USLOSS_Console("System call %d not implemented.\n", (int) arg);
    USLOSS_IllegalInstruction();
}

void finish(int argc, char **argv) {}




static void IllegalInstructionHandler(int type, void *arg){
    P1_ProcInfo info;
    assert(type == USLOSS_ILLEGAL_INT);

    int pid = P1_GetPid();
    int rc = P1_GetProcInfo(pid, &info);
    assert(rc == P1_SUCCESS);
    if (info.tag == TAG_KERNEL) {
        P1_Quit(1024);
    }
}
