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
enum syscall_type {
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

struct pipe_transmission {
  int descriptor;
  int nbytes;
};

// Unions allow me to store different kinds of command data
// It allows code to remain readable without becoming overcomplicated
union syscall_data {
  int microseconds;
  int pid;
  int descriptor;
  struct pipe_transmission pipe_info;
};

enum process_state_type {
  ST_READY,
  ST_RUNNING,
  ST_SLEEPING,
  ST_WAITING,
  ST_WRITING,
  ST_READING,
  ST_UNASSIGNED
};

#define INVALID_DESC -1
enum pipe_end { PIPE_READ, PIPE_WRITE, PIPE_NONE };

int timetaken = 0;

struct {
  enum process_state_type state;
  int next_syscall;

  // syscall structure holds information from parsing the file
  struct syscall {
    enum syscall_type syscall;
    union syscall_data data;
  } syscall_queue[MAX_SYSCALLS_PER_PROCESS];

  struct pipe {
    enum pipe_end my_end;
    int other_pid;
    int contained_bytes;
  } pipe_details[MAX_PIPE_DESCRIPTORS_PER_PROCESS];

  int parent_pid;
  int waiting_for;

} process_list[MAX_PROCESSES];

#define INVALID_PID -1

void clear_process(int pid) {
  process_list[pid].next_syscall = 0;

  process_list[pid].state = ST_UNASSIGNED;

  // Initialise all syscalls to default value
  for (int call = 0; call < MAX_SYSCALLS_PER_PROCESS; call++) {
    process_list[pid].syscall_queue[call].syscall = SYS_UNASSIGNED;
  }

  for (int desc = 0; desc < MAX_PIPE_DESCRIPTORS_PER_PROCESS; desc++) {
    process_list[pid].pipe_details[desc].my_end = PIPE_NONE;
  }

  process_list[pid].waiting_for = INVALID_PID;
  process_list[pid].parent_pid = INVALID_PID;
}

void initialise_process_list(void) {
  for (int pid = 0; pid < MAX_PROCESSES; pid++) {
    clear_process(pid);
    for (int pipe = 0; pipe < MAX_PIPE_DESCRIPTORS_PER_PROCESS; pipe++) {

      process_list[pid].pipe_details[pipe].contained_bytes = 0;
      process_list[pid].pipe_details[pipe].my_end = PIPE_NONE;
      process_list[pid].pipe_details[pipe].other_pid = INVALID_PID;
    }
  }
}

struct queue {
  int head;
  int tail;
  int elements[MAX_PROCESSES];
};

// Helper functions for the queue
void queue_init(struct queue *q) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    q->elements[i] = 0;
  }
  q->head = 0;
  q->tail = 0;
}
bool queue_empty(struct queue *q) { return q->head == q->tail; }
int dequeue(struct queue *q) { return q->elements[(q->head)++]; }
void enqueue(struct queue *q, int value) { q->elements[(q->tail)++] = value; }

void state_transition(int pid, enum process_state_type new_state,
                      struct queue *rdy_queue);

/*
 * elapse_time
 * Simulates the passage of time.
 *
 * time_delta: Amount of time which has passed
 * rdy_queue: Ready queue structure to enable state transitions
 *
 * Returns:
 * True - A state transition occurred
 * False - No state change occurred
 */
bool elapse_time(int time_delta, struct queue *rdy_queue) {

  timetaken += time_delta;
  printf("[#] T: %i\n", timetaken);
  bool state_change = false;
  for (int proc = 0; proc < MAX_PROCESSES; proc++) {
    if (ST_SLEEPING == process_list[proc].state) {
      int current_call = process_list[proc].next_syscall;
      int remaining_usecs =
          process_list[proc].syscall_queue[current_call].data.microseconds;
      remaining_usecs -= time_delta;
      process_list[proc].syscall_queue[current_call].data.microseconds =
          remaining_usecs;

      // Sleeping time finished
      if (remaining_usecs <= 0) {
        ++process_list[proc].next_syscall;
        state_transition(proc, ST_READY, rdy_queue);
        state_change = true;
      }
    }
  }
  return state_change;
}

#define STATE_NAME_LEN 15

/*
 *  state_transition
 *  Changes the state of a particular process
 *
 *  pid: Process ID of the process which we're changing the state of
 *  new_state: Target state (what state we want pid to be in)
 *  rdy_queue: Ready queue for putting ready processes #include
 */
