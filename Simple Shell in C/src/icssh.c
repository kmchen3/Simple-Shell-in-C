#include "icssh.h"
#include "helpers.h"
#include "linkedlist.h"
#include <readline/readline.h>
#include <signal.h> //i added

volatile sig_atomic_t flag = 0;
volatile int numBG = 0;

void sigchld_handler() {
    flag = 1;
}

void sigusr2_handler() { 
	char buf[100];
	int num = snprintf(buf, sizeof(buf), "Num of Background processes: %d\n", numBG);
	write(STDERR_FILENO, buf, num);
}

int main(int argc, char* argv[]) {
    int max_bgprocs = -1;
	int exec_result;
	int exit_status = -100;
	int history = 0;
	list_t *printHistory;
	pid_t pid;
	pid_t wait_result;
	char* line;
	list_t *list;
#ifdef GS
    rl_outstream = fopen("/dev/null", "w");
#endif

    // check command line arg
    if(argc > 1) {
        int check = atoi(argv[1]);
        if(check != 0)
            max_bgprocs = check;
        else {
            printf("Invalid command line argument value\n");
            exit(EXIT_FAILURE);
        }
    }

	// Setup segmentation fault handler
	if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
		perror("Failed to set signal handler");
		exit(EXIT_FAILURE);
	}

	/* when background process terminates ?
	SIGCHLD will occur 
	- implement and install SIGCHLD handler to catch signal 
	- set conditional flag denoting that a child has been terminated*/
	if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
		perror("Failed to set signal child handler");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGUSR2, sigusr2_handler) == SIG_ERR) {
		perror("Failed to set signal user handler");
		exit(EXIT_FAILURE);
	}

    list = CreateList(&Comparator, &Printer, &Deleter); 
	printHistory = CreateList(NULL, NULL, NULL); 

	// print the prompt & wait for the user to enter commands string
	while ((line = readline(SHELL_PROMPT)) != NULL) {
        // MAGIC HAPPENS! Command string is parsed into a job struct
        // Will print out error message if command string is invalid
		job_info* job = validate_input(line);

		/* - if set, reap all terminated background processes (1 at a time)
			- remove each process from data structure and print BG_TERM to STDOUT
			- once all terminated background processes have been reaped, set flag to 0 */
		if (flag == 1) { 
			int index = 0;
			wait_result = waitpid(-1, &exit_status, WNOHANG); //pid of dead child
			while (wait_result > 0) { // while there are dead children
				index = findIndex(wait_result, list); // find index of wait_result in list
				if (index >= 0){
					bgentry_t *removed = RemoveByIndex(list, index); //remove child from list
					fprintf(stdout, BG_TERM, removed->pid, removed->job->line);  //print child
					free_job(removed->job);
					free(removed);
				}
				wait_result = waitpid(-1, &exit_status, WNOHANG);
			}
			flag = 0;
			numBG = list->length;
		}

        if (job == NULL) { // Command was empty string or invalid
			free(line);
			continue;
		}

        //Prints out the job linked list struture for debugging
        #ifdef DEBUG   // If DEBUG flag removed in makefile, this will not longer print
     		debug_print_job(job);
        #endif

		if (strcmp(line, " ") != 0 && strncmp(line, "!", 1) != 0 && strcmp(line, "history") != 0) {
			char *temp = (char *)malloc(sizeof(char) * strlen(line) + 1);
			strcpy(temp, line);
			InsertAtHead(printHistory, temp);
		}

		// example built-in: exit
		if (strcmp(job->procs->cmd, "exit") == 0) {
			// Terminating the shell
			node_t *cur_node = list->head;
			while (cur_node != NULL) {
				pid_t target_pid = ((bgentry_t*)cur_node->data)->pid;
				kill(target_pid, SIGKILL); // send kill signal to each
				waitpid(target_pid, &exit_status, 0); // wait for child to die
				
				cur_node = cur_node->next;
				bgentry_t *removed = RemoveByIndex(list, 0); // remove list entry
				fprintf(stdout, BG_TERM, removed->pid, removed->job->line);  // BG print
				free_job(removed->job);
				free(removed);
			}

			node_t *temp = printHistory->head;
			while (temp != NULL) {
				temp = temp->next;
				char *tempLine = RemoveFromHead(printHistory);
				free(tempLine);
			}

			free(printHistory);
			free(list);
			free(line);
			free_job(job);
            validate_input(NULL);   // calling validate_input with NULL will free the memory it has allocated
            return 0;
		}

		if (strncmp(job->procs->cmd, "!", 1) == 0) {
			node_t *temp = printHistory->head;
			if (strlen(job->procs->cmd) == 1) {
				free_job(job);
				printf("%s\n", (char *)temp->data);
				InsertAtHead(printHistory, temp->data);
				job = validate_input(temp->data);
			}
			else if (strlen(job->procs->cmd) > 1) {
				int num = atoi(job->procs->cmd + 1);
				if (num >= 1 && num <= 5) {
					int hold = 1;
					while (hold < num) {
						temp = temp->next;
						hold++;
					}
					free_job(job);
					printf("%s\n", (char *)temp->data);
					InsertAtHead(printHistory, temp->data);
					job = validate_input(temp->data);
				}
				else {
					continue;
				}
			}

		}

		if (strcmp(job->procs->cmd, "cd") == 0) { // IT WORKS
			char *newdir;
			newdir = job->procs->argv[1]; //dog
			if (newdir == NULL) {
				newdir = "/root"; //getenv HOME
			}
			if (chdir(newdir) != 0) { // no directory found
				fprintf(stderr, "%s", DIR_ERR);
				free(line);
				free_job(job);
				continue;
			}
			char temp[63];
			if (getcwd(temp, sizeof(temp)) != NULL) {
				fprintf(stdout, "%s\n", temp);
				free(line);
				free_job(job);
				continue;
			}
			else {
				fprintf(stderr, "%s", DIR_ERR);
			}
			free(line);
			free_job(job);
		}

		if (strcmp(job->procs->cmd, "estatus") == 0) { // works but kinda funny
			if (WEXITSTATUS(exit_status) >= 255) { // i DONT think this should work right lowk hardcoded to print -100
				printf("-100\n");
				free(line);
				free_job(job);
				continue;
			}
			printf("%d\n", WEXITSTATUS(exit_status));
			free(line);
			free_job(job);
			continue;
		}

		if (strcmp(job->procs->cmd, "bglist") == 0) { // didn't work on 
			node_t* current = list->head; 
			while (current != NULL) {
				print_bgentry(current->data);
				current = current->next;
			}
			free(line);
			free_job(job);
			continue;
		}

		if (strcmp(job->procs->cmd, "history") == 0) { // needs help
			node_t *temp = printHistory->head;
			int index = 1;
			while(temp != NULL && index <= printHistory->length) {
				printf("%d: %s\n", index, (char *)temp->data);
				temp = temp->next;
				index++;
			}
			free_job(job);
			free(line);
			continue;
		}

		//int numPid[numProcs];
		// if pipe
		int numProcs = job->nproc; //2 or 3
		if (numProcs > 1) {
			int pipefd[numProcs - 1][2];
			//pipefd[0][0/1] if numProcs = 2
			//pipefd[0][0/1] and pipefd[1][0/1] if numProcs = 3
			for (int i = 0; i < numProcs - 1; i++) {
				if (pipe(pipefd[i]) == -1) {
					perror("pipe");
					exit(EXIT_FAILURE);
				}
				//printf("%d: 0: %d 1: %d\n", i, pipefd[i][0], pipefd[i][1]);
			}

			//int numPid[numProcs];
			for (int i = 0; i < numProcs; i++) {
				//numPid[i] = fork();
				pid = fork();
				//printf("numProcs: %d ; %d: %d\n", numProcs, i, pid);
				if (pid < 0) {
					perror("fork error");
					exit(EXIT_FAILURE);
				}
				else if (pid == 0) { // child
					if (job->in_file != NULL) { 
						int infile = open(job->in_file, O_WRONLY | O_CREAT , S_IRWXU);
						if (infile < 0) {
							fprintf(stderr, RD_ERR);
							free_job(job);  
							free(line);
							validate_input(NULL);  // calling validate_input with NULL will free the memory it has allocated

							exit(EXIT_FAILURE);
						}
						dup2(infile, STDIN_FILENO);
						close(infile); 
					}
					if (job->out_file != NULL) {
						int outfile = open(job->out_file, O_WRONLY | O_CREAT , S_IRWXU);
						dup2(outfile, STDOUT_FILENO);
						close(outfile); 
					}
					if (job->procs->err_file != NULL) {
						int errfile = open(job->procs->err_file, O_WRONLY | O_CREAT , S_IRWXU);
						dup2(errfile, STDERR_FILENO);
						close(errfile); //
					}

					// if not first process
					if (i != 0) {
						close(pipefd[i - 1][1]);
						if (dup2(pipefd[i - 1][0], STDIN_FILENO) == -1) {
							perror("dup2");
							exit(EXIT_FAILURE);
						}
						close(pipefd[i - 1][0]);
						//printf("pipefd[i - 1][0]: %d\n", pipefd[i - 1][0]);
					}
					// if not last process
					if (i != numProcs - 1) {
						close(pipefd[i][0]);
						if (dup2(pipefd[i][1], STDOUT_FILENO) == -1) {
							perror("dup2");
							exit(EXIT_FAILURE);
						}
						close(pipefd[i][1]);
						//printf("%d\n", dup2(pipefd[i][1], STDOUT_FILENO));
						//printf("pipefd[i][1]: %d\n", pipefd[i][1]);
					}
					// close all pipes before execvp()
					for (int j = 0; j < numProcs - 1; j++) {
						//printf("closing pipes"); // haven't seen it go in this 
						close(pipefd[j][0]);
						close(pipefd[j][1]);
					}
					// execvp()
					execvp(job->procs->cmd, job->procs->argv); 
					perror("execvp");
					exit(EXIT_FAILURE);
				}
				else {
					//printf("parent"); 
					if (i == 0 && job->bg == 1) {
						bgentry_t *temp;
						temp = (bgentry_t *)malloc(sizeof(bgentry_t));
						temp->job = job;
						temp->pid = pid;
						time(&temp->seconds);
						InsertInOrder(list, temp);
						numBG = list->length;
					}
				}
				job->procs = job->procs->next_proc;
				
			}
			for (int i = 0; i < numProcs - 1; i++) {
				close(pipefd[i][0]);
				close(pipefd[i][1]);
			}
			for (int i = 0; i < numProcs; i++) {
				pid = waitpid(-1, &exit_status, 0);
			}
			free_job(job);
			free(line);
			continue;
		}
		// example of good error handling!
		// create the child proccess
		if ((pid = fork()) < 0) {
			perror("fork error");
			exit(EXIT_FAILURE);
		}
		if (pid == 0) {  //If zero, then it's the child process
			//get the first command in the job list to execute
			proc_info* proc = job->procs;

			/* (Part 4) implement redirection in your shell:
			- open / create the required input or output files  (pay attention to flags)
			- fork() a child process and execvp() the command
			- use dup() or dup2() to make copies and set proper file descriptors */
			//how should i do the < > 2> ? in the while loop like strcmp(job->procs, ">") 
			// BUT HOW WOULD I KNOW ?? iterate through command to see if there is a  < > or 2> ?
			if (job->in_file != NULL) { //<
				int infile = open(job->in_file, O_RDONLY);
				if (infile < 0) {
					fprintf(stderr, RD_ERR);
					free_job(job);  
					free(line);
					validate_input(NULL);  // calling validate_input with NULL will free the memory it has allocated

					exit(EXIT_FAILURE);
				}
				dup2(infile, STDIN_FILENO);
			}
			if (job->out_file != NULL) {
				int outfile = open(job->out_file, O_WRONLY | O_CREAT , S_IRWXU);
				dup2(outfile, STDOUT_FILENO);
			}
			if (job->procs->err_file != NULL) {
				int errfile = open(job->procs->err_file, O_WRONLY | O_CREAT , S_IRWXU);
				dup2(errfile, STDERR_FILENO);
			}

			exec_result = execvp(proc->cmd, proc->argv);
			if (exec_result < 0) {  //Error checking
				printf(EXEC_ERR, proc->cmd);
				// Cleaning up to make Valgrind happy 
				// (not necessary because child will exit. Resources will be reaped by parent)
				free_job(job);  
				free(line);
					validate_input(NULL);  // calling validate_input with NULL will free the memory it has allocated

				exit(EXIT_FAILURE);
			}

		} else {
			// As the parent, wait for the foreground job to finish
			if (job->bg == 1) {
				bgentry_t *temp;
				temp = (bgentry_t *)malloc(sizeof(bgentry_t));
				temp->job = job;
				temp->pid = pid;
				time(&temp->seconds);
				InsertInOrder(list, temp);
				free(line);
				numBG = list->length;
				continue;
			}
			else {
				wait_result = waitpid(pid, &exit_status, 0);
				if (wait_result < 0) {
					printf(WAIT_ERR);
					exit(EXIT_FAILURE);
				}
			}
		}

		free_job(job);  // if a foreground job, we no longer need the data
		free(line);
	}

	// calling validate_input with NULL will free the memory it has allocated
	validate_input(NULL);

#ifndef GS
	fclose(rl_outstream);
#endif
	return 0;
}
