#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <wait.h>
#include <errno.h>
#include <ctype.h>

#define BUF_SIZE 256

/* define the struct of single command */
typedef struct command {
    char *str;                  // string of command line
    char **argv;                // array of arguments 
    int argc;                   // number of arguments 
    FILE *out;                  // stream of ouput file
    pid_t pid;                  // id of child process which in charge of this cmd.
} cmd_t;


/* global variables */
int is_interact_mode = 0;
int is_batch_mode = 0;
char *paths[BUF_SIZE] = {"/bin", NULL};

/* function prototype */
void print_error();
int search_path(char *, char *);
void replace_multi_with_single(char *, char);
char *trim(char *);

int parse_command(cmd_t *);         // parse the input string of command line.
int redirect(cmd_t *);              // redirect if has sign ">".
void execute_command(cmd_t *);      // execute built-in command and other command.


/*
 * a simple shell - wish
 */
int main(int argc, char **argv) {
    
    FILE *input_fp;             // input file stream
    char *line = NULL;          // input line
    size_t len = 0;             //
    ssize_t nread;              // the number of characters in line

    if (argc > 2) {
        print_error();
        exit(1);
    } else if (argc == 2) {
        if ( (input_fp = fopen(argv[1], "r")) == NULL) {
            print_error();
            exit(1);
        }
        is_batch_mode = 1;      // adjust to batch mode
    } else if (argc == 1) {
        input_fp = stdin;
        is_interact_mode = 1;   // adjust to interactive mode
    }
   
    // indefinate loop
    while (1) {

        if (is_interact_mode) {
            printf("wish> ");
        } 
        
        // get command line from input 
        if ( (nread = getline(&line, &len, input_fp)) > 0) { 
            // replace '\n' with '\0'
            if (line[nread - 1] == '\n') line[nread - 1] = '\0';

            // split line into command struct array.
            cmd_t cmds[BUF_SIZE];
            char *command;
            int cmd_num = 0;
            while ((command = strsep(&line, "&")) != NULL) {
                cmds[cmd_num++].str = command;
            }
            
            // parse and execute each command
            for (int i = 0; i < cmd_num; i++) {
                char *cmd_argv[BUF_SIZE];
                char cmd_str[BUF_SIZE];
                strcpy(cmd_str, cmds[i].str);

                // initial
                cmds[i].str = cmd_str;
                cmds[i].argc = 0;
                cmds[i].argv = cmd_argv;
                cmds[i].out = stdout;
                cmds[i].pid = getpid();

                // parse input command line 
                if (parse_command(&cmds[i]) == -1) {    // parse error 
                    continue;
                }

                // execute command
                execute_command(&cmds[i]);
            }
                
            // parent process wait for child processes
            for (int i = 0; i < cmd_num; i++) {
                if (cmds[i].pid != getpid())    // not built-in command
                    waitpid(cmds[i].pid, NULL, 0);
            }

            // make sure get next command line.
            line = NULL;
            len = 0;
        } 
        else if (feof(input_fp)) {        // EOF
            exit(0);
        }
    }
}



void print_error() {
    char error_message[BUF_SIZE] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

int search_path(char pathname[], char *first_args) {
    int i = 0;
    while (paths[i]) {
        sprintf(pathname, "%s/%s", paths[i], first_args);
        if (access(pathname, X_OK) == 0) {
            return 0;
        }
        i++;
    }
    return -1;
}

void replace_multi_with_single(char *str, char c) {
    char *dest = str;
    while (*str != '\0') {
        while (*str == c && *(str + 1) == c) {
            str++;
        } 
        *dest++ = *str++;
    }
    *dest = '\0';
}

char *trim(char *str) {
    char *start = str;
    while (isspace(*start)) {
        start++;
    }
    char *end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) {
        end--;
    }
    *(++end) = '\0';
    return start;
}

int parse_command(cmd_t *cmd) {
    // trim the whitespace before and after the command line string.
    cmd->str = trim(cmd->str);
    if (*cmd->str == '\0') return -1;
    
    // replace multiply whitespace and newline with single.
    replace_multi_with_single(cmd->str, ' ');
    replace_multi_with_single(cmd->str, '\t');

    // try to find redirection sign.
    int has_redirect = 0;
    char *command_line;
    command_line = strdup(cmd->str);

    if (strstr(command_line, ">") != NULL) has_redirect = 1;
    char *command = strsep(&command_line, ">"); 

    // bad redirection : 1.no output file after ">". 2.no program for redirect
    if (has_redirect) {
        if (command_line == NULL || *command == '\0') {
            print_error();
            return -1;
        }
    }

    if (command_line != NULL) {
        // more than one redirection sign ">"
        if (strstr(command_line, ">") != NULL) {
            print_error();
            return -1;
        }

        command_line = trim(command_line);

        char *output_pathname = strsep(&command_line, " ");
        // more than one output file
        if (command_line != NULL) {
            print_error();
            return -1;
        }
        if ((cmd->out = fopen(output_pathname, "w")) == NULL) {
            print_error();
            return -1;
        }
    }

    command = trim(command);
    
    // construct the arguments array.
    while (command != NULL) {
        cmd->argv[cmd->argc++] = strsep(&command, " \t"); 
    } 
    cmd->argv[cmd->argc] = NULL;

    return 0;
}

int redirect(cmd_t *cmd) {
    int out_fileno;
    if ( (out_fileno = fileno(cmd->out)) == -1) {
        print_error();
        return -1;
    } 
    if (out_fileno != STDOUT_FILENO) {
        if (dup2(out_fileno, STDOUT_FILENO) == -1) {
            print_error();
            return -1;
        }
        if (dup2(out_fileno, STDERR_FILENO) == -1) {
            print_error();
            return -1;
        }
    }
    return 0;
}

void execute_command(cmd_t *cmd) {
   
    // if built-in command
    // exit.
    if (strcmp(cmd->argv[0], "exit") == 0) {
        if (cmd->argc - 1 > 0) {
            print_error();
        }
        exit(0);
    }
    // path.
    else if (strcmp(cmd->argv[0], "path") == 0) {
        paths[0] = NULL;
        for (int i = 1; i < cmd->argc; i++) {
            paths[i - 1] = strdup(cmd->argv[i]);
        }
        paths[cmd->argc - 1] = NULL;
    }
    // cd.
    else if (strcmp(cmd->argv[0], "cd") == 0) {
        if (cmd->argc - 1 == 1) {
            if (chdir(cmd->argv[1]) != 0) {
                print_error();
            }
        } else {
            print_error();
        }
    } 
    // if other command.
    else {
        char pathname[BUF_SIZE];
        if (search_path(pathname, cmd->argv[0]) != 0) {
            print_error();
        } else {
            pid_t pid;
            pid = fork(); 
            cmd->pid = pid;

            // child process
            if (pid == 0) {
                // redirection 
                redirect(cmd);

                execv(pathname, cmd->argv);
                print_error();
            } 
            // parent process
            else if (pid  > 0) {
                /*wait(NULL);*/
            }
            else {
                print_error();
            }
        }
    }
}
