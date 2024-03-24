# Unix Shell Implementation

The shell allows users to execute simple commands such as: 'exit', 'cd', 'bglist' (background list), 'estatus', and 'history'.

Features:
- signal handling (SIGCHLD and SIGUSR2)
- executing built-in and external commands
- redirection of input (stdin), output (stdout), and error (stderr) streams
  - using '<', '>', '2>', '|'
- identifying background processes by appending '&' to the command
- provides the history of commands entered by the user using the '!' command followed by the command number
- error handling for failed forks and executions
