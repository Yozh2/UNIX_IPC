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
#define PIPENAME_LENGTH 100
#define PID_LENGTH 15
#define BUFFSIZE PIPE_BUF
#define TIMEOUT 1
#define HIGHLIGHT "\n+============================+\n"
#define SERVDOWN "Looks like the server is down\n"

int connect_and_transfer();

int main () {
	int n_total = 0;
	while ((n_total = connect_and_transfer()) == 0) {
	//	printf("Didn't receive anything from active servers.\n");
	//	printf("Reconnect? y/n\n");
	//	char c;
	//	scanf("%c", &c);
	//	if (c == 'n')
			break;
	}
	return 0;
}

int connect_and_transfer() {
	//FIFO INIT
	int fd_priv_NB;
	int fd_priv_RW;
	int fd_q;								//fifo queue descriptor
	pid_t pid = getpid();

	char pid_str[PID_LENGTH];
	char buffer[BUFFSIZE];

	//MAKING PRIVATE FIFO NAME
	char fifo_priv[PIPENAME_LENGTH] = {0};
	snprintf(fifo_priv, strlen(PIPENAME_PRIVATE) + PID_LENGTH, "%s%d", PIPENAME_PRIVATE, pid);

	//CREATING PRIVATE FIFO with pid name
	mkfifo(fifo_priv, 0666);
	//OPENING fifo_priv
	fd_priv_RW = open(fifo_priv, O_RDWR);
	fd_priv_NB = open(fifo_priv, O_RDONLY);
	close(fd_priv_RW);
	//ADD PID INTO FIFO_Q
	mkfifo(PIPENAME_Q, 0666);
	fd_q = open(PIPENAME_Q, O_WRONLY);
	write(fd_q, &pid, sizeof(pid_t));
//	printf("client pid sent: %d\n", pid);

//ENTER CRITICAL SECTION - sleeping while server works with string and connects for transfer

	//WAITING FOR SERVER TO START READING
	sleep(TIMEOUT);
//	printf("Woke up\n");

	//READY TO READ FROM PRIVATE FIFO
	int n_total = 0;
	int n_b = 0;

	while (1) {
		n_b = read(fd_priv_NB, buffer, BUFFSIZE);

		if (n_b < 0) {
			continue;
		}
		if (n_b <= 0) {
			break;
		}
		if (n_b > 0) {
			n_total += n_b;
			if (write(STDOUT_FILENO, buffer, n_b) != n_b) {
				perror("write");
			}
		}
	}

//	write(STDOUT_FILENO, HIGHLIGHT, sizeof(HIGHLIGHT));
//	printf("Received in Total: %d bytes\n", n_total);

	//CLOSING PRIVATE FIFO AND EXIT
	close(fd_q);
	close(fd_priv_NB);
	unlink(fifo_priv);
	return n_total;
}
