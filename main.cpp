//#include "csbuild.h"
#include "proxy.h"
#include <pthread.h>

int main() {
    //file.open("proxy.log", std::ofstream::out);
    const char * port = "12345";
    std::cout<<"in main\n";
    Proxy my_proxy(port);
    std::cout<<"get proxy\n";
    my_proxy.init_Proxy();
    std::cout<<"after proxy\n";
    return 1;
}
