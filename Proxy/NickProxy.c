/* Написать программу с 2 аргументами на входе
1. количество процессов, которые надо породить
2. Имена файлов, которые надо протолкнуть
есть родительский процесс, он взаимодействует с детьми
дети должны из одного ФД читать, а в другой ФД писать (block mode)
Родитель имеет кучу дескрипторов. Он читает из одного ребенка и передает в другого. Из последнего ребенка он пишет на выход.
все дочерние процессы должны быть реализованы одинаково
Родитель должен ОДНОВРЕМЕННО читать и писать в свои дескриптора
Советы по структуре родительского процесса
  1. не советую раскладывать данные по пачке разных массивов
  попробуй организоватьструктуру данных так, чтобы все≤ что отночится к одному соединению лежало в одной структуре
  2. Очень хочу попросить, чтобы у нас буфера, которые используются в процессах, были неодинаковыми.
  Они должны быть вычислены по формуле:
    для нулевого процесса - 3^n * 4k
    для первого - 3^(n-1) * 4k и так далее
  3. в Родителе для чтения и записи надо использовать буфер целиком (по одному байту читать нельзя)
  Ребенки имеют равные буфера любого размера.
  4. Для признака конца ввода нельзя использовать количество переданных байт или имя файла
  5. Не забудьте, что операции чтения и записи могут быть ЧАСТИЧНО УСПЕШНЫМИ
  6. 100% CPU поедать нельзя
*/
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

//CURRENT AND NEXT NODE DEFINITIONS
#define PAR parnods[NODE]
#define CH chnods[NODE]
#define CH_NEXT chnods[NODE+1]
#define FST 0
#define LST n-1

typedef struct parnod_t {
    int in, out;
    char *buf, *end;
    char *head, *tail;
    int complete;
} parnod_t;
parnod_t* parnods;
typedef struct chnod_t {
    int in, out;
} chnod_t;
chnod_t* chnods;
long n;
// SIMPLIFY PIPES
#define READ    0
#define WRITE   1

//OTHER TYPEDEFS
#define MAX_CHILDREN (maxfd_number_getter() - 3 - 2)/2
#define BUFFSIZE_MAX 1000000
#define BUFFSIZE_CHILD 1000
#define PIPESIZE 65536
#define BUFFER_CONSTANT 4
#define BASE 10

//SOME USEFUL FUNCTIONS HERE
size_t buffsize_getter(size_t NODE) {
  size_t res = BUFFER_CONSTANT;
  NODE = LST - NODE;
  //THAT BUFFER GROWS REVERSLY
  while(NODE-- && (res < BUFFSIZE_MAX))
      res *= 3;
  return res;
}
size_t maxfd_number_getter() {
  struct rlimit rl;
  if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
    perror("maxfd_number_getter getrlimit");
    exit(EXIT_FAILURE);
  }
  return (rl.rlim_cur - 1);
}
void structs_free() {
  for(size_t NODE = FST; NODE <= LST; NODE++)
      free(PAR.buf);
  free(parnods);
  free(chnods);
}
long strtolong_converter(char* str) {
  char *endptr;
  long val;
  errno = 0;
  val = strtol(str, &endptr, BASE);
  //POSSIBLE ERRORS CHECKING
  if (errno != 0 && val == 0) {
     perror ("Parsing the input number");
     exit(EXIT_FAILURE);
   }
  if (endptr == str) {
    perror("No digits found");
    exit(EXIT_FAILURE);
  }
  if (*endptr != '\0') {
     perror("Found characters after number");
     exit(EXIT_FAILURE);
   }
  return val;
};

//CHILD CODE FUNCTION

