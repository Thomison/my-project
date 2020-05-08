# UNIX shell

## Overiew

In this assignment, you will implement a command line interpreter (CLI) or, as it is more commonly known, a shell. The shell should operate in this basic way: when you type in a command (in response to its prompt), the shell creates a child process that executes the command you entered and then prompts for more user input when it has finished.

a simple shell implemention in c.

features:
- Paths: the user must specify a path variable to describe the set of directories to search for executables.
- Built-in commands:
- Redirection:
- Parrellel commands: when user typed in several commands link by `&`, the shell
    should creates different child process to handle each command only if it's
    not built-in command.
