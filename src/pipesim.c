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
union command_data_t {
  int microseconds;
  int pid;
  int descriptor;
  struct pipe_transmission_t pipe_info;
};

enum process_state_t { ST_READY, ST_RUNNING, ST_WAITING, ST_UNASSIGNED };

int timetaken = 0;
struct {
  enum process_state_t state;
  int next_instruction;

  struct {
    enum syscall_t syscall;
    union command_data_t data;
  } instruction_queue[MAX_SYSCALLS_PER_PROCESS];

} process_list[MAX_PROCESSES];

void clear_process(int pid) {
  process_list[pid].next_instruction = 0;

  // Initialise all instructions to default value
  for (int call = 0; call < MAX_SYSCALLS_PER_PROCESS; call++) {
    process_list[pid].instruction_queue[call].syscall = SYS_UNASSIGNED;
  }
}

void initialise_process_list() {
  for (int pid = 0; pid < MAX_PROCESSES; pid++) {
    clear_process(pid);
  }
}

void state_transition(int pid, enum process_state_t new_state) {
  process_list[pid].state = new_state;
  timetaken += USECS_PER_BYTE_TRANSFERED;
}

void sim_exit(int pid) {
  clear_process(pid);
  state_transition(pid, ST_UNASSIGNED);
}

// Uses the values in the command_queue to find and set
// the timetaken global variable.
void run_simulation(int time_quantum, int pipe_buff_size) {

  int running = 0; // PID=1 ( index starts at 0 ) always first process
  process_list[running].state = ST_RUNNING;

  do {
    int syscall_idex = process_list[running].next_instruction;
    switch (process_list[running].instruction_queue[syscall_idex].syscall) {
    case SYS_EXIT: {
      sim_exit(running);
      // must find new process to run
      break;
    }
    default: {
      break;
    }
    }

    for (int pid = 0; pid < MAX_PROCESSES; pid++) {
      if (process_list[pid].state == ST_READY) {
      }
    }

  } while (true); // until simulation is complete
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
    int next_instruction = process_list[pid_index].next_instruction;

    if (nwords == 3 && strcmp(words[1], "compute") == 0) {

      process_list[pid_index].instruction_queue[next_instruction].syscall =
          SYS_COMPUTE;

      process_list[pid_index]
          .instruction_queue[next_instruction]
          .data.microseconds = check_microseconds(words[2], lc);

    } else if (nwords == 3 && strcmp(words[1], "sleep") == 0) {

      process_list[pid_index].instruction_queue[next_instruction].syscall =
          SYS_SLEEP;

      process_list[pid_index]
          .instruction_queue[next_instruction]
          .data.microseconds = check_microseconds(words[2], lc);

    } else if (nwords == 2 && strcmp(words[1], "exit") == 0) {

      process_list[pid_index].instruction_queue[next_instruction].syscall =
          SYS_EXIT;

    } else if (nwords == 3 && strcmp(words[1], "fork") == 0) {

      process_list[pid_index].instruction_queue[next_instruction].syscall =
          SYS_FORK;

      process_list[pid_index].instruction_queue[next_instruction].data.pid =
          check_PID(words[2], lc);

    } else if (nwords == 3 && strcmp(words[1], "wait") == 0) {
      process_list[pid_index].instruction_queue[next_instruction].syscall =
          SYS_WAIT;

      process_list[pid_index].instruction_queue[next_instruction].data.pid =
          check_PID(words[2], lc);
    } else if (nwords == 3 && strcmp(words[1], "pipe") == 0) {
      process_list[pid_index].instruction_queue[next_instruction].syscall =
          SYS_PIPE;

      process_list[pid_index]
          .instruction_queue[next_instruction]
          .data.descriptor = check_descriptor(words[2], lc);

    } else if (nwords == 4 && strcmp(words[1], "writepipe") == 0) {
      process_list[pid_index].instruction_queue[next_instruction].syscall =
          SYS_WRITEPIPE;

      process_list[pid_index]
          .instruction_queue[next_instruction]
          .data.pipe_info.descriptor = check_descriptor(words[2], lc);

      process_list[pid_index]
          .instruction_queue[next_instruction]
          .data.pipe_info.nbytes = check_bytes(words[3], lc);

    } else if (nwords == 4 && strcmp(words[1], "readpipe") == 0) {

      process_list[pid_index].instruction_queue[next_instruction].syscall =
          SYS_READPIPE;

      process_list[pid_index]
          .instruction_queue[next_instruction]
          .data.pipe_info.descriptor = check_descriptor(words[2], lc);

      process_list[pid_index]
          .instruction_queue[next_instruction]
          .data.pipe_info.nbytes = check_bytes(words[3], lc);
    }
    // UNRECOGNISED LINE
    else {
      printf("%s: line %i of '%s' is unrecognized\n", program, lc, eventfile);
      exit(EXIT_FAILURE);
    }
    ++process_list[pid_index]
          .next_instruction; // Make sure to always assign to a new location
  }
  fclose(fp);

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

  initialise_process_list(); // initialising process list separately from
                             // parsing. Each function should be self-contained
  parse_eventfile(argv[0], argv[1]);
  run_simulation(quant, pipe_buff_size);

  printf("timetaken %i\n", timetaken);
  return 0;
}