void children_work(size_t CUR_CH_NODE) {
  //CLOSING EXTRA PARENT's DESCRIPTORS
  for (size_t NODE = FST; NODE <= CUR_CH_NODE; NODE++) {
    if (close(PAR.in) == -1) {
      perror("PAR.in closing");
      exit(EXIT_FAILURE);
    }
    if (close(PAR.out) == -1) {
      perror("PAR.out closing");
      exit(EXIT_FAILURE);
    }
  }

  //LOCAL BUFFER INIT PROCESS
  size_t NODE = CUR_CH_NODE;
  char *buf, *head, *tail;

  buf = head = tail = (char * ) calloc(BUFFSIZE_CHILD, sizeof(char));
  if (!buf) {
    perror("calloc while local buffer init");
    exit(EXIT_FAILURE);
  }
  errno = 0;
  while ((tail += read(CH.in, buf, BUFFSIZE_CHILD)) != buf) {
    if (errno != 0) {
      perror("read");
      exit(EXIT_FAILURE);
    }

    //WRITING ALL THE DATA WE HAVE AND CAN TRANSFER
    while ((head += write(CH.out, head, tail - head)) != tail)
      if (errno != 0) {
        perror("write");
        exit(EXIT_FAILURE);
      }
    //RESET
    head = tail = buf;
  }

  if (close(CH.in) == -1){
    perror("CH.in closing in child");
    exit(EXIT_FAILURE);
  }
  if (close(CH.out)) {
    perror("CH.out closing in child");
    exit(EXIT_FAILURE);
  }
  structs_free();
  free(buf);
  //CHILD ALWAYS EXITS HERE!!!
  exit(EXIT_SUCCESS);
}

//PARENT TRANSFER & WRITE & READ FUNCTIONS

void read_work(size_t NODE, fd_set* readfds, fd_set* writefds) {
  ssize_t n_b_read;

  if ((n_b_read = read(PAR.in, PAR.tail, PAR.end - PAR.tail)) == -1) {
    perror("read_work read");
    exit(EXIT_FAILURE);
  }
  PAR.tail += n_b_read;

  //DATA ENDED
  if (!n_b_read) {
    PAR.complete = 0;
    close(PAR.in);
  }

  //IF FULL BUFFER
  if ((PAR.tail == PAR.end) || !n_b_read) {
    FD_CLR(PAR.in, readfds);
    FD_SET(PAR.out, writefds);
  }
}

void write_work(size_t NODE, fd_set* readfds, fd_set* writefds) {
  size_t n_b_write;

  if ((n_b_write = write(PAR.out, PAR.head, PAR.tail - PAR.head)) == -1) {
    perror("write_work write");
    exit(EXIT_FAILURE);
  }
  PAR.head += n_b_write;

  //IF WE'VE WRITTEN ALL THE DATA
  if (PAR.head == PAR.tail) {
    PAR.head = PAR.tail = PAR.buf;
    FD_CLR(PAR.out, writefds);

  //CLOSING CONNECTION IF NO DATA AVAILABLE
  if (!PAR.complete)
    close(PAR.out);
  else
    FD_SET(PAR.in, readfds);
  }
}

void transfer() {

  //FD_SET INIT PROCESS
  fd_set readfds, writefds;
  fd_set mod_readfds, mod_writefds; //select modifies it
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  //SET MAXFD
  size_t max_fd = 3; // MAX(STDERR, STDIN, STDOUT)
  for(size_t NODE = FST; NODE <= LST; NODE++) {
    max_fd = (max_fd > PAR.in) ? max_fd : PAR.in;
    max_fd = (max_fd > PAR.out) ? max_fd : PAR.out;
  }
  max_fd++;

  //ON THE INIT STAGE ALL PARNODS WAIT FOR CHILDERN TO TRANSFER THE DATA
  for(size_t NODE = FST; NODE <= LST; NODE++)
    FD_SET(PAR.in, &readfds);


  //TRANSFERR UNTILL DATA IS OVER
  while(parnods[LST].complete || (parnods[LST].tail - parnods[LST].head)) {
    mod_readfds = readfds;
    mod_writefds = writefds;

    //WAITING FOR FDs ON SELECT
    if (select(max_fd, &mod_readfds, &mod_writefds, 0, 0) == -1) {
      perror("Parent select");
      exit(EXIT_FAILURE);
    }

    for(size_t NODE = FST; NODE <= LST; NODE++) {
      if (FD_ISSET(PAR.in, &mod_readfds))
        read_work(NODE, &readfds, &writefds);
      if (FD_ISSET(PAR.out, &mod_writefds))
        write_work(NODE, &readfds, &writefds);
    }
  }
}

