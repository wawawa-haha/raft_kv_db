#ifndef TIME_H
#define TIME_H
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <random>
#include <signal.h>
#include "raft_server.h"
class timer{
double time_interval;
static raft_server* m_rs;

public:
timer();
void stop_timer(){
    alarm(0);
}
void start_timer(){
    gen_rand_ty();
    alarm(time_interval);
}
void start_heart_timer(){
    alarm(0.075);
}
void gen_rand_ty();
static void handle_time_out(int sig){};
void set_time_handler(){};
void set_rs(raft_server* rs){
    m_rs = rs;
}
};
timer::timer(){
    m_rs = nullptr;
}
void timer::gen_rand_ty(){
     // 创建随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 创建一个0到1之间的均匀分布的随机数生成器
    std::uniform_real_distribution<> dis(0.0, 1.0);

    // 生成随机数，然后将其缩放到0.15到0.30的范围
    this->time_interval = dis(gen) * (0.30 - 0.15) + 0.15;
}

void timer::set_time_handler(){
    struct sigaction sa;
    sa.sa_handler = handle_time_out;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

void timer::handle_time_out(int sig){
    char msg[1];
    if(m_rs->m_state==server_state::Leader){
        msg[0]= '1';
    }else{
        msg[0] = '0';
    }
    int save_errno = errno;
    int msg = SIGALRM;//写入超时信号，超时处理逻辑放在主循环中去做
    send(pipefds[1], msg, 1, 0);
    errno = save_errno;
}
#endif TIME_H