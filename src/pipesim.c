#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*  CITS2002 Project 1 2020
    Name:                Michael Nefiodovas
    Student number(s):   22969312
 */

//  MAXIMUM NUMBER OF PROCESSES OUR SYSTEM SUPPORTS (PID=1..20)
#define MAX_PROCESSES 20

//  MAXIMUM NUMBER OF SYSTEM-CALLS EVER MADE BY ANY PROCESS
#define MAX_SYSCALLS_PER_PROCESS 50

//  MAXIMUM NUMBER OF PIPES THAT ANY SINGLE PROCESS CAN HAVE OPEN (0..9)
#define MAX_PIPE_DESCRIPTORS_PER_PROCESS 10

//  TIME TAKEN TO SWITCH ANY PROCESS FROM ONE STATE TO ANOTHER
#define USECS_TO_CHANGE_PROCESS_STATE 5

//  TIME TAKEN TO TRANSFER ONE BYTE TO/FROM A PIPE
#define USECS_PER_BYTE_TRANSFERED 1

//  ---------------------------------------------------------------------

//  YOUR DATA STRUCTURES, VARIABLES, AND FUNCTIONS SHOULD BE ADDED HERE:
enum syscall_t {
  SYS_COMPUTE,
  SYS_SLEEP,
  SYS_EXIT,
  SYS_FORK,
  SYS_WAIT,
  SYS_PIPE,
  SYS_WRITEPIPE,
  SYS_READPIPE,

  SYS_UNASSIGNED // Placeholder value for easier debugging
};

struct pipe_transmission_t {
  int descriptor;
  int nbytes;
};

// Unions allow me to store different kinds of command data
// It allows code to remain readable without becoming overcomplicated
union syscall_data_t {
  int microseconds;
  int pid;
  int descriptor;
  struct pipe_transmission_t pipe_info;
};

enum process_state_t { ST_READY, ST_RUNNING, ST_WAITING, ST_UNASSIGNED };

int timetaken = 0;
struct {
  enum process_state_t state;
  int next_syscall;

  struct {
    enum syscall_t syscall;
    union syscall_data_t data;
  } syscall_queue[MAX_SYSCALLS_PER_PROCESS];

} process_list[MAX_PROCESSES];

void clear_process(int pid) {
  process_list[pid].next_syscall = 0;

  // Initialise all syscalls to default value
  for (int call = 0; call < MAX_SYSCALLS_PER_PROCESS; call++) {
    process_list[pid].syscall_queue[call].syscall = SYS_UNASSIGNED;
  }
}

void initialise_process_list() {
  for (int pid = 0; pid < MAX_PROCESSES; pid++) {
    clear_process(pid);
  }
}

// Helper functions for the ready queue
bool queue_empty(int head, int tail) { return head == tail; }
int dequeue(int *queue, int *head) { return queue[(*head)++]; }
void enqueue(int *queue, int *tail, int value) { queue[(*tail)++] = value; }

int rdy_head = 0;
int rdy_tail = 0;
int rdy_queue[MAX_PROCESSES];

#define STATE_NAME_LEN 15

void state_transition(int pid, enum process_state_t new_state) {
  process_list[pid].state = new_state;

  char state[STATE_NAME_LEN];
  switch (new_state) {
  case ST_READY: {
    snprintf(state, sizeof(state), "READY");
    break;
  }
  case ST_RUNNING: {
    snprintf(state, sizeof(state), "RUNNING");
    break;
  }
  case ST_WAITING: {
    snprintf(state, sizeof(state), "WAITING");
    break;
  }
  case ST_UNASSIGNED: {
    snprintf(state, sizeof(state), "REMOVED");
    break;
  }
  default: {
    snprintf(state, sizeof(state), "!UNKNOWN!");
    break;
  }
  }

  printf("[#] Changing process_%i's state -> %s\n", pid, state);
  if (ST_READY == new_state) {
    // make sure to add processes which are ready to the "ready" queue
    enqueue(rdy_queue, &rdy_tail, pid);
  }

  timetaken += USECS_TO_CHANGE_PROCESS_STATE;
}

void sim_exit(int pid) {
  clear_process(pid);
  state_transition(pid, ST_UNASSIGNED);
}

void sim_compute(int pid, int time_quantum) {
  int syscall_index = process_list[pid].next_syscall;

  int usecs = process_list[pid].syscall_queue[syscall_index].data.microseconds;

  if (usecs > time_quantum) {
    timetaken += time_quantum;
    usecs -= time_quantum;
  } else {
    timetaken += usecs;
    // We have finished computing
    usecs = 0;
    ++process_list[pid].next_syscall;
  }

  process_list[pid].syscall_queue[syscall_index].data.microseconds = usecs;
  state_transition(pid, ST_READY);
}