void state_transition(int pid, enum process_state_type new_state,
                      struct queue *rdy_queue) {

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
  case ST_SLEEPING: {
    snprintf(state, sizeof(state), "SLEEPING");
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
  case ST_WRITING: {
    snprintf(state, sizeof(state), "WRITING");
    break;
  }
  case ST_READING: {
    snprintf(state, sizeof(state), "READING");
    break;
  }
  default: {
    snprintf(state, sizeof(state), "!UNKNOWN!");
    break;
  }
  }

  printf("[#] Changing process_%i's state -> %s\n", pid, state);

  process_list[pid].state = ST_UNASSIGNED;

  elapse_time(USECS_TO_CHANGE_PROCESS_STATE, rdy_queue);

  // Actually do the allocation
  process_list[pid].state = new_state;

  if (ST_READY == new_state) {
    // make sure to add processes which are ready to the "ready" queue
    enqueue(rdy_queue, pid);
  }
}

/*
 * sim_exit
 * simulates the exit() syscall on a process
 *
 * pid: Process ID we're going to exit.
 * rdy_queue: Ready queue to allow for a state transition
 */
void sim_exit(int pid, struct queue *rdy_queue) {
  // need to check if the parent of this process
  // is waiting
  int parent_pid = process_list[pid].parent_pid;
  if (INVALID_PID != parent_pid) {

    if (ST_WAITING == process_list[parent_pid].state) {
      // need to check if parent is waiting for this particular child:
      printf("[#] parent of exited process: %i\n", parent_pid);
      printf("[#] parent waiting for: %i\n",
             process_list[parent_pid].waiting_for);
      if (process_list[parent_pid].waiting_for == pid) {
        process_list[parent_pid].waiting_for = INVALID_PID;
        state_transition(parent_pid, ST_READY, rdy_queue);
      }
    }
  }
  clear_process(pid);
  state_transition(pid, ST_UNASSIGNED, rdy_queue);
}

void sim_compute(int pid, int time_quantum, struct queue *rdy_queue) {
  int syscall_index = process_list[pid].next_syscall;

  int usecs = process_list[pid].syscall_queue[syscall_index].data.microseconds;

  if (usecs > time_quantum) {
    elapse_time(time_quantum, rdy_queue);
    usecs -= time_quantum;
  } else {
    elapse_time(usecs, rdy_queue);
    // We have finished computing
    usecs = 0;
    ++process_list[pid].next_syscall;
  }

  process_list[pid].syscall_queue[syscall_index].data.microseconds = usecs;
  state_transition(pid, ST_READY, rdy_queue);
}

void sim_fork(int parent_pid, struct queue *rdy_queue) {
  int syscall_index = process_list[parent_pid].next_syscall;
  // subtract 1 to get the process index from 0 (rather than from 1)
  int child_pid =
      process_list[parent_pid].syscall_queue[syscall_index].data.pid - 1;

  // In reality the child would inherit all the
  // state of the parent, but since there's no
  // state for these processes.

  // Connect to all pipes on the parent with no reading end
  for (int pipe = 0; pipe < MAX_PIPE_DESCRIPTORS_PER_PROCESS; pipe++) {
    int end = process_list[parent_pid].pipe_details[pipe].other_pid;
    enum pipe_end start = process_list[parent_pid].pipe_details[pipe].my_end;

    if (INVALID_PID == end && PIPE_WRITE == start) {
      process_list[parent_pid].pipe_details[pipe].other_pid = child_pid;
      process_list[child_pid].pipe_details[pipe].my_end = PIPE_READ;
      process_list[child_pid].pipe_details[pipe].other_pid = parent_pid;
    }
  }

  process_list[child_pid].parent_pid = parent_pid;

  ++process_list[parent_pid].next_syscall;

  state_transition(child_pid, ST_READY, rdy_queue);
  state_transition(parent_pid, ST_READY, rdy_queue);
}

void sim_sleep(int pid, struct queue *rdy_queue) {
  state_transition(pid, ST_SLEEPING, rdy_queue);
}

void sim_wait(int pid, struct queue *rdy_queue) {
  int syscall_index = process_list[pid].next_syscall;
  process_list[pid].waiting_for =
      process_list[pid].syscall_queue[syscall_index].data.pid - 1;

  state_transition(pid, ST_WAITING, rdy_queue);
  ++process_list[pid].next_syscall;
}

void sim_pipe(int pid, struct queue *rdy_queue) {

  int syscall_index = process_list[pid].next_syscall;
  // mimicing the call to pipe();
  int desc = process_list[pid].syscall_queue[syscall_index].data.descriptor;

  process_list[pid].pipe_details[desc].my_end = PIPE_WRITE;
  process_list[pid].pipe_details[desc].other_pid = INVALID_PID;
  process_list[pid].pipe_details[desc].contained_bytes = 0;

  printf("JUST MADE A PIPE FOR %i WITH DESC: %i\n", pid, desc);

  state_transition(pid, ST_READY, rdy_queue);
  ++process_list[pid].next_syscall;
}

