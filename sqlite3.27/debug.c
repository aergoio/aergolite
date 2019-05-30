
#if DEBUGPRINT

#ifdef __ANDROID__
/*
#define DEBUG_LOG(...) do { \
  FILE *f = fopen("sdcard/log.txt", "a+"); \
  fprintf(f, __VA_ARGS__); \
  fflush(f); \
  fclose(f); \
} while (0)
*/
#include <android/log.h>
#define DEBUG_LOG(...)    do { __android_log_print(ANDROID_LOG_VERBOSE, "aergolite", __VA_ARGS__); } while (0)

#else  /* NOT __ANDROID__ */

//#define DEBUG_LOG(...)    do { printf(__VA_ARGS__); fflush(stdout); } while(0);

#include <unistd.h>  /* for getpid() */

#ifdef _WIN32
typedef DWORD thread_id_t;
#define GET_THREAD_ID(X)  X = GetCurrentThreadId()
#elif defined(__APPLE__)
#include <pthread.h> /* for pthread_threadid_np */
typedef uint64_t thread_id_t;
#define GET_THREAD_ID(X)  pthread_threadid_np(NULL, &X)
#else
typedef pthread_t thread_id_t;
#define GET_THREAD_ID(X)  X = pthread_self()
#endif

#define DEBUG_THREADS 256

thread_id_t _main_tid=0;
thread_id_t _worker_tid[DEBUG_THREADS]={0};
thread_id_t _last_tid=0;

static int thread_idx(thread_id_t tid){
  int i;
  for(i=0; i<DEBUG_THREADS; i++){
    if( _worker_tid[i]==0 )
      _worker_tid[i] = tid;
    if( _worker_tid[i]==tid )
      return i+1;
  }
  return 0;
}

SQLITE_PRIVATE void aergoliteLogToFile(char *format, va_list ap) {
  thread_id_t _thread_id;
  FILE *f;
  char _fname[256];
  sprintf(_fname, "log-%d", getpid());
  f = fopen(_fname, "a+");
  GET_THREAD_ID(_thread_id);
  if( _main_tid==0 ) _main_tid = _thread_id;
  if( _thread_id!=_last_tid ) fprintf(f, "\n");
  _last_tid = _thread_id;
  if( _thread_id==_main_tid ){
    fprintf(f, "[MAIN] ");
  } else {
    fprintf(f, "[WORKER %d] ", thread_idx(_thread_id));
  }
  vfprintf(f, format, ap);
  fclose(f);
}

SQLITE_PRIVATE void aergoliteLogPrint(char *format, va_list ap) {
  thread_id_t _thread_id;
  GET_THREAD_ID(_thread_id);
  if( _main_tid==0 ) _main_tid = _thread_id;
  if( _thread_id!=_last_tid ) printf("\n");
  _last_tid = _thread_id;
  if( _thread_id==_main_tid ){
    printf("[MAIN] ");
  } else {
    printf("[WORKER %d] ", thread_idx(_thread_id));
  }
  vprintf(format, ap);
}

#define DEBUG_LOG_TOFILE  aergoliteLogToFile
#define DEBUG_LOG_PRINT   aergoliteLogPrint

#if TARGET_OS_IPHONE
#define DEBUG_LOG DEBUG_LOG_PRINT
#else
//#define DEBUG_LOG DEBUG_LOG_TOFILE
#define DEBUG_LOG DEBUG_LOG_PRINT
#endif

SQLITE_API void aergolite_log(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  DEBUG_LOG(format, ap);
  va_end(ap);
}

#endif  /* NOT __ANDROID__ */

#endif  /* DEBUGPRINT */


#ifdef DEBUGPRINT
#define SYNCTRACE         aergolite_log
#define SYNCERROR         aergolite_log
#define CODECTRACE        aergolite_log
#else
#define SYNCTRACE(...)
#define SYNCERROR(...)
#define CODECTRACE(...)
#endif
