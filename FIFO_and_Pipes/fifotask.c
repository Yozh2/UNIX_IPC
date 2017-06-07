#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>

int main () {
	int fd;	
	char buf [10];
	
	mkfifo("fifo", 666);
	printf("mkfifo(\"fifo\", 666); \n");
	perror("1");
	
	fd = open("fifo", O_RDWR);
	printf("fd = open(\"fifo\", O_RDWR); == %d\n", fd);
	perror("2");
	
	write(fd, "aaa", 3);
	printf("write(fd, \"aaa\", 3);\n");
	perror("3");

//	close(fd);
//    printf("close(fd); \n");
//	perror("4");
	
	fd = open("fifo", O_RDONLY //| O_NDELAY);
	printf("fd = open(\"fifo\", O_RDONLY | O_NDELAY); == %d\n", fd);
	perror("5");
	
	int len = read(fd, buf, 3);
    printf("int len = read(fd, buf, 3); == %d\n", len);
	perror("6");
	
	write(1,buf, len);
	printf("write(1, buf, len);\n");
	perror("7");
	
	return 0;
}
