#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>  


#ifdef __INFO__  
#define INFO(format,...) printf("File: "__FILE__", Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)  
#else  
#define INFO(format,...)  
#endif  


#ifdef __DEBUG__  
#define DEBUG(format,...) printf("File: "__FILE__", Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)  
#else  
#define DEBUG(format,...)  
#endif  

#ifdef __ERROR__  
#define ERROR(format,...) printf("File: "__FILE__", Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)  
#else  
#define ERROR(format,...)  
#endif  

/* Assert-like macros */
#define TEST(x)                                                 \
  do {                                                          \
    if( ! (x) ) {                                               \
      fprintf(stderr, "ERROR: '%s' failed\n", #x);              \
      fprintf(stderr, "ERROR: at %s:%d\n", __FILE__, __LINE__); \
      exit(1);                                                  \
    }                                                           \
  } while( 0 )

#define TRY(x)                                                          \
  do {                                                                  \
    int __rc = (x);                                                     \
      if( __rc < 0 ) {                                                  \
        fprintf(stderr, "ERROR: TRY(%s) failed\n", #x);                 \
        fprintf(stderr, "ERROR: at %s:%d\n", __FILE__, __LINE__);       \
        fprintf(stderr, "ERROR: rc=%d errno=%d (%s)\n",                 \
                __rc, errno, strerror(errno));                          \
        exit(1);                                                        \
      }                                                                 \
  } while( 0 )



#endif