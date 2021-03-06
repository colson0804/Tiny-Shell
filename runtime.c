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
#define RUNNING 1
#define DONE 2
#define STOPPED 3

typedef struct job_l {
  pid_t pid;
  int jobNum;
  int isBG;
  int status;
  char *cmdline;
  struct job_l* next;
} jobL;

// Linked list for alias command (unimplemented)
typedef struct alias_l {
  char *alias;
  char* realCmd;
  struct alias_l* next;
} aliasL;

/* the pids of the background processes */
jobL *jobs = NULL;
aliasL *aliasList = NULL;

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

void StopFgProc() {
  // Called from SIGTSTP signal handler
  // Just run through list of jobs and stop the one that's
  //  in the foreground
  jobL* current = jobs;
  while (current != NULL) {
    if (!current->isBG) {
      current->status = STOPPED;
      kill(-current->pid, SIGTSTP);
      printf("[%d]   Stopped                %s\n", current->jobNum, current->cmdline);
      fflush(stdout);
      break;
    }
    current = current->next;
  }
}

/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  if (ResolveExternalCmd(cmd)){
    Exec(cmd, fork);
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
        if(access(buf,X_OK) == 0){ /*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(buf);
          return TRUE;
        }
    }
  }
  return FALSE; /*The command is not found or the user doesn't have enough priority to run.*/
}

void removeFromList(pid_t pid) {
  // Remove and free job with given pid
  jobL *jobNode = jobs;
  jobL *prev = NULL;
  jobL *next = NULL;
  while(jobNode != NULL){
    next = jobNode->next;
    if ((jobNode->pid == pid) && (jobNode->status != STOPPED)){
      if (prev){
        prev->next = next;
      }
      else{
        jobs = jobNode->next;
      }
      free(jobNode);
      jobNode = next;
    }
    else{
      prev = jobNode;
      jobNode = next;
    }
  }
}

void addtolist(pid_t pid, commandT* cmd){
  // Add to job list
  jobL *jobList = jobs;
  jobL *newJobNode = (jobL *)malloc(sizeof(jobL));

  newJobNode->next = NULL;
  int newid = 0;

  if (jobList == NULL){
    newJobNode->jobNum = 1;
    jobs = newJobNode;
  }
  else{
    newid = jobList->jobNum;
    while(jobList->next !=NULL){
      if(newid < jobList->jobNum)
        newid = jobList->jobNum;
      jobList = jobList->next;
    }
    jobList->next = newJobNode;
    newJobNode->jobNum = newid + 1;
  }

  if (cmd->bg){
    newJobNode->isBG = 1;
  }
  else{
    newJobNode->isBG = 0;
  }
  newJobNode->pid = pid;
  newJobNode->cmdline = cmd->cmdline;
  newJobNode->next = NULL;
  newJobNode->status = RUNNING;
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
      // Need to pass path name and arguments to execv
      setpgid(0, 0);
      sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
      execv(cmd->name, cmd->argv);
      
    } else { /* And the child PID to the parent */
      if (cmd->bg){
        // Simply add to list, but don't wait to finish before prompting user
        addtolist(pid, cmd);
        sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
      }
      else{
        int statusCode;
        addtolist(pid, cmd);
        sigprocmask(SIG_UNBLOCK, &sigmask, NULL); 
        // If in foreground, wait until finished to remove from job list
        while(waitpid(pid, &statusCode, WNOHANG|WUNTRACED) == 0){
          sleep(1);
        }
        removeFromList(pid);
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
  } else if (strcmp(cmd, "alias") == 0){
    return TRUE;
  }
  return FALSE;     
}

void runInFg(int job){
  // Run when user types fg
  jobL *jobNode = jobs;
  int status;
  while(jobNode != NULL){
    if(jobNode->jobNum == job) {
      jobNode->isBG=0;
      jobNode->status = RUNNING;
      // Continue job, then wait until finished
      kill(-jobNode->pid, SIGCONT);
      while(waitpid(jobNode->pid, &status, WUNTRACED|WNOHANG) == 0){
          sleep(1);
      }
      removeFromList(jobNode->pid);
      return;
    }
    jobNode = jobNode -> next;
  }
}

void runbgJob(int job){
  // Run when user types bg
  // Same as runInFg, but don't wait for process to complete
  jobL *jobNode = jobs;
  while(jobNode != NULL){
    if(jobNode->jobNum == job) {
      jobNode->status=RUNNING;
      kill(jobNode->pid, SIGCONT);
      return;
    }
    jobNode = jobNode -> next;
  }
}

void runJobCmd()
{
  // Run when user types 'jobs'
  jobL *jobNode = jobs;
  while(jobNode != NULL) {   
    int statusCode;
    // res returns 0 if job is still running
    pid_t res = waitpid(jobNode->pid, &statusCode, WUNTRACED|WNOHANG); 
    if (jobNode->status == STOPPED) {
      printf("[%d]   Stopped                %s\n", jobNode->jobNum, jobNode->cmdline);
      fflush(stdout);
    }
    else if (res == 0) { 
      printf("[%d]   Running                %s &\n", jobNode->jobNum, jobNode->cmdline);
      fflush(stdout);
    }
    jobNode = jobNode->next;
  }
}

static void removeAlias(char *cmd){

}

static void runAlias(commandT* newCmd){
  printf("Number of commands: %d\n", newCmd->argc);
  aliasL *aliasNode = aliasList;
  if (newCmd->argc == 0){
    while (aliasNode != NULL){
      printf("alias %s='%s\n'", aliasNode->alias, aliasNode->realCmd);
    }
  }
  else{
    //foo='ls -lh' is ending after the space...
    //we need to add stuff to the interpreter to look for "alias" and "="
    while (aliasNode != NULL) {
      
      aliasNode = aliasNode->next;
    }
    aliasNode = (aliasL *)malloc(sizeof(aliasL));
    aliasNode->realCmd = strdup(newCmd->argv[1]);
    aliasNode->alias = strdup(newCmd->argv[1]);
    printf("Command is: %s\n", aliasNode->realCmd);
    aliasNode->next = NULL;
  }
}

static void RunBuiltInCmd(commandT* cmd)
{
  if(strcmp(cmd->argv[0], "cd") == 0) {
    char* input = cmd->argv[1];
    if (input == NULL){
       if(chdir(getenv("HOME")))
        printf("Error changing directory\n");
       return;
    }
    char* cwd = getCurrentWorkingDir();
    char* newCwd = strcat(cwd, "/");
    newCwd = strcat(newCwd, input);    
    struct stat s;
    if(stat(input, &s) == 0){
      if(chdir(newCwd))
        printf("Error changing directory\n");
    }
  }
  else if (strcmp(cmd->argv[0], "unalias") == 0){
    removeAlias(cmd->argv[1]);
  }
  else if (strcmp(cmd->argv[0], "alias") == 0){
    runAlias(cmd);
  }
  else if(strcmp(cmd->argv[0], "bg") == 0) {
    runbgJob(atoi(cmd->argv[1]));
  }
  else if(strcmp(cmd->argv[0], "fg") == 0) {
    runInFg(atoi(cmd->argv[1]));
  }
  else if(strcmp(cmd->argv[0], "jobs") == 0) {
    runJobCmd();
  }   
}

void CheckJobs() {
  // Called in tsh.c
  // Will tell if job is completed and remove from job list
  jobL *jobNode = jobs;
  jobL *next = NULL; 
  while(jobNode != NULL){  
    next = jobNode->next;
    int statusCode;
    // Res pid returns 1 if job is finished
    pid_t res = waitpid(jobNode->pid, &statusCode, WNOHANG); 
    if (res){
      printf("[%d]   Done                   %s\n", jobNode->jobNum, jobNode->cmdline);
      fflush(stdout);
      removeFromList(jobNode->pid);
    } 
    jobNode = next;
  } 
}

char* getCurrentWorkingDir(){
 char buff[1024];
 return getcwd(buff, 1024);
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

void IntFgProc() {
  // Called from SIGINT handler
  jobL* curr = jobs;
  while (curr) {
    if (!curr->isBG) {
      // Interrupt and completely remove job
      kill(-curr->pid, SIGINT);
      removeFromList(curr->jobNum);
    }
    curr = curr->next;
  }
}
