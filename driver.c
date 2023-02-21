#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>

// Macros for the maximum file name length and path length
#define FILE_NAME_MAX 255
#define PATH_LEN_MAX 4095

// A flag for foreground-only mode
static volatile bool fg_only_mode = false;

// struct command
// Holds command data
struct command {
	char* cmd;                  // The command itself
	int argc;                   // The number of arguments (counting the command)
	char* argv[512];            // The arguments (including the command)
	char* i_file, * o_file;     // The input and output files for I/O redirection
	bool background;            // Whether the command should be a background process
};

// free_command
// Frees heap memory allocated for a command struct
// Parameters: a pointer to a command struct on the heap
// Returns: none
void free_command(struct command* cmd) {
    // Free the command
	free(cmd->cmd);

    // Free the arguments
	for (int i = 0; i < cmd->argc; i++) {
		free(cmd->argv[i]);
	}

    // Free the I/O files
	free(cmd->i_file);
	free(cmd->o_file);

    // Free the command struct
	free(cmd);
	return;
}

// print_command
// Prints the data in a command struct; only used for debugging
// Parameters: the command struct to be printed
// Returns: none
void print_command(struct command* cmd) {
	printf("\n");

    // Print a null pointer if the passed command is NULL
	if (cmd == NULL) {
		printf("%p\n\n", cmd);
		return;
	}

    // Print the command data
	printf("cmd: %s\n", cmd->cmd);
	printf("argc: %d\nargv[]: ", cmd->argc);
	for (int i = 0; i < cmd->argc; i++) {
		printf("%s (%d chars), ", cmd->argv[i], strlen(cmd->argv[i]) + 1);
	}
	printf("\ni_file: %s\n", cmd->i_file);
	printf("o_file: %s\n", cmd->o_file);
	printf("background: %d\n\n", cmd->background);
}

// delete_int
// Searches for and deletes an integer in an integer list, shifting other entries to fill the gap
// Parameters: i, the integer to delete; i_list, a pointer to the integer list to delete from;
//             n, a pointer to the number of elements in the integer list
// Returns: a pointer to the integer list
int* delete_int(int i, int* i_list, int* n) {
	// j will mark the index of i
    // Initialize j to -1 to indicate that i has not been found yet
    int j = -1;
    // Loop through i_list
	for (int k = 0; k < *n; k++) {
		// Check if i has been found yet
        if (j != -1) {
            // At this point, j == k - 1
            // Copy the current element (k) into the previous element (j)
            // This erases i from the list and shifts all following elements to fill the gap
			i_list[j] = i_list[k];
            // Move j forward to ensure j == k - 1 on the next loop
			j++;
		} else if (i_list[k] == i) {
            // i was found; set j to the current index
			j = k;
		}
	}

    // Reduce n and clear the space at the end
	(*n)--;
	i_list[*n] = 0;
    // Return the list
	return i_list;
}

// int_to_str
// Converts an integer into a string
// Parameters: the integer to be converted and a buffer to receive the output; the buffer is assumed to be large enough for the number
// Returns: the string form of the integer
char* int_to_str(int n, char* buffer) {
	// i is the index of the first digit of the number
	int i = 0;
	// If the integer is negative, add a negative sign and step i to the next index
	if (n < 0) buffer[i++] = "-";
	
	// j is the index of the last digit of the number; default j to zero
	int j = 0;
	// To avoid domain errors, check that the integer is greater than zero before calculating the number of digits in it
	if (n > 0) j = (int) log10(n) + i;
	
	// Add a null-terminator to the string
	buffer[j] = "\0";

	// Loop through each digit of the integer from the right to left
	while (j >= i) {
		// Get the rightmost digit
		int digit = n % 10;
		// Increment the integer to the next digit
		n /= 10;
		// Convert the current digit into a character and add it to the string
		buffer[j--] = (char) (digit + 48);
	}

	// Return the string of the number
	return buffer;
}

