//Написать программу, порождающую n детей. После полного порождения всех процессов.
//После этого эти дети должны напечатать свой порядковый номер в том порядке, в котором их породили
//sПользоваться можно только этими вещами сверху (msg)

#include <sys/types.h>
#include <sys/msg.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define N_MAX 1000
#define N_MIN 1
#define MSG_READ 0400
#define MSG_WRITE 0200
#define BUFFSIZE 100
#define SERVER_MSGTYPE 1048576

//MESSAGE STRUCTURE DEFINITION
typedef struct nick_msg {
  long int type;
  long int birth_order;
} mesg;

int main (int argc, const char * argv[]) {
  //INIT
  mesg to_printer, to_server;
  char buffer[BUFFSIZE];
  long int n = 0, birth_num = 0;
  long int i = 0;
  char * ptr_num = NULL;
  pid_t pid = getpid();
  int msg_id;

  //ARGCHECK
  if (argc != 2) {
    fprintf(stderr, "USAGE: %s [n printers]\n", argv[0]);
    exit(EXIT_FAILURE);
  }
	if (!((n = strtol(argv[1], &ptr_num, 10)) >= N_MIN && (n <= N_MAX) && (*ptr_num == '\0') && (errno != ERANGE)))
	{
		if (n < N_MIN || n > N_MAX)
			fprintf(stderr, "ERROR: printer quantity should be from %d to %d\n", N_MIN, N_MAX);
		else
			fprintf(stderr, "ERROR: Entered argument is not an integer\n");
		exit(EXIT_FAILURE);
	}

  //MESSAGE QUEUE CREATION
	if ((msg_id = msgget(IPC_PRIVATE, MSG_READ | MSG_WRITE | IPC_CREAT)) == -1)
	{
  	perror("ERROR: msg_id creation fault");
  	exit(EXIT_FAILURE);
  }
  //FORK N TIMES AND REMEMBER BIRTH NUMBER
  for (i = 1; i <= n; i++)
  {
    //IF PARENT - FORKING
    if (pid)
    {
      birth_num = i; //pid num is saved different in each printer
      pid = fork();
      if (pid == -1)
      {
        perror("ERROR: printer creation while fork");
        exit(EXIT_FAILURE);
      }
    }
    //IF CHILD - SKIPPING
    else
    {
      break;
    }
  }

  //SERVER PART - SENDING MESSAGES TO PRINT
  if (pid) //Parent work
  {
    for (i = 1; i <= n; i++)
    {
      to_printer.type = i;
      to_printer.birth_order = i;

      //SEND MESSAGE TO PRINTER TO PRINT
      if ((msgsnd(msg_id, &to_printer, sizeof(long int), 0)) == -1)
      {
        perror("ERROR: Server: Message sending failure");
        exit(EXIT_FAILURE);
      }
      //WAIT UNTIL ANSWER MESSAGE (if printer did printwork)
      if ((msgrcv(msg_id, &to_server, sizeof(long int), MSG_TYPE_PARENT, 0)) == -1)
      {
        perror("ERROR: Server: Message receiving failure");
        exit(EXIT_FAILURE);
      }
    }
    //printf("\n");
    //MESSAGE QUEUE REMOVING
    if ((msgctl(msg_id, IPC_RMID, NULL)) == -1)
    {
      perror("ERROR: msgctl queue deleting failure");
      exit(EXIT_FAILURE);
    }
  }
  //PRINTER PART - WAITING THE MESSAGE AND THEN PRINT
  else
  {
    if ((msgrcv(msg_id, &to_printer, sizeof(long int), birth_num, 0)) == -1)
    {
      perror("ERROR: Printer: msgrcv fault");
      exit(EXIT_FAILURE);
    }

    //PRINTING THE NUMBER
    sprintf(buf, "%ld ", birth_num);
    write(STDOUT_FILENO, buf, strlen(buf));
    fflush(stdout);

    to_server.type = SERVER_MSGTYPE;
    to_server.birth_order = birth_num;
    if ((msgsnd(msg_id, &to_server, sizeof(long int), 0)) == -1)
    {
      perror("ERROR: Printer: msgsnd to server fault");
      exit(EXIT_FAILURE);
    }
  }
  return 0;
}