void set_pipe(int pid, int pipedesc, int new_qty) {
  struct pipe pipe = process_list[pid].pipe_details[pipedesc];
  process_list[pid].pipe_details[pipedesc].contained_bytes = new_qty;
  int other = pipe.other_pid;
  process_list[pid].pipe_details[other].contained_bytes = new_qty;
}

void sim_write_pipe(int pid, int pipe_size, struct queue *rdy_queue) {

  int next_call = process_list[pid].next_syscall;
  int destination_desc =
      process_list[pid].syscall_queue[next_call].data.pipe_info.descriptor;
  int to_write =
      process_list[pid].syscall_queue[next_call].data.pipe_info.nbytes;

  struct pipe pipe = process_list[pid].pipe_details[destination_desc];
  int total_remaining = pipe_size - pipe.contained_bytes;

  printf("PIPE {%i} HAS %i BYTES and PID %i WANTS TO WRITE %i\n",
         destination_desc, pipe.contained_bytes, pid, to_write);

  if (to_write > total_remaining) {
    state_transition(pid, ST_WRITING, rdy_queue);
    to_write -= total_remaining;
    elapse_time(total_remaining, rdy_queue);
    set_pipe(pid, destination_desc, pipe_size);
    process_list[pid].syscall_queue[next_call].data.pipe_info.nbytes = to_write;
  } else {
    set_pipe(pid, destination_desc, pipe.contained_bytes + to_write);
    elapse_time(to_write, rdy_queue);
    process_list[pid].syscall_queue[next_call].data.pipe_info.nbytes = 0;
    state_transition(pid, ST_READY, rdy_queue);
    ++process_list[pid].next_syscall;
  }

  int bytes = process_list[pid].pipe_details[destination_desc].contained_bytes;

  printf("NOW: PIPE {%i} HAS %i BYTES after PID %i wrote %i\n",
         destination_desc, bytes, pid, to_write);

  // Wake up read ends of pipes
  if (bytes > 0) {
    for (int child_id = 0; child_id < MAX_PROCESSES; child_id++) {
      if (pid == process_list[child_id].parent_pid) {
        if (ST_READING == process_list[child_id].state) {
          state_transition(child_id, ST_READY, rdy_queue);
        }
      }
    }
  }
}

void sim_read_pipe(int pid, int pipe_size, struct queue *rdy_queue) {

  int next_call = process_list[pid].next_syscall;
  int destination_desc =
      process_list[pid].syscall_queue[next_call].data.pipe_info.descriptor;
  int to_read =
      process_list[pid].syscall_queue[next_call].data.pipe_info.nbytes;

  int parent = process_list[pid].parent_pid;

  struct pipe pipe = process_list[parent].pipe_details[destination_desc];

  printf("PIPE {%i} HAS %i BYTES and PID %i WANTS TO READ %i\n",
         destination_desc, pipe.contained_bytes, pid, to_read);

  int read_total;
  if (pipe.contained_bytes - to_read < 0) {

    state_transition(pid, ST_READING, rdy_queue);
    read_total = pipe.contained_bytes;
  } else {
    read_total = to_read;
    ++process_list[pid].next_syscall;
    state_transition(pid, ST_READY, rdy_queue);
  }
  elapse_time(read_total, rdy_queue);
  set_pipe(pid, destination_desc, pipe.contained_bytes - read_total);

  printf("NOW: PIPE {%i} HAS %i BYTES after PID %i READ %i\n", destination_desc,
         pipe.contained_bytes, pid, to_read);

  // wake up writing end of pipe
  if (read_total > 0) {
    int parent = process_list[pid].parent_pid;
    if (ST_WRITING == process_list[parent].state) {
      state_transition(parent, ST_READY, rdy_queue);
    }
  }
}

#define NO_RUNNING -1