// expand_sh_vars
// Replaces all occurrences of $$ with the current pid in a given string
// Parameters: a 2049-char string to perform the substitution on
// Returns: a pointer to the modified string
char** expand_sh_vars (char** cmd_buf) {
    // A pointer to the end of the string
    char* buf_end = *cmd_buf + 2048;

    // Find the first occurrence of $$
	char* ptr = strstr(*cmd_buf, "$$");
	// Loop until no more $$ are found
    while (ptr != NULL) {
        // Save a copy of the rest of cmd_buf after the current $$
		char temp[2049];
		strncpy(temp, ptr + 2, 2048);

        // Get the current pid and convert it to a string
		int curr_pid = getpid();
		int digits = ceil(log10(curr_pid)) + 1;     // A calculation of the number of digits of the pid
		char digit_str[digits + 1];                 // A buffer to receive the pid as a string
		sprintf(digit_str, "%d", curr_pid);         // Print to the pid buffer from the integer pid

        // Copy the string pid into cmd_buf up to the end
        // buf_end - ptr - 1 is the number of chars between ptr and the end of cmd_buf
		strncpy(ptr, digit_str, buf_end - ptr - 1);
		
        // Check whether the last statement ran into the end of cmd_buf
		if (ptr + digits - 1 < buf_end) {
            // Copy the rest of cmd_buf after the pid
            // buf_end - (ptr + digits - 1) is the number of chars 
            // between the end of the pid string and the end of cmd_buf
			strncpy(ptr + digits - 1, temp, buf_end - (ptr + digits - 1));
		}
		
        // Find the next occurrence of $$
		ptr = strstr(*cmd_buf, "$$");
	}
    
    // Return a pointer to the modified string
	return cmd_buf;
}

// parse_cmd_args
// Reads space-separated arguments from a string and adds them to a command struct
// Parameters: a pointer to a command struct to read into and a pointer to a string to read from
// Returns: the modified command struct
struct command* parse_cmd_args(struct command* cmd, char** cmd_buf) {
	// Char pointers for use with strtok_r()
    char* token, * saveptr;
	
    // Read the first space-separated token (the command)
    token = strtok_r(*cmd_buf, " ", &saveptr);
    // Allocate a string for the command and copy it from the token
	cmd->cmd = (char*) malloc(sizeof(char) * (strlen(token) + 1));
	strncpy(cmd->cmd, token, strlen(token) + 1);
	
    // Allocate a string for the first argument (the command) and copy it from the command string
	cmd->argv[0] = (char*) malloc(sizeof(char) * (strlen(cmd->cmd) + 1));
	strncpy(cmd->argv[0], cmd->cmd, strlen(cmd->cmd) + 1);

    // Read the next space-separated token
	token = strtok_r(NULL, " ", &saveptr);
    // Start counting arguments
	cmd->argc = 1;
	while (token != NULL) {
        // Allocate space for each argument and copy it from the current token
		cmd->argv[cmd->argc] = (char*) malloc(sizeof(char) * (strlen(token) + 1));
		strncpy(cmd->argv[cmd->argc], token, strlen(token) + 1);
        // Increment the argument counter
		cmd->argc++;

        // Find the next space-separated token
		token = strtok_r(NULL, " ", &saveptr);
	}

    // Check for an & argument at the end
	if (strcmp(cmd->argv[cmd->argc - 1], "&") == 0) {
		// Register the command as a background process
        cmd->background = true;

        // Remove the & argument so it won't be passed to exec()
		free(cmd->argv[cmd->argc - 1]);
		cmd->argv[cmd->argc - 1] = NULL;
		cmd->argc--;
	} else {
        // Otherwise, register the command as a foreground process
		cmd->background = false;
	}

    // Return the pointer to the command struct
    return cmd;
}

// parse_cmd_io_files
// Checks a command struct for I/O redirect arguments and
// registers them separately from other arguments
// Parameters: a pointer to a command struct
// Returns: the modified command struct
struct command* parse_cmd_io_files(struct command* cmd) {
	// Set the I/O files to NULL by default
    cmd->i_file = NULL;
	cmd->o_file = NULL;

    // Count the number of arguments to remove (assuming >, <, and the file names should be removed)
	int args_removed = 0;
    // Loop from the first non-command argument to the penultimate argument
    // (The last argument cannot be a valid I/O redirect sequence)
	for (int i = 1; i < cmd->argc - 1; i++) {
		// Alias the current argument for clarity
        char* arg = cmd->argv[i];

        // Check whether the current argument is the start of an I/O redirect sequence
		if (strcmp(arg, "<") == 0) {
            // Assume the next argument is a file name; calculate its length up to the file name maximum
			int file_len = strlen(cmd->argv[i + 1]);
			if (file_len > FILE_NAME_MAX) file_len = FILE_NAME_MAX;

            // Copy the next argument into the input file field of the command struct
			cmd->i_file = (char*) malloc(sizeof(char) * (file_len + 1));
			strncpy(cmd->i_file, cmd->argv[i + 1], file_len + 1);
			
            // Free the current argument (<) and the next one (the file name)
            // This prevents exec() from taking the I/O redirect sequence as an argument
			free(cmd->argv[i]);
			cmd->argv[i] = NULL;
			free(cmd->argv[i + 1]);
			cmd->argv[i + 1] = NULL;
			i++;
            // Count two arguments to be removed from the argument count at the end
			args_removed += 2;
		} else if (strcmp(arg, ">") == 0) {
            // Assume the next argument is a file name; calculate its length up to the file name maximum
			int file_len = strlen(cmd->argv[i + 1]);
			if (file_len > FILE_NAME_MAX) file_len = FILE_NAME_MAX;

            // Copy the next argument into the output file field of the command struct
			cmd->o_file = (char*) malloc(sizeof(char) * (file_len + 1));
			strncpy(cmd->o_file, cmd->argv[i + 1], file_len + 1);
			
            // Free the current argument (>) and the next one (the file name)
            // This prevents exec() from taking the I/O redirect sequence as an argument
			free(cmd->argv[i]);
			cmd->argv[i] = NULL;
			free(cmd->argv[i + 1]);
			cmd->argv[i + 1] = NULL;
			i++;
            // Count two arguments to be removed from the argument count at the end
			args_removed += 2;
		}
	}
	
