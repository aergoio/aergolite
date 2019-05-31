
#ifdef _WIN32
typedef HANDLE single_instance_handle;
#else
typedef int    single_instance_handle;
#endif
