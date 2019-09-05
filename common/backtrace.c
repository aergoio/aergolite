#ifdef _MSC_VER
#include "dbghelp.h"
#endif

#ifdef __ANDROID__
#include <unwind.h>
#include <dlfcn.h>

struct android_backtrace_state {
  void **current;
  void **end;
};

_Unwind_Reason_Code android_unwind_callback(struct _Unwind_Context* context, void* arg) {
  struct android_backtrace_state* state = (struct android_backtrace_state *) arg;
  uintptr_t pc = _Unwind_GetIP(context);
  if( pc ){
    if( state->current == state->end ){
      return _URC_END_OF_STACK;
    } else {
      *state->current++ = (void*)pc;
    }
  }
  return _URC_NO_REASON;
}

void dump_android_stack(void) {
  const int max = 100;
  void* buffer[max];

  struct android_backtrace_state state;
  state.current = buffer;
  state.end = buffer + max;

  DEBUG_LOG("--- android stack dump ---");

  _Unwind_Backtrace(android_unwind_callback, &state);

  int count = (int)(state.current - buffer);

  for (int idx = 0; idx < count; idx++){
    const void* addr = buffer[idx];
    const char* symbol;
    Dl_info info;
    if( dladdr(addr,&info) && info.dli_sname ){
      symbol = info.dli_sname;
    }else{
      symbol = "";
    }
    DEBUG_LOG("%03d: 0x%p %s", idx, addr, symbol);
  }

  DEBUG_LOG("---");
}
#endif  //__ANDROID__


#ifndef _WIN32
#include <execinfo.h>
#endif


void print_backtrace() {

#ifdef _MSC_VER
  unsigned int   i;
  void         * stack[100];
  unsigned short frames;
  SYMBOL_INFO  * symbol;
  HANDLE         process;

  process = GetCurrentProcess();

  SymInitialize(process, NULL, TRUE);

  frames = CaptureStackBackTrace(0, 100, stack, NULL);
  symbol = (SYMBOL_INFO *) calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
  symbol->MaxNameLen = 255;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  for( i=0; i<frames; i++ ){
    SymFromAddr(process, (DWORD64) (stack[i]), 0, symbol );
    printf("%i: %s - 0x%0X\n", frames - i - 1, symbol->Name, symbol->Address);
  }

  free(symbol);
#elif defined(_WIN32)
/* no stacktrace support on MinGW for now */
#elif defined(__ANDROID__)
  dump_android_stack();
#elif TARGET_OS_IPHONE
/* no stacktrace support on iOS for now */
#else
  void *array[32];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 32);

  // print out all the frames to stderr
  backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif

  exit(1);
}
