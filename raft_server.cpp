#include "raft_server.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include "message.h"
raft_server::raft_server(int id, const std::vector<std::pair<int,std::string>>& addresses,int port,int actmode)
    : m_server_id(id), server_addresses(addresses), num_servers(addresses.size()), m_mode(actmode) {

    entry_id = 1;
    message_pool =  threadpool(m_mode);
    message_pool.rs = this;
    this->native_port = port;
    this->server_listen_fd = init_native_fd(this->native_port);
    init_cluster_sockets();
      if (pipe(pipefds) == -1) {
            perror("pipe failed");
            exit(EXIT_FAILURE);
        }

        // 设置管道读端为非阻塞
        if (fcntl(pipefds[0], F_SETFL, O_NONBLOCK) == -1) {
            perror("fcntl failed to set non-blocking");
            exit(EXIT_FAILURE);
        }
    //epoll模块
     epoll_fd = epoll_create1(5);
        if (epoll_fd == -1) {
            // 处理错误
            perror("Failed to create epoll");
            exit(EXIT_FAILURE);
        }

        // 初始化服务器和套接字
        init_native_fd(port);
        init_cluster_sockets();

        // 添加监听套接字到 epoll
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = server_listen_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_listen_fd, &event) == -1) {
            // 处理错误
            perror("Failed to add listen fd to epoll");
            exit(EXIT_FAILURE);
        }
        //添加管道读端到epoll监听事件
        struct epoll_event time_event;
        time_event.data.fd = pipefds[0];
        time_event.events = EPOLLIN; 
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipefds[0], &event) == -1) {
            perror("epoll_ctl failed to add pipe read end");
            exit(EXIT_FAILURE);
        }
        // 添加其他套接字到 epoll
        for (auto fd : cluster_socket_fds) {
            event.data.fd = fd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
                // 处理错误
                perror("Failed to add cluster fd to epoll");
                exit(EXIT_FAILURE);
            }
        }

}
std::string raft_server::process_command(std::string s){
    this->command = s;
    std::string tosend = create_append_message();
    append_entries(tosend);
    
}
void raft_server::append_entries(std::string s){
    for(auto x:this->cluster_socket_fds){
        write(x,s.c_str(),s.size());
    }
}
void raft_server::handle_append_entries(int fd,Message m){
    if(m.logentry.term>=m_term&&(entry_id+1 == m.entry_id)){
        pending_logs.push({entry_id+1,m.logentry.command});
        std::string tem = creaet_AppendEntriesResponse();
        write(fd,tem.c_str(),tem.size());
        return;
    }else{
        return;
    }
 }
void raft_server::handle_append_entries_response(int fd,Message m){
 if(m.appendornot==1){
        append_num++;
        if(append_num/cluster_socket_fds.size()>=0.5){
            
        }
    }
}
std::string raft_server::creaet_AppendEntriesResponse(){
    Message temmessage = Message(MessageType::AppendEntriesResponse,m_term,m_server_id,m_server_id);
    temmessage.appendornot = 1;
    temmessage.appendornot = true;
    return temmessage.serialize();

}
std::string raft_server::create_append_message() {
        Message temmessage = Message(MessageType::AppendEntries,m_term,m_server_id,m_server_id,command);
        return temmessage.serialize();
        // 创建并返回投票请求消息
        // 消息格式通常包括当前任期、候选人ID、以及最后日志条目的索引和项
        // 这里只是一个示例，实际中需要包含更多的信息
}
void raft_server::socket_listen_thread(){
    std::vector<epoll_event> events(num_servers);
    while (this->running==true) { // 无限循环，直到服务器停止
        int num_events = epoll_wait(epoll_fd, events.data(), num_servers, -1);

        if (num_events == -1) {
            // epoll_wait失败
            perror("epoll_wait failed");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == server_listen_fd) {
                // 监听套接字上有连接请求,在init_cluster_sockets()函数中已经完成了全部套接字的连接,这种情况是有机器宕机
                accept_connection(server_listen_fd);
            }else{
                handle_data(events[i].data.fd);
            }
        }
    }
}

//定时器相关线程函数
void raft_server::timer_thread_func() {
    char tem[1];
    m_timer = timer();
    m_timer.set_rs(this);    
    m_timer.set_time_handler();
while(this->running == true){
    if(this->m_state == server_state::Leader){
        //Leader定期发送不附带消息的心跳报文/
        m_timer.start_heart_timer();//固定的心跳计时器
    }else{
        //follower等待超时发起选举申请。
        m_timer.start_timer();//rand计时器
    }
    read(pipefds[0], tem, sizeof(tem)); //管道读端读出1，代表心跳超时，重新发送心跳。读0代表选举超时，开始选举。
    if(tem[0]=='1'){
        send_heartbeats();
    }else{
        start_election();
    }
}
}
std::string raft_server::create_heart_beat_message(){
     Message temmessage = Message(MessageType::Heartbeat,m_term,m_server_id,m_server_id);
    return temmessage.serialize();
}
void raft_server::send_heartbeats(){
    m_timer.start_heart_timer();
    for (auto x: cluster_socket_fds) {
            if (x != m_server_id) { // 不向自己发送投票请求
                send_message(create_heart_beat_message(),x);//将心跳的string发送给出了自己外的所有fd
            }
        }
}
void raft_server::handle_heartbeat(int fd,Message m){
        if(m.term>=this->m_term){
            if(this->m_state == server_state::Leader){
                m_state = server_state::Follower;
                handle_vote_request(fd,m);
                m_timer.start_timer();
            }else{
                m_timer.start_timer();
            }
    }
}


