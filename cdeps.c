#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>


#define TRUE 1
#define FALSE 0
#define MAX_TOKEN_LENGTH 1024
#define MAX_LINE_LENGTH 4096
#define MAX_INCLUDE_FILES = 1024

#define USER_HEADER 1
#define SYSTEM_HEADER 2

/*
 * Usage related functions
 */

void print_usage() {
    printf("Usage: cdeps <target>\n");
    printf("where target can be one of the following\n");
    printf("filename.c - filename of the C source file whose dependencies should be displayed.\n");
    printf("directory  - root of directory tree containing C source files that should have their\n");
    printf("             dependencies displayed.\n");
}

void check_usage(int argc) {
    if(argc != 2) {
        print_usage();
        if (argc > 0) {
            exit(1);
        } else {
            exit(0);
        }
    }
}

void error_exit(char *message) {
    puts(message);
    exit(1);
}

/*
 * Push back reader related functions
 */
int pushedCh = 0;

int readc(FILE *fp) {
    int ch;
    if(pushedCh) {
        ch = pushedCh;
        pushedCh = 0;
    } else {
        ch = fgetc(fp);
    }
    return ch;
}

void pushc(char ch) {
    pushedCh = ch;
}

void skip_to_end_of_single_line_comment(FILE* fp) {
    int ch;
    do {
        ch = readc(fp);
    } while(ch != EOF && ch != '\n');
}

void skip_to_end_of_multi_line_comment(FILE* fp) {
    int ch;

    /* We are in a multi line comment but have reached the end
       of the file. Pretend that this comment was closed as the
       last thing in the file. */
    while((ch = readc(fp)) != EOF) {
        if(ch == '*') {
            ch = readc(fp);
            if(ch == '/') {
                return;
            } else {
                pushc(ch);
            }
        }
    }
}

int readc_source_code(FILE* fp) {
    int ch;
    static int inside_string = FALSE;
    static int nextCh = 0;

    if(nextCh) {
        ch = nextCh;
        nextCh = 0;
        return ch;
    }

    ch = readc(fp);

    if(ch == EOF) {
        return EOF;
    }

    if(inside_string) {
        if(ch == '\\') {
            nextCh = readc(fp);
            return ch;
        } else if(ch == '"') {
            inside_string = FALSE;
            return ch;
        } else {
            return ch;
        }
    } else {
        if(ch == '/') {
            ch = readc(fp);

            if(ch == EOF) {
                ch = EOF;
            } else if(ch == '/') {
                skip_to_end_of_single_line_comment(fp);
                ch = '\n';
            } else if(ch == '*') {
                skip_to_end_of_multi_line_comment(fp);
                ch = 0;
            } else {
                pushc(ch);
                ch = '/';
            }
        } else if(ch == '"') {
            inside_string = TRUE;
        }
    }

    return ch;
}

/*
The readc_without_comments function works like fgetc except that
it will just skip characters that are part of single or multi line
comments.
*/
int readc_without_comments(FILE *fp) {
    int ch;

    while((ch = readc_source_code(fp)) != EOF) {
        if (ch != 0) {
            return ch;
        }
    }

    return ch;
}

int check_for_string(FILE* fp, char *string) {
    unsigned long k;
    int ch;
    for(k=0; k<strlen(string); k++) {
        ch = readc_without_comments(fp);
        if(ch == EOF) {
            return FALSE;
        } else if(ch != string[k]) {
            return FALSE;
        }
    }
    return TRUE;
}

void eat_whitespace(FILE* fp) {
    int ch;
    while((ch = readc_without_comments(fp))) {
        if (ch == EOF) {
            return;
        }

        if (ch != ' ' && ch != '\t') {
            pushc(ch);
            break;
        }
    }
}

void read_to_end_of_line(FILE* fp) {
    int ch;
    while((ch = readc_without_comments(fp))) {
        if (ch == EOF || ch == '\n') {
            return;
        }
    }
}

void read_include_filename(FILE* fp, char *token) {
    int k;
    int ch;

    for(k=0; k<MAX_TOKEN_LENGTH-2; k++) {
        ch = readc_without_comments(fp);

        if(ch == EOF) {
            error_exit("Unexpected end of file");
        } else if(ch == '"') {
            token[k] = 0;
            return;
        } else {
            token[k] = ch;
        }
    }

    error_exit("Include filename was too long");
}

int parse_include_filename(FILE* fp, char *token) {
    int ch;
    if(!check_for_string(fp, "include")) {
        return FALSE;
    }

    eat_whitespace(fp);

    ch = readc_without_comments(fp);

    if(ch != '<' && ch != '"') {
        error_exit("Something wrong with include statement");
    }

    if(ch == '"') {
        read_include_filename(fp, token);
        return TRUE;
    } else {
        read_to_end_of_line(fp);
        return FALSE;
    }
}

