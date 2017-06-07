//Write a program that forks to child process.
//The child reads input file, the parent prints it into STDOUT
//They conduct a conversation via signals and nothing else
//RULES - can't pass bytes with signals. No printf/scanf inside signal handlers.

//Напишите программу, которая породит дочерний процесс.
//Дочерний читает файл, родитель печатает
//Общаются они через сигналы
//Вводная - передавать байты вместе с сигналами нельзя, взаимодействуй правильно.
//(printf/scanf нельзя пользоваться внутри обработчика сигналов)
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>


int output_byte = 0, shift = 128;
pid_t current_pid;

//SIGNAL HANDLERS
//SIGCHLD
void dead_child(int signum) {
  //printf("Sorry, sender-child process is dead\n");
  exit(EXIT_SUCCESS);
}

//SIGALRM
void dead_parent(int signum) {
  //printf("Sorry, receiver-parent process is dead\n");
  exit(EXIT_SUCCESS);
}

//CHECKING PARENT IS ALIVE
void tsoy(int signum) {
  //printf("Still alive!\n");
}

//DATA TRANSFER SIGNALS
//SIGUSR1
void one_bit(int signum) {
  //critical section entry - handling signals from the child
  //The resource - sigprocmask bytes. Which signal comes first - one/two/dead is a race condition.
  //who takes part - parent & child
  output_byte += shift;
  shift /= 2;
  //critical section exit
  kill(current_pid, SIGUSR1); //to answer signal is received
}

//SIGUSR2
void zero_bit(int signum) {
  //critical section entry - handling signals from the child
  //The resource - sigprocmask bytes. Which signal comes first - one/two/dead is a race condition.
  //who takes part - parent & child
  shift /= 2;
  //critical section exit
  kill(current_pid, SIGUSR1);
}

int main (int argc, char * argv[]) {
  //ARGS CHECK
  if (argc != 2) {
    fprintf(stderr, "USAGE: %s [source]\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  //PARENT PID REMEMBERING
  pid_t parent_pid = getpid();

  sigset_t set;

//CHANGING BLOCKED SIGNALS SET
  //SIGCHLD CALLS PARENT EXIT
  struct sigaction act_exit;
  memset(&act_exit, 0, sizeof(act_exit));
  act_exit.sa_handler = dead_child;
  sigfillset(&act_exit.sa_mask);
  sigaction(SIGCHLD, &act_exit, NULL);

  //SIGUSR1 CALLS one_bit() FUNCTION
  struct sigaction act_one_bit;
  memset(&act_one_bit, 0, sizeof(act_one_bit));
  act_one_bit.sa_handler = one_bit;
  sigfillset(&act_one_bit.sa_mask);
  sigaction(SIGUSR1, &act_one_bit, NULL);

  //SIGUSR2 CALLS  zero_bit() FUNCTION
  struct sigaction act_zero_bit;
  memset(&act_zero_bit, 0, sizeof(act_zero_bit));
  act_zero_bit.sa_handler = zero_bit;
  sigfillset(&act_zero_bit.sa_mask);
  sigaction(SIGUSR2, &act_zero_bit, NULL);

  //SET BLOCK MASKS
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGUSR2);
  sigaddset(&set, SIGCHLD);
  sigprocmask(SIG_BLOCK, &set, NULL);
  sigemptyset(&set);

  //FORK TO CREATE CHILD PROCESS
  current_pid = fork();

  //IF CHILD PROCESS - BECOME SENDER
  if (current_pid == 0) {
    char byte = 0;
    int fd = 0;
    sigemptyset(&set); //SIGSET clearing

    //SIGUSR1 IF EMPTY
    struct sigaction act_tsoy;
    memset(&act_tsoy, 0, sizeof(act_tsoy));
    act_tsoy.sa_handler = tsoy;
    sigfillset(&act_tsoy.sa_mask);
    sigaction(SIGUSR1, &act_tsoy, NULL);

    //SIGALRM IF DEAD PARENT
    struct sigaction act_alarm;
    memset(&act_alarm, 0, sizeof(act_alarm));
    act_alarm.sa_handler = dead_parent;
    sigfillset(&act_alarm.sa_mask);
    sigaction(SIGALRM, &act_alarm, NULL);

    if ((fd = open(argv[1], O_RDONLY)) < 0 ){
      perror("Can't open requested file");
      exit(EXIT_FAILURE);
    }

    int i;
    while (read(fd, &byte, 1) > 0){

      //SIGALRM IF PARENT-RECEIVER DIDN'T ANSWER AFTER TIMEOUT
      alarm(1);

      //BYTE TO BIT TRANSFER
      for ( i = 128; i >= 1; i /= 2){
        if ( i & byte)            // 1
          kill(parent_pid, SIGUSR1);
        else                      // 0
          kill(parent_pid, SIGUSR2);

        //WAITING FOR PARENT RESPONSE
        //printf("Marco!\n");
        //BLOCK UNTIL PARENT ANSWER

        //Critical section entry. Waiting until parent get data, handle it and answer, OR for alarm signal
        //Race condition in sigprocmask - which signal (tsoy/alarm) comes first
        sigsuspend(&set);
      }
    }
    //END OF FILE
    exit(EXIT_SUCCESS);
  }

  errno = 0;

  //PARENT TIME =================================
  //CATCHING SIGNALS UNTIL CHILD'S DEATH
  //Critical section somewhere here. Parent and his signal handlers are in race condition.
  //The resource is output_byte bits. Handlers can rewrite them until the main context prints them.
  do {
    if(shift == 0){ //WHOLE BYTE

      write(STDOUT_FILENO, &output_byte, 1);
      fflush(stdout);
      shift = 128;
      output_byte = 0;
    }
    //WAITING CHILD SIGNALS
    sigsuspend(&set);
  } while (1);

  exit(EXIT_SUCCESS);
}