    // If the command is for a background process and no I/O redirect is given, default to /dev/null
	if (cmd->background) {
		if (cmd->i_file == NULL) {
			cmd->i_file = (char*) malloc(sizeof(char) * 10);
			strncpy(cmd->i_file, "/dev/null", 10);
		}

		if (cmd->o_file == NULL) {
			cmd->o_file = (char*) malloc(sizeof(char) * 10);
			strncpy(cmd->o_file, "/dev/null", 10);
		}
	}

    // Remove the arguments from the count of arguments
	cmd->argc -= args_removed;

    // Return a pointer to the modified command struct
	return cmd;
}

// get_cmd
// Reads a command from the user and parses it into a command struct
// Parameters: none
// Returns: a pointer to a filled command struct
struct command* get_cmd() {
    // Allocate space for the command struct; return NULL if this fails
	struct command* cmd = (struct command*) calloc(1, sizeof(struct command));
	if (cmd == NULL) return NULL;
	
    // Print the shell prompt
	printf(": ");
	fflush(stdout);
	
    // Allocate space for the maximum command size
	char* cmd_buf = (char*) malloc(sizeof(char) * 2049);
    // Buffer variables for use with getline()
	char* buffer = NULL;
	size_t buffer_size = 0;
    // Get a line from stdin; return NULL if there is an error
	getline(&buffer, &buffer_size, stdin);
    if (ferror(stdin) != 0) return NULL;

    // Copy the user input into a buffer of the right size
    strncpy(cmd_buf, buffer, 2048);
    // Remove the newline character
	cmd_buf[strlen(cmd_buf) - 1] = '\0';
    // If the resulting command string is empty, return NULL
	if (strcmp(cmd_buf, "") == 0) return NULL;

    // Replace $$ with the current pid
	expand_sh_vars(&cmd_buf);
    // Read the arguments into the command struct
	parse_cmd_args(cmd, &cmd_buf);
    // Check for I/O redirection
	parse_cmd_io_files(cmd);

    // Free the command string
	free(cmd_buf);
    // Return a pointer to the command struct
	return cmd;
}

// cd
// Changes the current working directory to HOME or a directory given by an argument
// Parameters: a pointer to a non-empty command struct
// Returns: 0 if successful, -1 on failure
int cd(struct command* cmd) {
    // If the command struct pointer is empty, return an error code
	if (cmd == NULL) return -1;
	
    int err;    // An error-checking variable
    // Check for an argument
	if (cmd->argc > 1) {
        // Try to change the current working directory to the argument provided
		err = chdir(cmd->argv[1]);
	} else {
        // Try to change the current working directory to the directory in the HOME environment variable
		err = chdir(getenv("HOME"));
	}
	
    // Return an error code if the directory change failed
	if (err == -1) {
		perror("");
		return -1;
	}

    // Otherwise, return a success code
	return 0;
}

// status
// Prints the exit status or termination signal of the last non-custom foreground process that ended
// Parameters: an integer status code set by waitpid()
// Returns: none
void status(int last_exit_status) {
	// Check if the last exit status was ended by a signal
    if (WIFSIGNALED(last_exit_status) == true) {
        // If so, print the termination signal
		printf("Terminated by signal %d.\n", WTERMSIG(last_exit_status));
	} else {
        // Otherwise, print the exit status
		printf("Exit status %d.\n", WEXITSTATUS(last_exit_status));
	}
	return;
}

