#include <iostream>
#include "headers.h"

using namespace std;

int main(){
	cout<<sysconf(_SC_NPROCESSORS_ONLN)<<endl;
	return 0;
}
