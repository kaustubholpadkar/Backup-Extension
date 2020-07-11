#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/reg.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/limits.h>


// function to check if a given path is regular file
int is_regular_file(const char *path)
{
  struct stat ps;
  stat(path, &ps);
  return S_ISREG(ps.st_mode);
}


// function to split a given string wrt delimiter
char ** str_split(char* input_string, const char delimiter)
{
  char ** tokens = 0;
  char* temp_string = input_string;
  char* last_delim = 0;

  size_t count = 0;
  size_t index  = 0;

  char delim[2];
  delim[0] = delimiter;
  delim[1] = 0;

  // count the number of splits
  while (*temp_string)
  {
    if (delimiter == *temp_string)
    {
      count++;
      last_delim = temp_string;
    }
    temp_string++;
  }

  count += last_delim < (input_string + strlen(input_string) - 1);
  count++;

  tokens = malloc(sizeof(char*) * count);

  if (tokens)
  {
    char* token = strtok(input_string, delim);
    
    while (token)
    {
      assert(index < count);
      *(tokens + index++) = strdup(token);
      token = strtok(0, delim);
    }

    *(tokens + index) = 0;
  }

  return tokens;
}


// function to get the current timestamp as a string
static void get_timestamp(char buf[]) {
  time_t ctime;
  struct tm *lt;
  time(&ctime);
  lt = localtime(&ctime);
  strftime(buf, 85, ".%a_%Y-%m-%d_%H:%M:%S_%Z.bkp", lt);
}


// function to get filename from the file descriptor in system call
static void get_filename_from_file_descriptor(pid_t child, char * filename) {
  ssize_t r;
  int MAXSIZE = 0xFFF;
  char data[0xFFF];

  int fd = (int) ptrace(PTRACE_PEEKUSER, child, sizeof(long) * EBX, 0);

  sprintf(data, "/proc/self/fd/%d", fd);
  r = readlink(data, filename, MAXSIZE);

  if (r >= 0)
  {
    filename[r] = '\0';
  }

}


// function to get filename from the file descriptor and path in the system call
static void get_filename_from_file_descriptor_and_path (pid_t child, char * filename, int * flag) {

  char *child_addr;
  ssize_t r;
  int MAXSIZE = 0xFFF;
  char data[0xFFF];

  int fd = (int) ptrace(PTRACE_PEEKUSER, child, sizeof(long) * EBX, 0);
  child_addr = (char *) ptrace(PTRACE_PEEKUSER, child, sizeof(long) * ECX, 0);
  * flag = (int) ptrace(PTRACE_PEEKUSER, child, sizeof(long) * EDX, 0);

  int ffd = openat(fd, child_addr, 0);

  sprintf(data, "/proc/self/fd/%d", ffd);
  r = readlink(data, filename, MAXSIZE);

  if (r >= 0)
  {
    filename[r] = '\0';
  }

  close(ffd);
}


// function to get the file name from child process in the system call
static void get_filename (pid_t child, char * file, int * flag) {

  char *child_addr;
  int i;
  int length = 0;

  child_addr = (char *) ptrace(PTRACE_PEEKUSER, child, sizeof(long) * EBX, 0);
  * flag = (int) ptrace(PTRACE_PEEKUSER, child, sizeof(long) * ECX, 0);

  do {
    long val;
    char *p;

    val = ptrace(PTRACE_PEEKTEXT, child, child_addr, NULL);

    if (val == -1) {
      fprintf(stderr, "PTRACE_PEEKTEXT error: %s\n", strerror(errno));
      exit(1);
    }

    child_addr += sizeof (long);

    p = (char *) &val;
    for (i = 0; i < sizeof (long); ++i, ++file) {
      *file = *p++;
      length++;
      if (*file == '\0') break;
    }
  } while (i == sizeof (long));
}


// function to read the file content
char * read_file (FILE *file) {

  // find the length of file
  fseek(file, 0, SEEK_END);
  int length = ftell(file);
  fseek(file, 0, SEEK_SET);

  char * data;

  if (length == 0) {
    data = "";
  } else {
    data = malloc((length + 1) * sizeof(char));
    fread(data, sizeof(char), length, file);
    data[length] = '\0';
  }
  return data;
}


// function to write content to the file
void write_file (FILE *file, char *data) {
  fputs(data, file);
}


// function to get the absolute path from given relative / absolute path
char * get_absolute_path (char * path) {
  char abs_path[PATH_MAX];
  char * res = realpath(path, abs_path);
  return res;
}


// function to create directory if it does not exist
static void create_directory_if_not (char * path) {
  struct stat st = {0};
  if (stat(path, &st) == -1) {
    mkdir(path, 0777);
  }
}


