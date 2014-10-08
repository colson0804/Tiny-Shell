/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/


#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*));
#define RUNNING = 1;
#define DONE = 2;
#define STOPPED = 3;

typedef struct job_l {
  pid_t pid;
  int id;
  int isBG;
  int status;
  char *cmdline;
  struct job_l* next;
} jobL;

/* the pids of the background processes */
jobL *jobs = NULL;
int jobCounter = 1;


/************Function Prototypes******************************************/
/* run command */
static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void Exec(commandT*, bool);
/* runs a builtin command */
static void RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool IsBuiltIn(char*);
/************External Declaration*****************************************/

/**************Implementation***********************************************/
int total_task;
void RunCmd(commandT** cmd, int n)
{
  int i;
  total_task = n;

  if(n == 1)
    RunCmdFork(cmd[0], TRUE);
  else{
    RunCmdPipe(cmd[0], cmd[1]);
    for(i = 0; i < n; i++)
      ReleaseCmdT(&cmd[i]);
  }
}

void RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc<=0)
    return;

  if (IsBuiltIn(cmd->argv[0]))
  {
    RunBuiltInCmd(cmd);
  }
  else
  {
    RunExternalCmd(cmd, fork);
  }
}

void RunCmdBg(commandT* cmd)
{
  //todo
}

void RunCmdPipe(commandT* cmd1, commandT* cmd2)
{
}

void RunCmdRedirOut(commandT* cmd, char* file)
{
}

void RunCmdRedirIn(commandT* cmd, char* file)
{
}


/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  if (ResolveExternalCmd(cmd)){
    Exec(cmd, fork);
    if(cmd->bg != 1) {
      cmd->jobNumber = jobCounter;
      jobCounter++;
    }
  }
  else {
    printf("%s: command not found\n", cmd->argv[0]);
    ReleaseCmdT(&cmd);
  }
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd)
{
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;

  if(strchr(cmd->argv[0],'/') != NULL){
    if(stat(cmd->argv[0], &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
        }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)){
    c = strchr(&(pathlist[i]),':');
    if(c != NULL){
      for(j = 0; c != &(pathlist[i]); i++, j++)
        buf[j] = pathlist[i];
      i++;
    }
    else{
      for(j = 0; i < strlen(pathlist); i++, j++)
        buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    strcat(buf, "/");
    strcat(buf,cmd->argv[0]);
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(buf);
          return TRUE;
        }
    }
  }
  return FALSE; /*The command is not found or the user doesn't have enough priority to run.*/
}

jobL* addtolist(pid_t pid, commandT* cmd){
  jobL *jobList = jobs;
  jobL *newJobNode = (jobL *)malloc(sizeof(jobL));

  newJobNode->next = NULL;
  int newid = 0;

  if (jobList == NULL){
    newJobNode->id = 1;
    jobs = newJobNode;
  }
  else{
    newid = jobList->id;
    while(jobList->next !=NULL){
      if(newid < jobList->id)
        newid = jobList->id;
      jobList = jobList->next;
    }
    jobList->next = newJobNode;
  }

  newJobNode->id = newid + 1;
  if (cmd->bg){
    newJobNode->isBG = 1;
  }
  else{
    newJobNode->isBG = 0;
  }
  newJobNode->pid = pid;
  newJobNode->cmdline = cmd->cmdline;
  newJobNode->next = NULL;
  return newJobNode;
}

static void Exec(commandT* cmd, bool forceFork)
{
  int pid;
  sigset_t sigmask;

  sigprocmask(SIG_BLOCK, &sigmask, NULL);

  if ((pid = fork()) < 0) {
    perror("fork failed");
  } else { 
    if (pid == 0){ /* Return 0 to the child */
      // Need to pass path name and arguments to execvp
      setpgid(0,0);
      execv(cmd->name, cmd->argv);
      
    } else { /* And the child PID to the parent */
      if (cmd->bg){
        jobL *newJob = addtolist(pid, cmd);
        sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
      }
      else{
        wait(NULL);
        sigprocmask(SIG_UNBLOCK, &sigmask, NULL);        
      }
    }
 } 
}

static bool IsBuiltIn(char* cmd)
{
  // Need to search for cmd[0] in file path
  if(strcmp(cmd, "cd") == 0) {
    return TRUE;
  } else if (strcmp(cmd, "bg") == 0) {
    return TRUE;
  } else if (strcmp(cmd, "fg") == 0) {
    return TRUE;
  } else if (strcmp(cmd, "jobs") == 0) {
    return TRUE;
  }
  return FALSE;     
}


void runbgJob(int job){
  // jobL *jobNode = jobs;
  // while(jobNode != NULL){
  //   if(jobNode->state == STOPPED && jobNode->id == job){
  //     kill(jobNode->pid, SIGCONT);
  //   }
  //   jobNode = jobNode -> next;
  // }
}

static void RunBuiltInCmd(commandT* cmd)
{
  if(strcmp(cmd->argv[0], "cd") == 0) {
    char* input = cmd->argv[1];
    if (input == NULL){
       chdir(getenv("HOME"));
       return;
    }
    char* cwd = getCurrentWorkingDir();
    char* newCwd = strcat(cwd, "/");
    newCwd = strcat(newCwd, input);    
    struct stat s;
    if(stat(input, &s) == 0){
      chdir(newCwd);
    }
  }


  if(strcmp(cmd->argv[0], "bg") == 0) {
    runbgJob(atoi(cmd->argv[1]));
  }

  if(strcmp(cmd->argv[0], "jobs") == 0) {
    CheckJobs();
  }

}

char* getCurrentWorkingDir(){
 char buff[1024];
 return getcwd(buff, 1024);
}

void CheckJobs()
{
  jobL *jobNode = jobs;

  while(jobNode != NULL){ 
    if (WIFEXITED(jobNode->status)){
      printf("[%d]   DONE                %s", jobNode->id, jobNode->cmdline);
    }
    jobNode = jobNode->next;
  }
}


commandT* CreateCmdT(int n)
{
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd){
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}
