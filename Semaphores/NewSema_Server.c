//Написать программу, которая по аналогии с FIFO_transfer сделает трансфер через разделяемую память и через семафоры.
//Усложнение - должна работать только ОДНА ПАРА во время передачи. Файл при трансфере не должен быть испорчен
//Запускаться должны в произвольном порядке.
//См Consumer-Producer.
//Процессы можно прервать в любой момент.
//Можно воспользоваться этим - значения переменных семафора после того, как объект создан, равны нулю.

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <signal.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define PROJECT_ID 2874
#define SHARED_MEMORY_SIZE 4096
#define DATA_SIZE SHARED_MEMORY_SIZE - sizeof(int)
#define NAMESIZE 1000
#define SEMNUM 6
#define MAXOPSNUM 6

#define IS_SERV 0   // 2 = SERVER HAS STARTED
                    // 1 = CLIENT HAS CONNECTED
                    // 0 = INIT or IF SERVER HAS DIED

#define IS_CLI 1    // 2 = CLIENT HAS STARTED
                    // 1 = SERVER HAS CONNECTED
                    // 0 = INIT or IF CLIENT HAS DIED

#define WAS_SERV 2  // 0 = INIT
                    // 1 = THERE WAS A SERVER (probably died)(set by client)
#define WAS_CLI 3   // 0 = INIT
                    // 1 = THERE WAS A CLIENT (probably died)(set by server)

#define DATA 4      // DATA PACKAGES READY FOR READ
#define MUTEX 5     // MUTEX BETWEEN SERVER-CLIENT RELATIONSHIP

struct data_to_send {
  int data_size;
  char data[DATA_SIZE];
};

#define SEMOP_PREPARE(number, operation, flags)\
  {\
    sem_ops[operation_number].sem_num = number;\
    sem_ops[operation_number].sem_op = operation;\
    sem_ops[operation_number].sem_flg = flags;\
    operation_number++;\
    }

void server_work(char* filename, int semid, void* memptr) {
  int fd, operation_number = 0, temp; //SEMOP things

  if ((fd = open(filename, 0)) == -1) {
    perror("ERROR: cannot open input file");
    exit(EXIT_FAILURE);
  }
  printf("%s opened\n", filename);

  struct sembuf sem_ops[MAXOPSNUM];
  //ONLY ONE SERVER SHOULD RUN NOW

  //CHECK IF THERE ARE NO SERVERS AT THE MOMENT
  SEMOP_PREPARE(IS_SERV, 0, IPC_NOWAIT)
  //CHECK IF NO SERVERS DIED ALREADY
  SEMOP_PREPARE(WAS_SERV, 0, IPC_NOWAIT)
  //SET 2, 1 FOR EXISTENCE AND 1 FOR CLIENT TO TAKE
  SEMOP_PREPARE(IS_SERV, 2, SEM_UNDO)

  temp = operation_number;
  operation_number = 0;

  if (semop(semid, sem_ops, temp) == -1) {
    if (errno == EAGAIN) {
      printf("The server already existed, the program will exit\n");
      exit(EXIT_FAILURE);
    }
    else {
      perror("Semop 1");
      exit(EXIT_FAILURE);
    }
  }
  else
    printf("Done. Waiting for client to start transfer\n");

  //GET RID OF OTHER SERVERS - ONLY 1 NOW IS RUNNONG
  //CHECK IF CLIENT IS RUNNING
  SEMOP_PREPARE(IS_CLI, -1, 0)
  //SET SEM INDICATE CLIENT EXISTENCE
  SEMOP_PREPARE(WAS_CLI, 1, SEM_UNDO)

  temp = operation_number;
  operation_number = 0;
  if (semop(semid, sem_ops, temp) == -1) {
    perror("Semop 2");
    exit(EXIT_FAILURE);
  }
  else
    printf("The client has been connected\n");

  //DATA TRANSFER PACKET CREATION
  struct data_to_send* data = (struct data_to_send*) memptr;
  int bytes_read;

  //SAVING DATA TO 0 IN CASE SEMARRAY ISN'T DELETED AFTER SERVER's DEATH
  SEMOP_PREPARE(DATA, +1, SEM_UNDO)
  SEMOP_PREPARE(DATA, -1, 0)

  temp = operation_number;
  operation_number = 0;
  if (semop(semid, sem_ops, temp) == -1) {
    perror("Semop 2 again");
    exit(EXIT_FAILURE);
  }

  do {
    SEMOP_PREPARE(IS_CLI, -1, IPC_NOWAIT)
    SEMOP_PREPARE(IS_CLI, 1, 0)
    SEMOP_PREPARE(DATA, 0, 0)
    SEMOP_PREPARE(MUTEX, 0, 0) //MUTEX INVERSION
    SEMOP_PREPARE(MUTEX, 1, SEM_UNDO) // UNDO makes sure it is 0 in case of death

    temp = operation_number;
    operation_number = 0;
    if (semop(semid, sem_ops, temp) == -1) {
      if (errno == EAGAIN){
        perror("The client is dead, the program will exit");
        exit(EXIT_FAILURE);
      }
      else {
        perror("Semop 3");
        exit(EXIT_FAILURE);
      }
    }

//CRITICAL SECTION IN DATA TRANSFER

    if ((bytes_read = read(fd, data->data, DATA_SIZE)) == -1) {
      perror ("reading from input file");
      exit(EXIT_FAILURE);
    }
    data->data_size = bytes_read;
    SEMOP_PREPARE(MUTEX, -1, SEM_UNDO) // UNDO BALANCING
    //SEM_UNDO here's important to keep DATA to 0 even if semarray isn't deleted
    SEMOP_PREPARE(DATA, +1, 0)
    temp = operation_number;
    operation_number = 0;
    if (semop(semid, sem_ops, temp) == -1) {
      perror ("Semop 4");
      exit(EXIT_FAILURE);
    }
  } while (bytes_read);
  close(fd);
};

int main(int argc, char * argv[]) {
  //ARGCHECK
  if (argc != 2) {
    printf("USAGE: %s [input filename]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  //SHARED MEMORY INIT
  int key = ftok("/tmp", PROJECT_ID);
  int shmid, semid;
  if((shmid = shmget(key, SHARED_MEMORY_SIZE, IPC_CREAT | 0666)) == -1) {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  //SEMAPHORES INIT
  if((semid = semget(key, SEMNUM, IPC_CREAT | 0666)) == -1) {
    perror("semget");
    exit(EXIT_FAILURE);
  }

  //SEMAPHORES AND SHMEM NOW CREATED
  printf("semid: %d; key: %d\n", semid, key);

  //ATTACHING TO MEMORY
  void * memptr = NULL;
  if ((memptr = shmat(shmid, NULL, 0)) == ((void *) -1)) {
    perror("shmat error");
    exit(EXIT_FAILURE);
  }

  //GO DOING SERVER TRANSFER WORK
  server_work(argv[1], semid, memptr);

  //DETACHING FROM EMMORY
  //CLIENT WILL CLEAR ALL
  shmdt(memptr);
  exit(EXIT_SUCCESS);
}
