#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

char * stripchr(char *mainstr, int separator) {
  char *ptr;
  if (mainstr == NULL) return NULL;
  ptr = strchr(mainstr, separator);
  if (ptr == 0) return NULL;
  ptr[0] = '\0';
  ptr++;
  return ptr;
}

char * striprchr(char *mainstr, int separator) {
  char *ptr;
  if (mainstr == NULL) return NULL;
  ptr = strrchr(mainstr, separator);
  if (ptr == 0) return NULL;
  ptr[0] = '\0';
  ptr++;
  return ptr;
}

int add_file(char *path, char *file, FILE *fpwrite) {
  FILE *fp;
  char file_path[512], buf[512], copy[512], *ptr;

  sprintf(file_path, "%s/%s", path, file);

  fp = fopen(file_path, "r");
  if (fp == 0) {
    printf("File NOT included: %s\n", file_path);
    return FALSE;
  }

  printf("  %s\n", file_path);

  while (!feof(fp)) {
    if (fgets(buf, 512, fp) == NULL) break;
    stripchr(buf, 10);
    if (strncmp(buf, "#include \"", 10) == 0) {
      char base[256];
      int ret;
      strcpy(copy, buf);
      ptr = (buf + 10);
      stripchr(ptr, '"');
      if (strchr(ptr,'/')) {
        file = striprchr(ptr,'/');
        sprintf(base,"%s/%s",path,ptr);
        ret = add_file(base, file, fpwrite);
      }else{
        ret = add_file(path, ptr, fpwrite);
      }
      if (ret==FALSE) {
        strcpy(buf, copy);
        goto copy_line;
      }
    } else if (strcmp(buf, "#include <binn.h>") == 0) {
      if (add_file(".","binn.c",fpwrite) == FALSE) {
        goto copy_line;
      }
    } else {
    copy_line:
      fputs(buf, fpwrite);
      fputs("\n", fpwrite);
    }
  }

  fclose(fp);
  return TRUE;

}

void process_file(char* path, char *infile, char *outfile) {
  FILE *fpwrite;

  printf("Processing file %s...\n", infile);

  fpwrite = fopen(outfile, "w");
  if (fpwrite == 0) {
    printf("Failed: could not open file: %s\n", outfile);
    exit(1);
  }

  add_file(path, infile, fpwrite);

  fclose(fpwrite);
}

int main() {

#ifdef _WIN32
  system("copy ..\\binn\\src\\binn.c core\\");
  system("copy ..\\binn\\src\\binn.h core\\");
#else
  //system("cp ../binn/src/binn.c core/");
  //system("cp ../binn/src/binn.h core/");
#endif

  system("mkdir -p amalgamation");
  system("rm amalgamation/*");

  process_file("core", "sqlite3.h", "amalgamation/sqlite3.h");
  process_file("core", "sqlite3.c", "amalgamation/sqlite3.c");
  process_file("plugins/mini-raft", "mini-raft.c", "amalgamation/sqlite3.c");

#ifdef _WIN32
  system("del core\\binn.c");
  system("del core\\binn.h");
#else
  //system("rm core/binn.c");
  //system("rm core/binn.h");
#endif

  puts("done");
  return 0;
}
