#include <dirent.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "util/log.h"

#if __APPLE__
#include <fcntl.h>
#endif

#include "util/common.h"

unsigned long
djb2_hash(unsigned char const *str) {
  unsigned long hash = 5381;
  int c;

  while ((c = *str++) != 0)
    hash = ((hash << 5) + hash) + (unsigned)c; /* hash * 33 + c */

  return hash;
}

unsigned int upper_pow2(unsigned int v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;

  return v;
}

long get_ncores(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

int dir_exists(char const *dname) {
  struct stat st = {0};
  return stat(dname, &st) != -1;
}

void dir_mk(char const *dname) {
  if (dir_exists(dname))
    return;
  mkdir(dname, 0700);
}

uint32_t dir_num_files(char const *dname) {
  uint32_t file_count = 0;
  DIR * dirp;
  struct dirent * entry;

  dirp = opendir(dname); /* There should be error handling after this */
  if (!dirp)
    return 0;

  while ((entry = readdir(dirp)) != NULL) {
    if (entry->d_type == DT_REG) { /* If the entry is a regular file */
      file_count++;
    }
  }
  closedir(dirp);

  return file_count;
}

size_t file_read(FILE *f, char **out) {
  size_t cap = 8192;
  size_t nbytes = 1024;
  size_t index = 0;
  size_t nread = 0;
  
  if (!f)
    panic_txt("Empty pointer passed as the file.");

  char *ret = malloc(cap);
  fseek(f, 0, SEEK_SET);

  while (1) {
    nread = fread(ret + index, 1, nbytes, f);

    if (nread != nbytes) {
      if (feof(f))
        break;
      perror("Error: ");
      panic_txt("Couldn't read the file.");
    }

    if (index + nbytes  >= cap) {
      cap *= 2;
    }
    ret = realloc(ret, cap);
    index += nread;
  }
  *out = ret;
  return index + nread;
}

int fd_path(int fd, char **buf) {
  char *ret = 0;
#ifdef _WIN32
#		error "Phuck Windows."
#elif __APPLE__
	if (fcntl(fd, F_GETPATH, ret) != -1)
		return -1;
  ret = malloc(sizeof(char) * PATH_MAX);
  *buf = ret;
	return 0;
#elif __linux__
    // linux
  ret = malloc(sizeof(char) * PATH_MAX);
  char name[1024] = {0};;
  snprintf(name, 1024, "/proc/self/fd/%d", fd);
  int size = readlink(name, ret, PATH_MAX);
  if (size == -1){
    free(ret);
		return -1;
  }
  ret[size] = 0;
  *buf = ret;
	return 0;
#else
#   error "Unknown compiler"
#endif

  *buf = ret;
	return 0;
}
