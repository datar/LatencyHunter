#include <iostream>
#include "headers.h"

using namespace std;

int main(){
	cout<<sysconf(_SC_NPROCESSORS_ONLN)<<endl;
    if (mlockall(MCL_CURRENT | MCL_FUTURE))
        fprintf(stderr, "failed to lock all memory (ignored)\n");
	return 0;
}
