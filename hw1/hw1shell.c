#include <signal.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "unistd.h"
#include <wait.h>
#include <errno.h>
#include <fcntl.h>



int fg_process(char** arglist){
  int pid = fork();
  if (pid < 0){
    fprintf(stderr, "fork error in fg_process: %s", strerror(errno));
    return 0;
  }
  else if (pid==0){
    //fg child process should be terminated on SIGINT
    struct sigaction sigint;
    sigint.sa_handler = SIG_DFL;
    sigint.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sigint, 0)<0){
      fprintf(stderr, "error in sigation in fg_process: %s", strerror(errno));
      return 0;
    }
    execvp(arglist[0],arglist);
    //if code executed after execvp there's an error
    fprintf(stderr, "error in execvp in fg_process: %s", strerror(errno));
    return 0;
  }
  else {
    waitpid(pid, NULL, WUNTRACED);
  }
  return 1;
}

int bg_process(char** arglist){
  int pid = fork();
  if (pid < 0){
    fprintf(stderr, "fork error in bg_process: %s", strerror(errno));
    return 0;
  }
  else if (pid==0){
    //child process
    if (execvp(arglist[0],arglist)<0){
      fprintf(stderr, "error in execvp bg_process: %s", strerror(errno));
      return 0;
    }
  }
  return 1;
}