int readline_without_comments(FILE *fp, char *buffer) {
    int ch;
    int count;
    for(count = 0; count<MAX_LINE_LENGTH; count++) {
        ch = readc_without_comments(fp);

        if(ch == EOF) {
            if(count == 0) {
                buffer[count] = 0;
                return EOF;
            }
            ch = '\n';
        }

        if(ch == '\n') {
            buffer[count] = 0;
            break;
        } else {
            buffer[count] = ch;
        }
    }

    if(count >= MAX_LINE_LENGTH-1) {
        error_exit("Line is too long");
    }

    return count;
}

char *clonestr(char *string) {
    if (string == NULL) {
        error_exit("Trying to clone a null string");
    }
    char *clone = malloc(strlen(string)+1);
    strcpy(clone, string);
    return clone;
}

void process_file(char *filename) {
    int status;
    char buffer[MAX_LINE_LENGTH];
    FILE *fp;
    int include_length = strlen("#include");
    char *include_filenames[1024];
    int include_filename_count = 0;

    fp = fopen(filename, "r");
    if(fp == NULL) {
        printf("Could not open file: %s\n", filename);
        exit(1);
    }

    while((status = readline_without_comments(fp, buffer)) != EOF) {
        char *line = buffer;

        while(*line == ' ' || *line == '\t') {
            line++;
        }

        if(!strncmp("#include", line, include_length)) {
            line+=include_length;

            while(*line == ' ' || *line == '\t') {
                line++;
            }

            int header_type;

            char begin_quote_char = *line;
            char end_quote_char = 0;

            if(begin_quote_char == '"') {
                header_type = USER_HEADER;
                end_quote_char = '"';
            } else if (begin_quote_char == '<') {
                header_type = SYSTEM_HEADER;
                end_quote_char = '>';
            } else {
                error_exit("Unexpected header quote character");
            }
            char *include_filename = ++line;

            while(*line != end_quote_char) {
                line++;
            }

            if(*line == end_quote_char) {
                *line = 0;
            } else {
                error_exit("Unexpected end of include filename");
            }

            if(header_type == USER_HEADER) {
                include_filenames[include_filename_count++] = clonestr(include_filename);
            }
        }
    }
    fclose(fp);

    printf("Number of include files: %d\n", include_filename_count);
    for(int k=0; k<include_filename_count; k++) {
        printf("%s\n", include_filenames[k]);
    }

    for(int k=0; k<include_filename_count; k++) {
        free(include_filenames[k]);
    }
}

int is_c_filename(char *filename) {
    char *s = strrchr(filename, '.');

    if(s) {
        return !strcmp(s, ".c");
    } else {
        return FALSE;
    }
}

int is_c_file(struct dirent *directory_entry) {
    if(directory_entry->d_type == DT_REG) {
        return is_c_filename(directory_entry->d_name);
    } else {
        return FALSE;
    }
}

char *join_path_elements(char *path1, char *path2) {
    int len = strlen(path1) + strlen(path2) + 2;
    char *result = malloc(len);

    snprintf(result, len, "%s/%s", path1, path2);
    return result;
}

void process_folder(char *dirname) {
    DIR* directory;
    struct dirent *directory_entry;

    directory = opendir(dirname);
    if(directory) {
        while((directory_entry = readdir(directory))) {
            char *filename = directory_entry->d_name;
            if(!strcmp(".", filename)) {
                continue;
            } else if(!strcmp("..", filename)) {
                continue;
            } else if(!strcmp(".git", filename)) {
                continue;
            } else if(is_c_file(directory_entry)) {
                filename = join_path_elements(dirname, filename);
                process_file(filename);
                free(filename);
            } else if(directory_entry->d_type == DT_DIR) {
                char *subdir_name = join_path_elements(dirname, filename);
                process_folder(subdir_name);
                free(subdir_name);
            }
        }

        if(closedir(directory)) {
            printf("Could not close directory %s", dirname);
        }
    } else {
        printf("Could not open directory %s", dirname);
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    struct stat statbuffer;
    char *target;
    int last_index;

    check_usage(argc);
    target = argv[1];

    if(stat(target, &statbuffer) != 0) {
        error_exit("The target does not exist");
    }

    if(S_ISREG(statbuffer.st_mode)) {
        process_file(target);
    } else if (S_ISDIR(statbuffer.st_mode)) {
        // Remove trailing slash on directory name
        last_index = (int)strlen(target)-1;
        if(target[last_index]=='/') {
            target[last_index] = 0;
        }
        process_folder(target);
    } else {
        error_exit("The target needs to be a file or a folder");
    }
}
