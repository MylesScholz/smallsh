#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct command {
	char* cmd;
	int argc;
	char* argv[512];
};

struct command* get_cmd() {
	struct command* cmd = (struct command*) malloc(sizeof(struct command));
	if (cmd == NULL) return NULL;
	
	char cmd_buf[2049];
	printf(": ");
	scanf("%2048s", cmd_buf);

	char* token, * saveptr;
	token = strtok_r(cmd_buf, " ", &saveptr);
	cmd->cmd = (char*) malloc(sizeof(char) * (strlen(token) + 1));
	strncpy(cmd->cmd, token, strlen(token) + 1);

	// TODO: parse arguments
	
	return cmd;
}

int main(int argc, char** argv) {
	struct command* cmd;
	while(1) {
		cmd = get_cmd();

		if (strcmp(cmd->cmd, "exit") == 0) {
			break;
		} else {
			// TODO: free cmd
		}
	}

	return EXIT_SUCCESS;
}

