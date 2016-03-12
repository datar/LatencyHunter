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

#endif