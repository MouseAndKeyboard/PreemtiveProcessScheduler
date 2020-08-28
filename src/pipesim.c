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
enum action_t { Compute, Sleep, Exit, Fork, Wait, Pipe, Writepipe, Readpipe };

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

struct command_t {
  enum action_t action;
  int pid;
  union command_data_t data;
};

int timetaken = 0;
int num_commands = 0;
struct command_t command_queue[MAX_PROCESSES * MAX_SYSCALLS_PER_PROCESS];

// Uses the values in the command_queue to find and set
// the timetaken global variable.
void run_simulation(int time_quantum, int pipe_buff_size) {

  for (int cmd = 0; cmd < num_commands; cmd++) {
    switch (command_queue[cmd].action) {
    case Exit: {
      timetaken += USECS_TO_CHANGE_PROCESS_STATE;
      break;
    }
    default: {
      break;
    }
    }
  }
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

    //  COMMENT LINES ARE SIMPLY SKIPPED
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

    struct command_t cmd;
    cmd.pid = thisPID;

    //  IDENTIFY LINES RECORDING SYSTEM-CALLS AND THEIR OTHER VALUES
    //  THIS FUNCTION ONLY CHECKS INPUT;  YOU WILL NEED TO STORE THE VALUES
    //
    //  Storing values inside the command structure.
    if (nwords == 3 && strcmp(words[1], "compute") == 0) {
      cmd.action = Compute;
      cmd.data.microseconds = check_microseconds(words[2], lc);
    } else if (nwords == 3 && strcmp(words[1], "sleep") == 0) {
      cmd.data.microseconds = check_microseconds(words[2], lc);
      cmd.action = Sleep;
    } else if (nwords == 2 && strcmp(words[1], "exit") == 0) {
      cmd.action = Exit;
    } else if (nwords == 3 && strcmp(words[1], "fork") == 0) {
      cmd.data.pid = check_PID(words[2], lc);
      cmd.action = Fork;
    } else if (nwords == 3 && strcmp(words[1], "wait") == 0) {
      cmd.data.pid = check_PID(words[2], lc);
      cmd.action = Wait;
    } else if (nwords == 3 && strcmp(words[1], "pipe") == 0) {
      cmd.data.pid = check_descriptor(words[2], lc);
      cmd.action = Pipe;
    } else if (nwords == 4 && strcmp(words[1], "writepipe") == 0) {
      cmd.data.pipe_info.descriptor = check_descriptor(words[2], lc);
      cmd.data.pipe_info.nbytes = check_bytes(words[3], lc);
      cmd.action = Writepipe;
    } else if (nwords == 4 && strcmp(words[1], "readpipe") == 0) {
      cmd.data.pipe_info.descriptor = check_descriptor(words[2], lc);
      cmd.data.pipe_info.nbytes = check_bytes(words[3], lc);
      cmd.action = Readpipe;
    }
    // UNRECOGNISED LINE
    else {
      printf("%s: line %i of '%s' is unrecognized\n", program, lc, eventfile);
      exit(EXIT_FAILURE);
    }

    command_queue[lc] = cmd;

    // Line counts from zero
    ++lc;
  }
  fclose(fp);
  num_commands = lc;

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

  parse_eventfile(argv[0], argv[1]);
  run_simulation(quant, pipe_buff_size);

  printf("timetaken %i\n", timetaken);
  return 0;
}