// redirect_io
// Redirects stdin and stdout to the files specified in a given command struct
// Parameters: a pointer to a non-empty command struct
// Returns: 0 if successful, -1 on failure
int redirect_io(struct command* cmd) {
    // Check for a specified input file
	if (cmd->i_file != NULL) {
        // Try to open the specified input file for reading only; return an error code if unsuccessful
		int fd = open(cmd->i_file, O_RDONLY);
		if (fd == -1) {
			perror(cmd->i_file);
			return -1;
		}

        // Redirect stdin to the input file; return an error code if unsuccessful
		int err = dup2(fd, STDIN_FILENO);
		if (err == -1) {
			perror("Redirect");
			return -1;
		}
        // Close the input file descriptor
		close(fd);
	}

    // Check for a specified output file
	if (cmd->o_file != NULL) {
        // Try to open the specified output file for writing only; truncate it if it exists; create it if not
        // Return an error code if unsuccessful
		int fd = open(cmd->o_file, O_WRONLY | O_TRUNC | O_CREAT, 0770);
		if (fd == -1) {
			perror(cmd->o_file);
			return -1;
		}

        // Redirect stdout to the output file
		int err = dup2(fd, STDOUT_FILENO);
		if (err == -1) {
			perror("Redirect");
			return -1;
		}
        // Close the output file descriptor
		close(fd);
	}

    // Return a success code
	return 0;
}

// SIGTSTP_handler
// A handler for SIGTSTP (Ctrl + Z); toggles foreground-only mode
// Parameters: the triggering signal number
// Returns: none
void SIGTSTP_handler(int sig_num) {
	// Print a different message depending on the current mode
    if (fg_only_mode == false) {
		write(STDOUT_FILENO, "\nEntering foreground-only mode (& disabled).\n", 45);
	} else {
		write(STDOUT_FILENO, "\nLeaving foreground-only mode (& enabled).\n", 43);
	}
    // Toggle the foreground-only mode flag
	fg_only_mode = !fg_only_mode;
	return;
}

// main
// The program driver; runs a miniature shell
// Parameters: argc, the number of arguments; argv, a list of strings of arguments
// Returns: an exit code
int main(int argc, char** argv) { 
	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = &SIGTSTP_handler;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	int* children = (int*) calloc(512, sizeof(int));
	int n_children = 0;
    int last_exit_status;

	struct command* cmd;

	while(true) {
		// Block SIGINT
		sigset_t sig_proc_mask;
		sigemptyset(&sig_proc_mask);
		sigaddset(&sig_proc_mask, SIGINT);
		sigprocmask(SIG_SETMASK, &sig_proc_mask, NULL);

		int bg_pid, bg_stat;
		bg_pid = waitpid(-1, &bg_stat, WNOHANG);
		if (bg_pid > 0) {
			delete_int(bg_pid, children, &n_children);

			printf("Background process (pid = %d) ended. ", bg_pid);
			if (WIFSIGNALED(bg_stat) == true) {
				printf("Terminated by signal %d.\n", WTERMSIG(bg_stat));
			} else {
				printf("Exit status %d.\n", WEXITSTATUS(bg_stat));
			}
			fflush(stdout);
		}
		
		cmd = get_cmd();
		if (cmd == NULL) continue;

		if (strcmp(cmd->cmd, "exit") == 0) {
			for (int i = 0; i < n_children; i++) {
				kill(children[i], SIGKILL);
			}

			free_command(cmd);
			break;
		}

		if (strchr(cmd->cmd, '#') == cmd->cmd) {
			free_command(cmd);
			continue;
		}

		if (strcmp(cmd->cmd, "cd") == 0) {
			cd(cmd);
			free_command(cmd);
			continue;
		}

		if (strcmp(cmd->cmd, "status") == 0) {
			status(last_exit_status);
			free_command(cmd);
			continue;
		}

		if (fg_only_mode == true && cmd->background == true) {
			cmd->background = false;
		}

		pid_t spawn_pid = fork();
		if (spawn_pid == -1) {
			perror("");
			return EXIT_FAILURE;
		} else if (spawn_pid == 0) {
			sigemptyset(&sig_proc_mask);
			sigaddset(&sig_proc_mask, SIGTSTP);
			if (cmd->background == true) {
				sigaddset(&sig_proc_mask, SIGINT);
			}
			sigprocmask(SIG_SETMASK, &sig_proc_mask, NULL);
			
			int err = redirect_io(cmd);
			if (err == -1) {
				return EXIT_FAILURE;
			}

            execvp(cmd->cmd, cmd->argv);

			perror(cmd->cmd);
			return EXIT_FAILURE;
		} else {
			sigemptyset(&sig_proc_mask);
			sigaddset(&sig_proc_mask, SIGINT);
			sigprocmask(SIG_SETMASK, &sig_proc_mask, NULL);
			
			if (cmd->background) {
				children[n_children++] = spawn_pid;
				
				printf("Background process (pid = %d) created.\n", spawn_pid);
				fflush(stdout);
			} else {
				int child_status;
				waitpid(spawn_pid, &child_status, 0);
				last_exit_status = child_status;
				if (WIFSIGNALED(child_status) == true) {
					printf("\nTerminated by signal %d.\n", WTERMSIG(child_status));
				}
				tcflush(STDIN_FILENO, TCIFLUSH);
			}
		}

		free_command(cmd);
	}

	return EXIT_SUCCESS;
}

