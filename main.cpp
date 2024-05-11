# include "raft_server.h"
# include <string>
int main(){
    auto x = new raft_server(1,{{2,"127.0.0.1"}},8089,0);
    x->start();
    std::string tem;
    while(getline(std::cin,tem)){
        std::cout<<x->process_command(tem);
    }
    
}