void sim_fork(int parent_pid) {
  int syscall_index = process_list[parent_pid].next_syscall;
  // subtract 1 to get the process index from 0 (rather than from 1)
  int child_pid =
      process_list[parent_pid].syscall_queue[syscall_index].data.pid - 1;

  // In reality the child would inherit all the
  // state of the parent, but since there's no
  // state for these processes.

  ++process_list[parent_pid].next_syscall;

  state_transition(child_pid, ST_READY);
  state_transition(parent_pid, ST_READY);
}

// Uses the values in the command_queue to find and set
// the timetaken global variable.
void run_simulation(int time_quantum, int pipe_buff_size) {

  int running = 0; // PID=1 ( index starts at 0 ) always first process
  bool done = false;

  process_list[running].state = ST_RUNNING;
  do {

    // Get the system call of the currently executing process
    int syscall_index = process_list[running].next_syscall;

    printf("[%iusecs] [syscall %i] \t process_%i::", timetaken, syscall_index,
           running);

    // Decide how to process the particular system call
    switch (process_list[running].syscall_queue[syscall_index].syscall) {
    case SYS_EXIT: {
      printf("exit();\n");
      sim_exit(running);
      done = true;
      break;
    }
    case SYS_COMPUTE: {
      printf("compute();\n");
      sim_compute(running, time_quantum);
      break;
    }
    case SYS_FORK: {
      printf("fork();\n");
      sim_fork(running);
      break;
    }
    default: {
      break;
    }
    }

    // if there are still processes running, we need to select the next
    // one to compute.
    if (!queue_empty(rdy_head, rdy_tail)) {
      // select the process at the front of the queue
      running = dequeue(rdy_queue, &rdy_head);
      state_transition(running, ST_RUNNING);
      done = false;
    }

  } while (!done); // until simulation is complete
}

//  ---------------------------------------------------------------------

//  FUNCTIONS TO VALIDATE FIELDS IN EACH eventfile - NO NEED TO MODIFY
int check_PID(char word[], int lc) {
  int PID = atoi(word);

  if (PID <= 0 || PID > MAX_PROCESSES) {
    printf("invalid PID '%s', line %i\n", word, lc);
    exit(EXIT_FAILURE);
  }
  return PID;
}

int check_microseconds(char word[], int lc) {
  int usecs = atoi(word);

  if (usecs <= 0) {
    printf("invalid microseconds '%s', line %i\n", word, lc);
    exit(EXIT_FAILURE);
  }
  return usecs;
}

int check_descriptor(char word[], int lc) {
  int pd = atoi(word);

  if (pd < 0 || pd >= MAX_PIPE_DESCRIPTORS_PER_PROCESS) {
    printf("invalid pipe descriptor '%s', line %i\n", word, lc);
    exit(EXIT_FAILURE);
  }
  return pd;
}

int check_bytes(char word[], int lc) {
  int nbytes = atoi(word);

  if (nbytes <= 0) {
    printf("invalid number of bytes '%s', line %i\n", word, lc);
    exit(EXIT_FAILURE);
  }
  return nbytes;
}

