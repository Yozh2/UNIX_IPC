#include <sys/types.h>
#include <sys/msg.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define MAX_PROCESSES 999
#define MIN_PROCESSES 1

#define MSG_READ 0400
#define MSG_WRITE 0200

#define MSG_TYPE_PARENT 1048576

#define SIZE_OF_BUF 100

int main(int argc, char * argv[]) {
	long int n = 0, i = 0, number_of_birth = 0;
	char * ptr_nnumber = NULL;
	pid_t pid = 0;
	int msg_id = 0;

	typedef struct my_msgbuf
	{
		long int mtype;
		long int number_of_birth;
	} message;

	message msg_for_child, msg_for_parent;
	char buf[SIZE_OF_BUF];

	//ARGCHECK
	if (argc != 2) {
    fprintf(stderr, "USAGE: %s [n child processes]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// argv[1] CONDITIONS CHECK
	if (!((n = strtol(argv[1], &ptr_nnumber, 10)) >= MIN_PROCESSES && (n <= MAX_PROCESSES) && (*ptr_nnumber == '\0') && (errno != ERANGE)))
	{
		if (n < MIN_PROCESSES || n > MAX_PROCESSES)
			fprintf(stderr, "ERROR: Incalid child amount\n");
		else
			fprintf(stderr, "ERROR: NAN\n");
		exit(EXIT_FAILURE);
	}

	pid = getpid();

	if ((msg_id = msgget(IPC_PRIVATE, MSG_READ | MSG_WRITE | IPC_CREAT)) == -1)
	{
		perror("ERROR: in function msgget");
		fprintf(stderr, "ERROR: It doesn't make msg\n");
		exit(EXIT_FAILURE);
	}

	for (i = 1; i <= n; i++)
	{
		if (pid)
		{
			number_of_birth = i;
			pid = fork();
			if (pid == -1)
			{
				perror("ERROR: in function fork");
				fprintf(stderr, "ERROR: It doesn't make process\n");
				exit(EXIT_FAILURE);
			}
		}
		else
		{
			break;
		}
	}

	if (pid)
	{
		for (i = 1; i <= n; i++)
		{
			msg_for_child.mtype = i;
			msg_for_child.number_of_birth = i;
			if ((msgsnd(msg_id, &msg_for_child, sizeof(long int), 0)) == -1)
			{
				perror("ERROR: in function msgsnd");
				fprintf(stderr, "ERROR: It (parent) doesn't send message\n");
				exit(EXIT_FAILURE);
			}

			if ((msgrcv(msg_id, &msg_for_parent, sizeof(long int), MSG_TYPE_PARENT, 0)) == -1)
			{
				perror("ERROR: in function msgrcv");
				fprintf(stderr, "ERROR: It (parent) doesn't receiv message\n");
				exit(EXIT_FAILURE);
			}
		}
		printf("\n");
		if ((msgctl(msg_id, IPC_RMID, NULL)) == -1)
		{
			perror("ERROR: in function msgctl");
			fprintf(stderr, "ERROR: It (parent) doesn't delete msg\n");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		if ((msgrcv(msg_id, &msg_for_child, sizeof(long int), number_of_birth, 0)) == -1)
		{
			perror("ERROR: in function msgrcv");
			//fprintf(stderr, "Some problems with message receiving");
			exit(EXIT_FAILURE);
		}

		sprintf(buf, "%ld ", number_of_birth);

		write(STDOUT_FILENO, buf, strlen(buf));

		//printf("%ld ", number_of_birth);

		msg_for_parent.mtype = MSG_TYPE_PARENT;
		msg_for_parent.number_of_birth = number_of_birth;
		if ((msgsnd(msg_id, &msg_for_parent, sizeof(long int), 0)) == -1)
		{
			perror("ERROR: in function msgsnd");
			fprintf(stderr, "ERROR: It (child) doesn't send message\n");
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}
