//программа читает имя файла как аргумент и форкается
//родитель файл печатает
//ребенок файл вычитывает
//используй pipe для обмена между родителем и ребенком

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <sys/wait.h>
int main (int argc, char * argv[]) {
	//ARGS CHECK
	if (argc != 2) {
		printf("ERROR: Only one argument is allowed!");
		return 0;
	}

	//INIT 
	int fd[2];								//init file descriptors
	pipe(fd);								//creating pipe
	int file_source;	
	const int buff_size = 1024;
	char buffer[buff_size];
	int status, n_b = 0;

	if (fork() == 0) {						//if child process

		close(fd[0]);
		file_source = open(argv[1], O_RDONLY);
		while ((n_b = read(file_source, buffer, buff_size)) > 0) {
			if (n_b == 0) {
				break;
			} else
				write(fd[1], buffer, n_b);
		};
		close(file_source);
		close(fd[1]);
	} else {								//if parent process
		close(fd[1]);
		while ((n_b = read(fd[0], buffer, buff_size)) > 0) {
			write(STDOUT_FILENO, buffer, n_b);
		}
		close(fd[0]);
	}
	return 0;
}
