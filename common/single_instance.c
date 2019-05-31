#if defined(__linux__)  /* includes Android */
#include <sys/socket.h>
#include <sys/un.h>
#endif

/***************************************************************************/

static void calc_unique_name(char *dest, char *str) {
  int crc1, crc2;

  crc1 = crc32(str, strlen(str));
  crc2 = crc32rev(str, strlen(str));

  sprintf(dest, "litesync%x%x", crc1, crc2);

}

/***************************************************************************/

static void create_mutex_name(char *dest, char *str) {

  strcpy(dest, "Global\\");
  dest += strlen(dest);

  calc_unique_name(dest, str);

}

/***************************************************************************/

//#ifndef _WIN32
/*
** -> it must be up to 96 characters <-
** implemented with a fixed prefix + crc32 of the given str:
**   /tmp/litesync12345678
*/
static void create_unix_tempfile_name(char *dest, char *str) {

  strcpy(dest, "/tmp/");
  dest += strlen(dest);

  calc_unique_name(dest, str);

}
//#endif

/***************************************************************************/

#if !defined(_WIN32) && !defined(__linux__)

enum LockOperation {
  LOCK,
  UNLOCK
};

static int LockFile(int fd, enum LockOperation lock) {
#if 1

  return flock(fd, lock == LOCK ? LOCK_EX | LOCK_NB : LOCK_UN);

#else

  // init the flock parameter struct
  struct flock fl;
  fl.l_type = lock == LOCK ? F_WRLCK : F_UNLCK;

  // lock the entire file
  fl.l_start =
  fl.l_len =
  fl.l_whence = 0;

  // is this needed?
  fl.l_pid = getpid();

  return fcntl(fd, F_SETLK, &fl);

#endif
}

#endif

/***************************************************************************/

SQLITE_PRIVATE BOOL check_single_instance(aergolite *this_node, char *dbpath) {
  BOOL success=FALSE;

#if _WIN32

  char mutex_name[128];
  HANDLE hMutex;

  create_mutex_name(mutex_name, dbpath);
  SYNCTRACE("check_single_instance dbpath=%s mutex_name=%s\n", dbpath, mutex_name);
  if (mutex_name[0] == 0) return FALSE;

  /* Try to create the mutex */
  hMutex = CreateMutexA(NULL, FALSE, mutex_name);
  if( GetLastError()==ERROR_ALREADY_EXISTS ){
    /* Mutex exist. Another instance may be running */
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
  } else if( hMutex ){
    this_node->single_instance = hMutex;
    success = TRUE;
  }

#elif defined(__linux__)  /* includes Android */

  char socket_name[128];
  int sock;

  calc_unique_name(socket_name, dbpath);
  SYNCTRACE("check_single_instance dbpath=%s socket_name=%s\n", dbpath, socket_name);

  if( (sock=socket(AF_UNIX,SOCK_STREAM,0)) < 0 ){
    return FALSE;
  } else {
    struct sockaddr_un addr;
    int name_len, addr_len, rc;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    name_len = strlen(socket_name);
    addr.sun_path[0] = '\0';
    strncpy(addr.sun_path + 1, socket_name, name_len);
    addr_len = offsetof(struct sockaddr_un, sun_path) + 1 + name_len;
    rc = bind(sock, (struct sockaddr *) &addr, addr_len);
    if( rc ){
      assert(sock > 2);  /* cannot close stdout and stderr */
      close(sock);
    } else {
      this_node->single_instance = sock;
      success = TRUE;
    }
  }

#else

  char file_name[256];
  int  fdLock;

#if TARGET_OS_IPHONE
  strcpy(file_name, dbpath);
  strcat(file_name, "-lock");
#else
  create_unix_tempfile_name(file_name, dbpath);
#endif

  SYNCTRACE("check_single_instance dbpath=%s lock_file=%s\n", dbpath, file_name);
  if (file_name[0] == 0) return FALSE;

  /* try to open the file */
  fdLock = open(file_name, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  if (fdLock == -1) return FALSE;

  /* try to lock the file */
  if (LockFile(fdLock, LOCK) != 0) {
    /* failed */
    close(fdLock);
  } else {
    this_node->single_instance = fdLock;
    success = TRUE;
  }

#endif

  return success;

}

/***************************************************************************/

SQLITE_PRIVATE void release_single_instance(aergolite *this_node) {

  SYNCTRACE("release_single_instance\n");

#if _WIN32

  if( this_node->single_instance ){
    ReleaseMutex(this_node->single_instance);
    CloseHandle(this_node->single_instance);
    this_node->single_instance = NULL;
  }

#elif defined(__linux__)  /* includes Android */

  if( this_node->single_instance ){
    assert(this_node->single_instance > 2);  /* cannot close stdout and stderr */
    close(this_node->single_instance);
    this_node->single_instance = 0;
  }

#else

  if( this_node->single_instance ){
    LockFile(this_node->single_instance, UNLOCK);
    close(this_node->single_instance);
    this_node->single_instance = 0;
  }

#endif

}