void init(size_t NODE) {
  //NODE INITIALISATION FUNCTION
  size_t size = buffsize_getter(NODE);
  PAR.buf = PAR.head = PAR.tail = (char*) calloc(size, sizeof(char));
  PAR.end = PAR.buf + size;
  PAR.complete = 1;

  //PIPES INITIALISATION
  int pipe1[2], pipe2[2]; // temporary store fds
  if (pipe(pipe1) == -1) {
    perror("pipe1 init");
    exit(EXIT_FAILURE);
  }
  if (pipe(pipe2) == -1) {
    perror("pipe2 init");
    exit(EXIT_FAILURE);
  }

  //CONNECTING DESCRIPTORS
  CH.out = pipe1[WRITE];
  PAR.in = pipe1[READ];

  //FST CHILD IS SET TO READING FROM INPUT ALREADY
  //DOING THE LST TO PRINT TO STDOUT
  //AND CONNECTING OTHER NODES BETWEEN THEM
  if (NODE == LST)
      parnods[LST].out = STDOUT_FILENO;
  else {
      PAR.out = pipe2[WRITE];
      CH_NEXT.in  = pipe2[READ];
  }

  //CHILD CREATION
  int fork_pid = fork();
  if (fork_pid == -1) {
    //SOMETHING WRONG HAPPENED
    perror("fork");
    exit(EXIT_FAILURE);
  }
  else if (fork_pid == 0)
    children_work(NODE);
  else {
    //CHANGE PARENT NODES DESCRIPTORS TO NONBLOCK
    if (fcntl(PAR.in, F_SETFL, O_NONBLOCK) == -1) {
      perror("PAR.in fcntl init error");
      exit(EXIT_FAILURE);
    }
    if (fcntl(PAR.out, F_SETFL, O_NONBLOCK) == -1) {
      perror("PAR.out fcntl init error");
      exit(EXIT_FAILURE);
    }

    //CLOSING EXTRA DESCRIPTORS IN CHILDREN
    if (close(CH.in) == -1) {
      perror("CH.in closing");
      exit(EXIT_FAILURE);
    }
    if (close(CH.out) == -1) {
      perror("CH.out closing");
      exit(EXIT_FAILURE);
    }
  }
}

int main(int argc, char* argv[]) {
  //ARGCHECK
  if (argc != 3) {
     printf("USAGE: %s [n] [filename]\n", argv[0]);
     exit(EXIT_FAILURE);
  }
  n = strtolong_converter(argv[1]);

  if (n > MAX_CHILDREN) {
    printf("Maximum number of children is %zu\n", MAX_CHILDREN);
  }

  //MEMORY ALLOCATIONS
  chnods  = calloc(n, sizeof(chnod_t));
  parnods = calloc(n, sizeof(parnod_t));
  if (!chnods || !parnods) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }

  //THE FST CHILD OPENS INPUT FILE AS IT's .in
  if ((chnods[FST].in = open(argv[2], O_RDONLY)) == -1) {
    perror("chmods[FST].in open");
    exit(EXIT_FAILURE);
  }

  // init parnods, produce children, remove redundant fds
  for(size_t NODE = FST; NODE <= LST; NODE++)
      init(NODE);

  //STARTING TRANSFER PROCESS
  transfer();
  //CLEAN UP
  structs_free();
  exit(EXIT_SUCCESS);
}