void raft_server::start(){
    running = true;

    // 启动套接字监听和处理线程
    listen_thread = std::thread(&raft_server::socket_listen_thread, this);

    // 启动定时心跳和超时线程
    timer_thread = std::thread(&raft_server::timer_thread_func, this);

    // 等待线程结束
    listen_thread.join();
    timer_thread.join();

    // epoll_event结构数组用于存储epoll_wait的返回事件

}


//投票相关的函数
void raft_server::start_election(){
        m_timer.start_timer();
        m_term++;
        // 2. 给自己投票
        voted_for = m_server_id;
        votes_received++;
        // 3. 转换为候选人状态
        m_state = server_state::Candidate;
        // 4. 发送投票请求给所有其他服务器
        for (auto x: cluster_socket_fds) {
            if (x != m_server_id) { // 不向自己发送投票请求
                send_message(create_vote_request_message(),x);//将投票请求的string发送给出了自己外的所有fd
            }
        }
}
//该函数生成序列化后的投票请求
    std::string raft_server::create_vote_request_message() {
        Message temmessage = Message(MessageType::VoteRequest,m_term,-1,m_server_id);
        return temmessage.serialize();
        // 创建并返回投票请求消息
        // 消息格式通常包括当前任期、候选人ID、以及最后日志条目的索引和项
        // 这里只是一个示例，实际中需要包含更多的信息
}
void raft_server::handle_vote_request(int fd,Message m){
    if(m_state == server_state::Leader){
        if(m.term>m_term){
            m_term = m.term;
            m_state = server_state::Follower;
            voted_for = m.from_server_id;
            Message temmessage = Message(MessageType::VoteResponse,m_term,-1,m_server_id,m.vote_for);
            std::string tosend(temmessage.serialize());
            send(fd,tosend.c_str(),tosend.size(),MSG_DONTWAIT);
        }else{
            return;
        }
    }
}
void raft_server::handle_vote_response(int fd,Message m){
    if(m.vote_for == this->m_server_id){
        vote_num++;
        if(vote_num/cluster_socket_fds.size()>=0.5){
            m_state = server_state::Leader;
        }
    }
}

//日志相关函数


 
void  raft_server::accept_connection(int fd){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Error: Unable to create socket for server " << std::endl;
        }
        
        sockfd= accept(fd,NULL,0);
        
        // 将套接字放入set中
        this->cluster_socket_fds.insert(sockfd);
}

void raft_server::handle_data(int fd) {
    char buffer[1024];
    ssize_t count = read(fd, buffer, sizeof(buffer));
    if (count < 0) {
        perror("Error reading from socket");
        close(fd);
        return;
    } else if (count == 0) {
        // 对方关闭了连接
        close(fd);
        return;
    }
    Message tem;
    // 将读取到的数据封装为Message对象
    tem = tem.deserialize(buffer);
    // 创建SocketTask对象
    SocketTask* task = new SocketTask(fd, tem);
    // 将SocketTask对象添加到线程池的等待队列中
    if (!message_pool.append(task)) {
        std::cerr << "Failed to add task to thread pool." << std::endl;
        delete task; // 如果添加失败，需要删除任务以避免内存泄漏
    }
}
//该函数完成bind，但是未开始监听套接字，可以使用一个线程去监听这个套接字
int init_native_fd(int port){
     struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int res = 0;
    // 1. 创建套接字
    res = socket(AF_INET, SOCK_STREAM, 0);
    if (res < 0) {
        perror("Cannot create socket");
        return 1;
    }

    // 2. 设置套接字选项
    int optval = 1;
    if (setsockopt(res, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        return 1;
    }

    // 3. 绑定套接字
    server_addr.sin_family = AF_INET; // 使用IPv4地址
    server_addr.sin_addr.s_addr = INADDR_ANY; // 绑定到所有可用接口
    server_addr.sin_port = htons(port); // 绑定到端口port

    if (bind(res, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
}

void raft_server::init_cluster_sockets() {
    // 创建套接字
    for (auto& pair : server_addresses) {
        int server_id = pair.first;
        const std::string& ip_address = pair.second;
        
        // 创建套接字
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Error: Unable to create socket for server " << server_id << std::endl;
            continue; // 继续下一个服务器
        }
        
        // 初始化服务器地址结构
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
        server_addr.sin_port = htons(server_id); // 使用服务器ID作为端口号
        
        // 连接服务器
        if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Error: Unable to connect to server " << server_id << " at address " << ip_address << std::endl;
            close(sockfd);
            continue; // 继续下一个服务器
        }
        
        // 将套接字放入set中
        cluster_socket_fds.insert(sockfd);
    }
}