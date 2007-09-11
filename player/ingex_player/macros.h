#ifndef __MACROS_H__
#define __MACROS_H__


#ifdef __cplusplus
extern "C" 
{
#endif


/* function checks */
#define CHK(cond) \
    if (!(cond)) \
    { \
        ml_log_error("'%s' failed, in %s:%d\n", #cond, __FILE__, __LINE__); \
    }

#define CHK_ORET(cond) \
    if (!(cond)) \
    { \
        ml_log_error("'%s' failed, in %s:%d\n", #cond, __FILE__, __LINE__); \
        return 0; \
    }

#define CHK_OFAIL(cond) \
    if (!(cond)) \
    { \
        ml_log_error("'%s' failed, in %s:%d\n", #cond, __FILE__, __LINE__); \
        goto fail; \
    }

#define CHK_ORETV(cond) \
    if (!(cond)) \
    { \
        ml_log_error("'%s' failed, in %s:%d\n", #cond, __FILE__, __LINE__); \
    }

#define CHK_OCMD(cond, cmd) \
    if (!(cond)) \
    { \
        ml_log_error("'%s' failed, in %s:%d\n", #cond, __FILE__, __LINE__); \
        cmd; \
    }

    
/* memory allocations with check */
#define CALLOC_ORET(data, type, size) \
    CHK_ORET((data = (type*)calloc(sizeof(type), size)) != NULL);

#define CALLOC_OFAIL(data, type, size) \
    CHK_OFAIL((data = (type*)calloc(sizeof(type), size)) != NULL);

#define MALLOC_ORET(data, type, size) \
    CHK_ORET((data = (type*)malloc(size * sizeof(type))) != NULL);

#define MALLOC_OFAIL(data, type, size) \
    CHK_OFAIL((data = (type*)malloc(size * sizeof(type))) != NULL);

#define MEM_ALLOC_ORET(data, func, type, size) \
    CHK_ORET((data = (type*)func(size * sizeof(type))) != NULL);

#define MEM_ALLOC_OFAIL(data, func, type, size) \
    CHK_OFAIL((data = (type*)func(size * sizeof(type))) != NULL);



/* free the memory and set the variable to NULL */
#define SAFE_FREE(d_ptr) \
    if (*d_ptr != NULL) \
    { \
        free(*d_ptr); \
        *d_ptr = NULL; \
    }


/* Helpers for logging 
e.g. ml_log_error("Some error %d" LOG_LOC_FORMAT, x, LOG_LOC_PARAMS); */
#define LOG_LOC_FORMAT      ", in %s:%d\n"
#define LOG_LOC_PARAMS      __FILE__, __LINE__      


/* pthreads */
#define PTHREAD_MUTEX_LOCK(x) \
    if (pthread_mutex_lock( x ) != 0 ) \
    { \
        ml_log_error("pthread_mutex_lock failed" LOG_LOC_FORMAT, LOG_LOC_PARAMS); \
    }
    
#define PTHREAD_MUTEX_UNLOCK(x) \
    if (pthread_mutex_unlock( x ) != 0 ) \
    { \
        ml_log_error("pthread_mutex_unlock failed" LOG_LOC_FORMAT, LOG_LOC_PARAMS); \
    }
    
#define PTHREAD_COND_WAIT(x) \
    if (pthread_cond_wait( x ) != 0 ) \
    { \
        ml_log_error("pthread_cond_wait failed" LOG_LOC_FORMAT, LOG_LOC_PARAMS); \
    }
    
#define PTHREAD_COND_SIGNAL(x) \
    if (pthread_cond_signal( x ) != 0 ) \
    { \
        ml_log_error("pthread_cond_signal failed" LOG_LOC_FORMAT, LOG_LOC_PARAMS); \
    }
    


/* 64-bit printf formatting */
    
#if defined(__x86_64__)
#define PFi64 "ld"
#define PFu64 "lu"
#define PFoff "ld"
#else
#define PFi64 "lld"
#define PFu64 "llu"
#define PFoff "lld"
#endif
    



#ifdef __cplusplus
}
#endif


#endif


