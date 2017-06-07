//Написать программу, порождающую n детей. После полного порождения всех процессов.
//После этого эти дети должны напечатать свой порядковый номер в том порядке, в котором их породили

#include <sys/ipc.h>
#include <stdio.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MSG_READ 0400
#define MSG_WRITE 0200
#define MAX_MESSAGE_SIZE (sizeof(pid_t) + 1)

//#define IPC_PRIVATE 3141592

struct my_mesg
{
	long type; //The message type is positive number
	char text[MAX_MESSAGE_SIZE]; //The message data the volume of which is equal to MAX_MESSAGE_SIZE
};

int main(int argc, const char *argv[])
{
	//Check arguments
	if (argc != 2)
	{
		perror("Invalid arguments!\n");
		return 0;
	}
	
	//Initialize variables
	long int i = 0; //Counter
	char * x;	//Special for errors in strtoll() fuction
	long n = strtol(argv[1], &x, 10); //This function convert string to long long
	pid_t pid; 
//	pid_t parent_pid = getpid(); //Remember parent pid
	int child_exit_code;
	struct msqid_ds message_prop;
	
	//Initialize the message queue
	int msqid, msgsnd_error;
	
	if ((msqid = msgget(IPC_PRIVATE, MSG_READ | MSG_WRITE | IPC_CREAT | IPC_EXCL)) == EEXIST) //Try to create new message queue and get msqid
	{
		msqid = msgget(IPC_PRIVATE, 0); //If it is impossible, then we get the ID of an existing and delete it
		
		if (msgctl(msqid, IPC_RMID, &message_prop) != 0)
		{
			perror("Error of access rights when deleting an existing message queue!\n");
			return 0;
		}
		
		msqid = msgget(IPC_PRIVATE, MSG_READ | MSG_WRITE | IPC_CREAT); //Create new message and get msqid
	}
	
	//Create n children
	for (i = 0; i < n; i++)
	{
		if ((pid = fork()) == 0)
		{
			struct my_mesg message;
			message.type = i + 1;
			sprintf(message.text, "%d", getpid());
			
			if ((msgsnd_error = msgsnd(msqid, &message, MAX_MESSAGE_SIZE, IPC_NOWAIT)) == EAGAIN) //Try to send message and else handle the error
			{
				perror("Overflow message queue!\n");
				return 0;
			}
			else if (msgsnd_error == EIDRM)
			{
				perror("The message queue is deleted!\n");
				return 0;
			}
			
			break;
		}
	}
	struct my_mesg message;
	
	if (pid != 0) //Parent
	{
		for (i = 1; i <= n; i++) //Waiting until children from 1 to n print their numbers
		{
			if (msgrcv(msqid, &message, MAX_MESSAGE_SIZE, i, 0) == -1) //Give pid i-th Child
			{
				perror("Error when receiving messages!\n");
				return 0;
			}
			pid = (pid_t) strtol(message.text, &x, 10);
			waitpid(pid, &child_exit_code, 0);
			
			message.type = i + n;
			//Inform other processes about the completion of the i-th
			if ((msgsnd_error = msgsnd(msqid, &message, MAX_MESSAGE_SIZE, IPC_NOWAIT)) == EAGAIN) 
			{
				perror("Overflow message queue!\n");
				return 0;
			}
			else if (msgsnd_error == EIDRM)
			{
				perror("The message queue is deleted!\n");
				return 0;
			}
			
		}
		//Deleting message queue
		if (msgctl(msqid, IPC_RMID, &message_prop) != 0)
			perror("Error of access rights when deleting an existing message queue!\n");
		
		return 0;
	}
	
	//Print the serial number child-processes in the order of their generation
	if (i == 0) //Caught the first child
	{
		printf("I am dead: 1 %d\n", getpid());
		exit(0);
	}
	//Try to give information about the completion of the (i - 1)-th
	if (msgrcv(msqid, &message, MAX_MESSAGE_SIZE, i + n, 0) == -1)
	{
		perror("Error when receiving messages!\n");
		return 0;
	}
	
	printf("I am dead: %ld %d\n", i + 1, getpid());
	exit(0);
}