// function to create directory tree from given path and base directory
static void create_directory_tree (char * path, char * base_dir_path) {

  const char delim = '/';
  char arr_path[PATH_MAX];
  strcpy(arr_path, path);
  char ** tokens = str_split(arr_path, delim);
  char * current_path = (char *) malloc(PATH_MAX * sizeof(char));
  strcpy(current_path, base_dir_path);

  for (int i = 0; * (tokens + i + 1); i++) {
    strcat(current_path, "/");
    strcat(current_path, *(tokens + i));
    create_directory_if_not(current_path);
    free(*(tokens + i));
  }

  free(tokens);
  free(current_path);
}


// function to create a backup file in the backup directory
static void create_backup_file (char * original_file, char * backup_path) {

  char current_timestamp[85];
  char * content;
  FILE * src_file;
  FILE * dst_file;
  char * abs_path;
  char * full_backup_path = (char *) malloc(PATH_MAX * sizeof(char));

  get_timestamp(current_timestamp);

  if (access(original_file, F_OK ) != -1) {

    // file exists
    int is_file = is_regular_file(original_file);

    if (is_file) {

      abs_path = get_absolute_path(original_file);

      strcpy(full_backup_path, backup_path);
      strcat(full_backup_path, abs_path);
      strcat(full_backup_path, current_timestamp);

      src_file = fopen(abs_path, "r");
      content = read_file (src_file);
      fclose(src_file);

      create_directory_tree(abs_path, backup_path);

      printf("backup file created at: %s\n", full_backup_path);

      dst_file = fopen(full_backup_path, "w");
      write_file (dst_file, content);
      fclose(dst_file);
    }
  }

  free(full_backup_path);

}


// function to create a backup directory if does not exist
static void create_backup_dir(char * path) {

  struct stat st = {0};

  if (stat(path, &st) == -1) {
    mkdir(path, 0777);
  }

}
// till this

int main(int argc, char **argv) {

  if (argc != 2) {
    printf("Exactly One Argument Expected!\n");
    return -1;
  }

  int * flag;
  int status, backup_flag;
  int insyscall = 0;
  long syscall;
  char * shell = "/bin/bash";
  char * new_argv[4] = {shell, "-c", argv[1], NULL};

  char * original_file;

  struct passwd * pw = getpwuid(getuid());
  const char * homedir = pw->pw_dir;
  const char * backupdir_name = "/.backup";

  char * backup_path = (char *) malloc(PATH_MAX * sizeof(char));

  strcpy(backup_path, homedir);
  strcat(backup_path, backupdir_name);
  create_backup_dir(backup_path);

  // create a child process
  pid_t child = fork();


  if (child == 0) {

    // monitored process
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    execv(shell, new_argv);

  }

  else {

    // monitoring process

    while (1) {

      wait(&status);

      if (WIFEXITED(status)) {
        break;
      }

      backup_flag = 0;
      flag = (int *) malloc(sizeof(int));
      syscall = ptrace(PTRACE_PEEKUSER, child, 4 * ORIG_EAX, NULL);
      original_file = (char *) malloc(PATH_MAX * sizeof(char));

      // system call : open
      if (syscall == SYS_open) {

        if (insyscall == 0) {
          // system call entry
          insyscall = 1;
          get_filename(child, original_file, flag);

          if (*flag % 4) {
            backup_flag = 1;
          }
        }

        else {
          // system call exit
          insyscall = 0;
        }

      }

      // system call : openat
      else if (syscall == SYS_openat) {

        if (insyscall == 0) {
          // system call entry
          insyscall = 1;
          get_filename_from_file_descriptor_and_path(child, original_file, flag);

          if (*flag % 4) {
            backup_flag = 1;
          }
        }

        else {
          // system call exit
          insyscall = 0;
        }

      }

      // system call : rename
      else if (syscall == SYS_rename) {

        if (insyscall == 0) {
          // system call entry
          insyscall = 1;
          backup_flag = 1;

          get_filename(child, original_file, flag);
        }

        else {
          // system call exit
          insyscall = 0;
        }

      }

      // system call : renameat
      else if (syscall == SYS_renameat) {

        if (insyscall == 0) {
          // system call entry
          insyscall = 1;
          backup_flag = 1;

          get_filename_from_file_descriptor_and_path(child, original_file, flag);
        }

        else {
          // system call exit
          insyscall = 0;
        }

      }

      // system call : truncate
      else if (syscall == SYS_truncate) {

        if (insyscall == 0) {
          // system call entry
          insyscall = 1;
          backup_flag = 1;

          get_filename(child, original_file, flag);
        }

        else {
          // system call exit
          insyscall = 0;
        }

      }

      // system call : ftruncate
      else if (syscall == SYS_ftruncate) {

        if (insyscall == 0) {
          // system call entry
          insyscall = 1;
          backup_flag = 1;

          get_filename_from_file_descriptor(child, original_file);
        }

        else {
          // system call exit
          insyscall = 0;
        }

      } 

      // any other system call
      else {

        insyscall = 0;

      }

      if (backup_flag) {
        create_backup_dir(backup_path);
        create_backup_file(original_file, backup_path);
      }

      free(flag);
      free(original_file);
      ptrace(PTRACE_SYSCALL, child, NULL, NULL);

    }

  }

  return 0;
}

