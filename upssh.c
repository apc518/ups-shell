/*
Andy Chamberlain
2021-02-21
CS 475: Operating Systems with Adam A. Smith
A custom shell
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define HISTORY_LEN 100
#define PATH_MAX_LENGTH 4096

typedef struct history_entry{
    char str[100];
    struct history_entry *prev;
    struct history_entry *next;
} history_entry;

void print_history(const history_entry * h_e);
void change_directory(const char * cmd);
int is_cd(const char * cmd);
int cd_arg_idx(const char * cmd);
void execute_command(const char * cmd);
void execute_program(const char * cmd);
int contains_slashes(const char * str);
int last_slash_idx(const char * path);
int num_chars(const char * str, const char c);
char ** split_by_char(const char * cmd, const char c);
char * parse_executable_name(const char * path);
char * concat_path(const char * oldpath, const char * relpath);
char * env_path(const char * program_name);
void trim_trailing_whitespace(char *str);

int main(int argc, char **argv){
    history_entry *hist_oldest = NULL;
    history_entry *hist_newest = NULL;
    int current_hist_len = 0;

    // input loop
    while(1){
        char input[512] = {0};
        printf("\033[;32mUPS\033[0m:");
        printf("\033[;36m%s\033[0m", getcwd(NULL, PATH_MAX_LENGTH));
        printf("> ");
        fgets(input, 512, stdin);

        // remove newline char at the end of input that is left there by fgets
        int len = strlen(input);
        input[len - 1] = 0;

        if(strlen(input) < 1){
            continue;
        }

        if(strcmp("exit", input) == 0){
            break;
        }
        else{ // enter the command into history before executing it
            history_entry *temp = (history_entry*)malloc(sizeof(history_entry));
            if(temp == NULL){
                printf("This command could not be saved to the command history.\n");
            }
            strcpy(temp->str, input);
            if(hist_newest == NULL){ // shell has just started
                hist_oldest = temp;
            }
            else{
                hist_newest->next = temp;
                temp->prev = hist_newest;
            }

            hist_newest = temp;

            if(current_hist_len < HISTORY_LEN){
                current_hist_len++;
            }
            else{
                history_entry *discard = hist_oldest;
                hist_oldest = hist_oldest->next;
                free(discard);
            }
        }
        if(strcmp("history", input) == 0){
            print_history(hist_oldest);
        }
        else if(strcmp("mypwd", input) == 0){
            printf("%s\n", getcwd(NULL, PATH_MAX_LENGTH));
        }
        else if(is_cd(input)){
            change_directory(input);
        }
        else{ // try to execute a program
            execute_command(input);
        }
    }
    //printf("Goodbye!\n");
    return 0;
}

void print_history(const history_entry * h_e){
    while(h_e->next != NULL){
        if(strlen(h_e->str) > 0) printf("%s\n", h_e->str);
        h_e = h_e->next;
    }
}

void change_directory(const char * cmd){
    int arg_idx = cd_arg_idx(cmd);

    char *arg = (char*)(cmd + arg_idx);
    char *cwd = getcwd(NULL, PATH_MAX_LENGTH);

    int arg_length = strlen(arg);
    int cwd_length = strlen(cwd);

    if(arg[0] == '/'){ // absolute path
        if(arg_length > PATH_MAX_LENGTH){
            printf("Could not change directory: maximum path length exceeded.\n");
            return;
        }
        if(chdir(arg) == -1){
            printf("Could not change directory: invalid path.\n");
        }
    }
    else{ // relative path... here we go!
        // try to navigate to each directory one at a time, checking each one as we go

        const char *delim = "/";
        char *token = strtok(arg, delim); // get first token

        char *original_wd = getcwd(NULL, PATH_MAX_LENGTH);

        while(token != NULL){ // go through the rest of the tokens
            if(strcmp(token, ".") != 0){
                // check that this token added onto the current directory isnt too big
                int token_length = strlen(token);
                cwd = getcwd(NULL, PATH_MAX_LENGTH);
                cwd_length = strlen(cwd);
                if(token_length + cwd_length > PATH_MAX_LENGTH){
                    printf("Could not change directory: maximum path length exceeded.\n");
                    if(chdir(original_wd) != 0){ // change back to original working directory
                        printf("Fatal error: could not return to original path. Exiting...\n");
                        exit(1);
                    }
                    return;
                }

                char * newpath = (char*)malloc(sizeof(char) * (token_length + cwd_length + 2));

                if(newpath == NULL){
                    printf("Could not change directory: blocked by OS.\n");
                    if(chdir(original_wd) != 0){ // change back to original working directory
                        printf("Fatal error: could not return to original path. Exiting...\n");
                        exit(1);
                    }
                    return;
                }

                strcpy(newpath, cwd);
                strcat(newpath, "/");
                strcat(newpath, token);

                if(chdir(newpath) != 0){
                    printf("Could not change directory: path not found.\n");
                    if(chdir(original_wd) != 0){ // change back to original working directory
                        printf("Fatal error: could not return to original path. Exiting...\n");
                        exit(1);
                    }
                    return;
                }

                free(newpath);
            }
            token = strtok(NULL, delim);
        }

    }
}

int is_cd(const char * cmd){
    if(cmd[0] != 'c' || cmd[1] != 'd' || cmd[2] != ' '){
        return 0;
    }
    return 1;
}

int cd_arg_idx(const char * cmd){
    // return -1 if the command isn't a structurally valid cd (doesnt check the directory argument itself)
    // otherwise, return the index where the argument begins

    for(int i = 3; i < PATH_MAX_LENGTH + 3; i++){
        if(cmd[i] != ' '){
            return i;
        }
    }

    return -1;
}

void execute_command(const char * cmd){
    // figure out the ampersand shit
    // call execute_program appropriately
    if(num_chars(cmd, '&') == 0){ // simple easy wonderful synchronous processing
        execute_program(cmd);
        wait(NULL);
    }
    else if(cmd[strlen(cmd) - 1] != '&'){ // execute all at once, wait for all.
        char *cmd_copy =  strdup(cmd);
        int num_children = 0;
        char ** commands = split_by_char(cmd_copy, '&');

        for(int i = 0; commands[i] != NULL && i < strlen(cmd_copy); i++){
            execute_program(commands[i]);
            num_children++;
        }

        for(int i = 0; i < num_children; i++){
            wait(NULL);
        }
    }
    else{ // execute and don't wait.
        char *cmd_copy =  strdup(cmd);
        char ** commands = split_by_char(cmd_copy, '&');

        for(int i = 0; commands[i] != NULL && i < strlen(cmd_copy); i++){
            execute_program(commands[i]);
        }
    }
}

void execute_program(const char * cmd){
    if(strlen(cmd) < 1){
        return;
    }

    char *cmd_copy = strdup(cmd);

    trim_trailing_whitespace(cmd_copy);

    char *path;
    char *program_name;

    char **args = split_by_char(cmd_copy, ' ');
    if(args == NULL){
        printf("Error: memory allocation blocked by OS.\n");
        exit(1);
    }

    if(cmd_copy[0] == '/'){ // its an absolute path
        path = args[0];
        program_name = parse_executable_name(path);
        args[0] = program_name;
        free(program_name);
    }
    else if(contains_slashes(args[0])){ // its a relative path
        char *rel_path = args[0];
        path = concat_path(getcwd(NULL, PATH_MAX_LENGTH), rel_path);
        program_name = parse_executable_name(path);
        args[0] = program_name;
        free(program_name);
    }
    else{ // look for this program in the environment PATH variable
        path = env_path(args[0]);
        if(path == NULL){
            printf("Command '%s' not found.\n", args[0]);
            return;
        }
        program_name = args[0];
    }

    if(access(path, F_OK) != 0){
        printf("File not accessible: %s\n", path);
        return;
    }
    if(access(path, X_OK) != 0){
        printf("%s is not an executable file.\n", path);
        return;
    }

    pid_t pid = fork();
    if(pid == 0){ // child process
        execv(path, args);
        printf("Unknown error trying to run %s.\n", path);
        exit(1);
    }
}

int contains_slashes(const char * str){
    int length = strlen(str);
    for(int i = 0; i < length; i++){
        if(str[i] == '/'){
            return 1;
        }
    }
    return 0;
}

int last_slash_idx(const char * path){
    // returns -1 if no slash is found, or the index of the last slash
    int length = strlen(path);
    int idx = -1;
    for(int i = 0; i < length; i++){
        if(path[i] == '/'){
            idx = i;
        }
    }
    return idx;
}

int num_chars(const char * str, const char c){
    int num = 0;
    int length = strlen(str);
    for(int i = 0; i < length; i++){
        if(str[i] == c){
            num++;
        }
    }
    return num;
}

char ** split_by_char(const char * cmd, const char c){
    char *cmd_copy = strdup(cmd);
    char **args = (char**)malloc(sizeof(char*) * (num_chars(cmd_copy, c) + 2));
    if(args == NULL) return NULL;

    args[num_chars(cmd_copy, c) + 1] = NULL;
    
    const char delim[2] = {c, 0};
    char *token = strtok(cmd_copy, delim);
    
    for(int i = 0; token != NULL; i++){
        args[i] = token;
        token = strtok(NULL, delim);
    }

    return args;
}

char * parse_executable_name(const char * path){
    int path_length = strlen(path);
    int last_slash = last_slash_idx(path);
    int name_length = path_length - last_slash;
    char *name = (char*)malloc(sizeof(char) * name_length + 2);
    for(int i = 0; i < name_length - 1; i++){
        name[i] = path[last_slash + 1 + i];
    }
    name[name_length - 1] = 0;
    return name;
}

char * concat_path(const char * oldpath, const char * relpath){
    char * newpath = (char*)malloc(sizeof(char) * (strlen(oldpath) + strlen(relpath) + 2));
    if (newpath == NULL) return NULL;

    strcpy(newpath, oldpath);
    strcat(newpath, "/");
    strcat(newpath, relpath);

    return newpath;
}

char * env_path(const char * program_name){
    char *big_path = strdup(getenv("PATH"));
    char delim[2] = ":";
    char *token = strtok(big_path, delim);

    while(token != NULL){
        char * newpath = concat_path(token, program_name);
        if (newpath == NULL) return NULL;

        if(access(newpath, F_OK) == 0 && access(newpath, X_OK) == 0){ // if file is accessible and executable
            return newpath;
        }
        free(newpath);
        token = strtok(NULL, delim);
    }
    return NULL;
}

void trim_trailing_whitespace(char *str){
    char *end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = 0;
}