// Uses the values in the command_queue to find and set
// the timetaken global variable.
void run_simulation(int time_quantum, int pipe_buff_size) {

  int running = 0; // PID=1 ( index starts at 0 ) always first process
  int count = 1;
  struct queue rdy_queue;
  queue_init(&rdy_queue);

  process_list[running].state = ST_RUNNING;
  do {
    if (NO_RUNNING != running) {
      // Get the system call of the currently executing process
      int syscall_index = process_list[running].next_syscall;

      printf("[%iusecs] [syscall %i] \t process_%i::", timetaken, syscall_index,
             running);

      // Decide how to process the particular system call
      switch (process_list[running].syscall_queue[syscall_index].syscall) {
      case SYS_EXIT: {
        printf("exit();\n");
        sim_exit(running, &rdy_queue);
        --count;
        break;
      }
      case SYS_COMPUTE: {
        printf("compute();\n");
        sim_compute(running, time_quantum, &rdy_queue);
        break;
      }
      case SYS_FORK: {
        printf("fork();\n");
        sim_fork(running, &rdy_queue);
        ++count;
        break;
      }
      case SYS_SLEEP: {
        printf("sleep();\n");
        sim_sleep(running, &rdy_queue);
        break;
      }
      case SYS_WAIT: {
        printf("wait();\n");
        sim_wait(running, &rdy_queue);
        break;
      }
      case SYS_PIPE: {
        printf("pipe();\n");
        sim_pipe(running, &rdy_queue);
        break;
      }
      case SYS_WRITEPIPE: {
        printf("writepipe();\n");
        sim_write_pipe(running, pipe_buff_size, &rdy_queue);
        break;
      }
      case SYS_READPIPE: {
        printf("readpipe();\n");
        sim_read_pipe(running, pipe_buff_size, &rdy_queue);
        break;
      }
      case SYS_UNASSIGNED: {
        fprintf(stderr, "[!] UNASSIGNED SYSTEM CALL process_%i (halting...)\n",
                running);
        exit(EXIT_FAILURE);
        break;
      }
      default: {
        fprintf(stderr,
                "[!] UNKNOWN SYSTEM CALL process_%i \"%i\" (halting...)\n",
                running,
                process_list[running].syscall_queue[syscall_index].syscall);
        exit(EXIT_FAILURE);
        break;
      }
      }
    }

    // if there are processes in the queue,
    // we need to select the next one to compute.
    if (!queue_empty(&rdy_queue)) {
      running = dequeue(&rdy_queue);
      state_transition(running, ST_RUNNING, &rdy_queue);
    } else {
      // Ready queue is empty, we should check
      // if all processes have been terminated
      for (int proc = 0; proc < MAX_PROCESSES; proc++) {
        if (ST_UNASSIGNED != process_list[proc].state) {
          if (elapse_time(1, &rdy_queue)) {
            running = dequeue(&rdy_queue);
            state_transition(running, ST_RUNNING, &rdy_queue);
          } else {
            running = NO_RUNNING;
          }
          break;
        }
      }
    }

    if (timetaken > 400)
      exit(1);

  } while (count); // until simulation is complete
}

//  ---------------------------------------------------------------------

//    FUNCTIONS TO VALIDATE FIELDS IN EACH eventfile - NO NEED TO MODIFY
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
    struct syscall *syscall =
        &process_list[pid_index].syscall_queue[next_syscall];

    if (nwords == 3 && strcmp(words[1], "compute") == 0) {
      syscall->syscall = SYS_COMPUTE;
      syscall->data.microseconds = check_microseconds(words[2], lc);

    } else if (nwords == 3 && strcmp(words[1], "sleep") == 0) {
      syscall->syscall = SYS_SLEEP;
      syscall->data.microseconds = check_microseconds(words[2], lc);

    } else if (nwords == 2 && strcmp(words[1], "exit") == 0) {
      syscall->syscall = SYS_EXIT;

    } else if (nwords == 3 && strcmp(words[1], "fork") == 0) {
      syscall->syscall = SYS_FORK;
      syscall->data.pid = check_PID(words[2], lc);

    } else if (nwords == 3 && strcmp(words[1], "wait") == 0) {
      syscall->syscall = SYS_WAIT;
      syscall->data.pid = check_PID(words[2], lc);

    } else if (nwords == 3 && strcmp(words[1], "pipe") == 0) {
      syscall->syscall = SYS_PIPE;
      syscall->data.descriptor = check_descriptor(words[2], lc);

    } else if (nwords == 4 && strcmp(words[1], "writepipe") == 0) {
      syscall->syscall = SYS_WRITEPIPE;
      syscall->data.pipe_info.descriptor = check_descriptor(words[2], lc);
      syscall->data.pipe_info.nbytes = check_bytes(words[3], lc);

    } else if (nwords == 4 && strcmp(words[1], "readpipe") == 0) {
      syscall->syscall = SYS_READPIPE;
      syscall->data.pipe_info.descriptor = check_descriptor(words[2], lc);
      syscall->data.pipe_info.nbytes = check_bytes(words[3], lc);

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
