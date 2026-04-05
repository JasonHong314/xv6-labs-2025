#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "user/user.h"

void
run_exec(char *file, char **cmd, int cmdargc)
{
  int pid;
  char *exec_argv[MAXARG];

  if(cmdargc + 2 > MAXARG){
    fprintf(2, "find: too many arguments for exec\n");
    return;
  }

  for(int i = 0; i < cmdargc; i++){
    exec_argv[i] = cmd[i];
  }
  exec_argv[cmdargc] = file;
  exec_argv[cmdargc + 1] = 0;

  pid = fork();
  if(pid < 0){
    fprintf(2, "find: fork failed\n");
    return;
  }

  if(pid == 0){
    exec(exec_argv[0], exec_argv);
    fprintf(2, "find: exec %s failed\n", exec_argv[0]);
    exit(1);
  } else {
    wait(0);
  }
}

void
find(char *path, char *target, int exec_mode, char **cmd, int cmdargc)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  char name[DIRSIZ + 1];

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if(st.type == T_FILE){
    char *q = path + strlen(path);
    while(q >= path && *q != '/')
      q--;
    q++;

    if(strcmp(q, target) == 0){
      if(exec_mode)
        run_exec(path, cmd, cmdargc);
      else
        printf("%s\n", path);
    }

    close(fd);
    return;
  }

  if(st.type != T_DIR){
    close(fd);
    return;
  }

  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
    fprintf(2, "find: path too long\n");
    close(fd);
    return;
  }

  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';

  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;

    if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      continue;

    memmove(name, de.name, DIRSIZ);
    name[DIRSIZ] = 0;

    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;

    if(stat(buf, &st) < 0)
      continue;

    if(strcmp(name, target) == 0){
      if(exec_mode)
        run_exec(buf, cmd, cmdargc);
      else
        printf("%s\n", buf);
    }

    if(st.type == T_DIR)
      find(buf, target, exec_mode, cmd, cmdargc);
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc == 3){
    find(argv[1], argv[2], 0, 0, 0);
  } else if(argc >= 5 && strcmp(argv[3], "-exec") == 0){
    find(argv[1], argv[2], 1, &argv[4], argc - 4);
  } else {
    fprintf(2, "usage: find <dir> <name> [-exec cmd ...]\n");
    exit(1);
  }

  exit(0);
}