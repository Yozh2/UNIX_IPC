//написать программу (или две) которые запускаются в двух разных терминалах
// первая открывает FIFO и читает тот файл который в argv[1] и пишет его в FIFO
// вторая пишет на экран из FIFO
//в процессе передачи через FIFO данные испорчены быть не должны в случае, если мы хахотим еще одну пару передатчика и копира запустим. Так что без FCNTL и MALLOC'ов, FLOCK/FL итд.
//то есть надо защитить FIFO от своих же дополнительных пар программ, чтобы они его не переписали.
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>

#define PIPENAME_Q "/tmp/fifo_main_queue"
#define PIPENAME_PRIVATE "/tmp/fifoPR_"
#define PIPENAME_LENGTH 100 //fifo name placeholder
#define PID_LENGTH 15
#define BUFFSIZE PIPE_BUF
#define TIMEOUT 1

// IF THE CLIENT IS DOWN RECEIVE SIGPIPE
void sig_pipe(int signum) {
	//	printf("SIGPIPE: the client is down\n");
	exit(101);
}

int main (int argc, char * argv[]) {

	//SET EXIT ACTION IF RECEIVED TERMINATION SIGNALS
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = sig_pipe;
	sigaction(SIGPIPE, &action, NULL);

	//ARGS CHECK
	if (argc != 2) {
//		printf("USAGE: %s input_file\n", argv[0]);
		return 0;
	}

	//FIFO INIT
	int fd_priv_NB; 		//nonblock private fifo descriptor
	int fd_priv_RW;
	int fd_q;						//fifo queue descriptor
	pid_t client_pid;
	char buffer[BUFFSIZE];

	//OPEN SOURCE FILE
	int file_source, n_b = 0, n_total = 0;
	if (( file_source = open(argv[1], O_RDONLY)) < 0) {
		perror("file_source");
		return 1;
	}

	//OPENING OR CREATING FIFO_Q
	mkfifo(PIPENAME_Q, 0666);
	if ((fd_q = open(PIPENAME_Q, O_RDWR)) < 0) {
		perror("fifo_queue opening");
		return 2;
	}

	//GETTING CLIENT PID
	while (client_pid == 0) {
		read(fd_q, &client_pid, sizeof(pid_t));
	}
	close(fd_q);
//	printf("Client pid received: %d\n", client_pid);

	//MAKING PRIVATE FIFO NAME
	char fifo_priv[PIPENAME_LENGTH] = {0};
  snprintf(fifo_priv, PID_LENGTH + strlen(PIPENAME_PRIVATE), "%s%d", PIPENAME_PRIVATE, client_pid);
//	printf("fifo_priv name generated %s\n", fifo_priv);

	//OPENING PRIVATE FIFO
	if (((fd_priv_RW = open(fifo_priv, O_RDWR)) < 0) && (errno == ENXIO)) {
		perror("fd_priv_RW opening");
		close(fd_priv_RW);
		close(file_source);
		exit(100);
	}
	fd_priv_NB = open(fifo_priv, O_WRONLY);
	close(fd_priv_RW);
//	printf("%s opened, starting writing\n", fifo_priv);

	//WRITING TO PRIVATE FIFO
	while ((n_b = read(file_source, buffer, BUFFSIZE)) > 0) {
			n_total += n_b;
			if (write(fd_priv_NB, buffer, n_b) <= 0) {
				if (errno == EAGAIN) {
					perror("full pipe");
					continue;
				}
				else if (errno == EPIPE) {
//					printf("EPIPE: The client is down!\n");
					exit(101);
				}
			}
			sleep(1);
	}
//	printf("data sent %d bytes\n", n_total);
	close(file_source);
	close(fd_priv_NB);
	return 0;
}
