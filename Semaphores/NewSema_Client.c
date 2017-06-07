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

#define IS_SERV 0    // 2 = SERVER HAS STARTED
                          // 1 = CLIENT HAS CONNECTED
                          // 0 = INIT or IF SERVER HAS DIED

#define IS_CLI 1    // 2 = CLIENT HAS STARTED
                          // 1 = SERVER HAS CONNECTED
                          // 0 = INIT or IF CLIENT HAS DIED

#define WAS_SERV 2  // 0 = INIT
                          // 1 = THERE WAS A SERVER (probably died)(set by client)

#define WAS_CLI 3  // 0 = INIT
                          // 1 = THERE WAS A CLIENT (probably died)(set by server)

#define DATA 4          // DATA PACKAGES READY FOR READ
#define MUTEX 5         // MUTEX BETWEEN SERVER-CLIENT RELATIONSHIP

struct package {
  int size;
  char data[DATA_SIZE];
};

#define SEMOP_PREPARE(number, operation, flags)\
    {\
        sem_ops[operaton_number].sem_num = number;\
        sem_ops[operaton_number].sem_op = operation;\
        sem_ops[operaton_number].sem_flg = flags;\
        operaton_number++;\
    }

void client_work (int semid, void* memptr) {
  int operaton_number = 0, temp; // for SEMOP_PREPARE, SEMOP_DO

  struct sembuf sem_ops[MAXOPSNUM];
  //ONLY ONE CLIENT SHOULD RUN NOW

  //CHECK IF THERE ARE NO CLIENTS AT THE MOMENT
  SEMOP_PREPARE(IS_CLI, 0, IPC_NOWAIT)
  //CHECK IF NO CLIENTS DIED ALREADY
  SEMOP_PREPARE(WAS_CLI, 0, IPC_NOWAIT)
  //SET 2, 1 FOR EXISTENCE AND 1 FOR SERVER TO TAKE
  SEMOP_PREPARE(IS_CLI, 2, SEM_UNDO)

  temp = operaton_number;
  operaton_number = 0;
  if (semop(semid, sem_ops, temp) == -1) {
    if (errno == EAGAIN) {
      printf("The client already existed. The progtam will exit\n");
      exit(EXIT_FAILURE);
    }
    else {
      perror ("Semop 1");
      exit(EXIT_FAILURE);
    }
  }

  //prinf("Done. Waiting for server to start transfer\n");

//+============================================================================+
//|           IN THAT STAGE ONLY 1 CLIENT EXISTS AND IT's THE 1st              |
//+============================================================================+

  SEMOP_PREPARE(IS_SERV, -1, 0) // make sure server runs
  SEMOP_PREPARE(WAS_SERV, 1, SEM_UNDO) // set the sem indicating server existence

  temp = operaton_number;
  operaton_number = 0;
  if (semop(semid, sem_ops, temp) == -1) {
    perror("Semop 2");
    exit(EXIT_FAILURE);
  }
  //printf("The server has connected\n");

//+============================================================================+
//|      IN THAT STAGE EVERYTHING SET PROPERLY AND CONNECTION IS SECURE        |
//+============================================================================+
  //DATA TRANSFER PACKET CREATION
  struct package* data = (struct package*) memptr;
  int bytes_write;

  do {
    SEMOP_PREPARE(IS_SERV, -1, IPC_NOWAIT)
    SEMOP_PREPARE(IS_SERV, 1, 0)
    SEMOP_PREPARE(DATA, -1, 0)
    SEMOP_PREPARE(MUTEX, 0, 0)
    SEMOP_PREPARE(MUTEX, 1, SEM_UNDO)

    temp = operaton_number;
    operaton_number = 0;
    if (semop(semid, sem_ops, temp) == -1) {
      if (errno == EAGAIN) {
        perror("The server is dead, the program will exit");
        exit(EXIT_FAILURE);
        }
      else {
        perror("Semop 3");
        exit(EXIT_FAILURE);
      }
    }

//CRITICAL SECTION IN DATA TRANSFER

    bytes_write = data->size;
    if (write(STDOUT_FILENO, data->data, bytes_write) == -1) {
      perror("writing to STDOUT error");
    }

    SEMOP_PREPARE(MUTEX, -1, SEM_UNDO)
    temp = operaton_number;
    operaton_number = 0;
    if (semop(semid, sem_ops, temp) == -1) {
      perror("Semop 4");
      exit(EXIT_FAILURE);
    }
  } while (bytes_write);
}

int main(int argc, char * argv[]) {
  //ARGCHECK
  if (argc != 1) {
    printf("%s: no arguments allowed\n", argv[0]);
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
  //printf("semid: %d; key: %d\n", semid, key);

  //ATTACHING TO MEMORY
  void * memptr = NULL;
  if ((memptr = shmat(shmid, NULL, 0)) == ((void *) -1)) {
    perror("shmat error");
    exit(EXIT_FAILURE);
  }

  //GO DOING CLIENT TRANSFER WORK
  client_work(semid, memptr);

  //THE CLIENT REMOVES IPC_RESOURCES SO IT EXITS AFTER SERVER
  shmctl(shmid, IPC_RMID, NULL);
  semctl(semid, 0, IPC_RMID);

  //DETACHING FROM EMMORY
  //CLIENT WILL CLEAR ALL
  shmdt(memptr);
  exit(EXIT_SUCCESS);
}