//  parse_eventfile() READS AND VALIDATES THE FILE'S CONTENTS
//  YOU NEED TO STORE ITS VALUES INTO YOUR OWN DATA-STRUCTURES AND VARIABLES
void parse_eventfile(char program[], char eventfile[]) {
#define LINELEN 100
#define WORDLEN 20
#define CHAR_COMMENT '#'

  //  ATTEMPT TO OPEN OUR EVENTFILE, REPORTING AN ERROR IF WE CAN'T
  FILE *fp = fopen(eventfile, "r");

  if (fp == NULL) {
    printf("%s: unable to open '%s'\n", program, eventfile);
    exit(EXIT_FAILURE);
  }

  char line[LINELEN], words[4][WORDLEN];
  int lc = 0;

  //  READ EACH LINE FROM THE EVENTFILE, UNTIL WE REACH THE END-OF-FILE
  while (fgets(line, sizeof line, fp) != NULL) {
    ++lc;
    //    COMMENT LINES ARE SIMPLY SKIPPED
    if (line[0] == CHAR_COMMENT) {
      continue;
    }

    //  ATTEMPT TO BREAK EACH LINE INTO A NUMBER OF WORDS, USING sscanf()
    int nwords = sscanf(line, "%19s %19s %19s %19s", words[0], words[1],
                        words[2], words[3]);

    //  WE WILL SIMPLY IGNORE ANY LINE WITHOUT ANY WORDS
    if (nwords <= 0) {
      continue;
    }

    //  ENSURE THAT THIS LINE'S PID IS VALID
    int thisPID = check_PID(words[0], lc);

    //  IDENTIFY LINES RECORDING SYSTEM-CALLS AND THEIR OTHER VALUES
    //  THIS FUNCTION ONLY CHECKS INPUT;  YOU WILL NEED TO STORE THE VALUES
    //
    //  Storing values inside the command structure.

    int pid_index = thisPID - 1;
    int next_syscall = process_list[pid_index].next_syscall;

    if (nwords == 3 && strcmp(words[1], "compute") == 0) {

      process_list[pid_index].syscall_queue[next_syscall].syscall = SYS_COMPUTE;

      process_list[pid_index].syscall_queue[next_syscall].data.microseconds =
          check_microseconds(words[2], lc);

    } else if (nwords == 3 && strcmp(words[1], "sleep") == 0) {

      process_list[pid_index].syscall_queue[next_syscall].syscall = SYS_SLEEP;

      process_list[pid_index].syscall_queue[next_syscall].data.microseconds =
          check_microseconds(words[2], lc);

    } else if (nwords == 2 && strcmp(words[1], "exit") == 0) {

      process_list[pid_index].syscall_queue[next_syscall].syscall = SYS_EXIT;

    } else if (nwords == 3 && strcmp(words[1], "fork") == 0) {

      process_list[pid_index].syscall_queue[next_syscall].syscall = SYS_FORK;

      process_list[pid_index].syscall_queue[next_syscall].data.pid =
          check_PID(words[2], lc);

    } else if (nwords == 3 && strcmp(words[1], "wait") == 0) {
      process_list[pid_index].syscall_queue[next_syscall].syscall = SYS_WAIT;

      process_list[pid_index].syscall_queue[next_syscall].data.pid =
          check_PID(words[2], lc);
    } else if (nwords == 3 && strcmp(words[1], "pipe") == 0) {
      process_list[pid_index].syscall_queue[next_syscall].syscall = SYS_PIPE;

      process_list[pid_index].syscall_queue[next_syscall].data.descriptor =
          check_descriptor(words[2], lc);

    } else if (nwords == 4 && strcmp(words[1], "writepipe") == 0) {
      process_list[pid_index].syscall_queue[next_syscall].syscall =
          SYS_WRITEPIPE;

      process_list[pid_index]
          .syscall_queue[next_syscall]
          .data.pipe_info.descriptor = check_descriptor(words[2], lc);

      process_list[pid_index]
          .syscall_queue[next_syscall]
          .data.pipe_info.nbytes = check_bytes(words[3], lc);

    } else if (nwords == 4 && strcmp(words[1], "readpipe") == 0) {

      process_list[pid_index].syscall_queue[next_syscall].syscall =
          SYS_READPIPE;

      process_list[pid_index]
          .syscall_queue[next_syscall]
          .data.pipe_info.descriptor = check_descriptor(words[2], lc);

      process_list[pid_index]
          .syscall_queue[next_syscall]
          .data.pipe_info.nbytes = check_bytes(words[3], lc);
    }
    // UNRECOGNISED LINE
    else {
      printf("%s: line %i of '%s' is unrecognized\n", program, lc, eventfile);
      exit(EXIT_FAILURE);
    }
    ++process_list[pid_index]
          .next_syscall; // Make sure to always assign to a new location
  }
  fclose(fp);

  for (int pid_index = 0; pid_index < MAX_PROCESSES; pid_index++) {
    process_list[pid_index].next_syscall = 0;
  }

#undef LINELEN
#undef WORDLEN
#undef CHAR_COMMENT
}

//  ---------------------------------------------------------------------

//  CHECK THE COMMAND-LINE ARGUMENTS, CALL parse_eventfile(), RUN SIMULATION
int main(int argc, char *argv[]) {

  if (argc != 4) {
    fprintf(stderr, "[!] %s expected 3 arguments, got %i\n", argv[0], argc - 1);
    exit(EXIT_FAILURE);
  }

  int quant = atoi(argv[2]);
  // Checking if time quantum is valid
  if (quant < 1) {
    fprintf(stderr, "[!] %s time quantum must be > 0, got: %i\n", argv[0],
            quant);
    exit(EXIT_FAILURE);
  }

  int pipe_buff_size = atoi(argv[3]);
  // Checking that the buffer size is valid
  if (pipe_buff_size < 1) {
    fprintf(stderr, "[!] %s pipe buffer size must be > 0, got %i\n", argv[0],
            pipe_buff_size);
    exit(EXIT_FAILURE);
  } else {
    fprintf(stderr, "[#] need to check valid range of buffer size\n");
  }

  printf("[#] Initialising process list\n");
  initialise_process_list(); // initialising process list separately from
                             // parsing. Each function should be self-contained
  printf("[#] Parsing eventfile...\n");
  parse_eventfile(argv[0], argv[1]);

  printf("[#] Running simulation...\n");
  run_simulation(quant, pipe_buff_size);

  printf("timetaken %i\n", timetaken);
  return 0;
}
