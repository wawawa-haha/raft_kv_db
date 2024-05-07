#include "raft_server.h"

raft_server::raft_server(int id, const std::vector<std::pair<int,std::string>>& addresses,int port)
    : server_id(id), server_addresses(addresses), num_servers(addresses.size()) {
    this->native_port = port;
    this->server_listen_fd = init_native_fd(this->native_port);
    init_cluster_sockets();
     epoll_fd = epoll_create1(0);
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

        // 添加其他套接字到 epoll
        for (auto fd : cluster_socket_fds) {
            event.data.fd = fd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
                // 处理错误
                perror("Failed to add cluster fd to epoll");
                exit(EXIT_FAILURE);
            }
        }
    // 初始化其他变量...

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
void raft_server::start_election(){
        current_term++;
        // 2. 给自己投票
        voted_for = server_id;
        votes_received++;
        // 3. 转换为候选人状态
        state = server_state::Candidate;
        // 4. 发送投票请求给所有其他服务器
        for (auto x: cluster_socket_fds) {
            if (x != server_id) { // 不向自己发送投票请求
                send_message(create_vote_request_message(),x);//将投票请求的string发送给出了自己外的所有fd
            }
        }
}
//该函数生成序列化后的投票请求
    std::string raft_server::create_vote_request_message() {
        Message temmessage = Message(MessageType::VoteRequest,current_term,-1);
        return temmessage.serialize();
        // 创建并返回投票请求消息
        // 消息格式通常包括当前任期、候选人ID、以及最后日志条目的索引和项
        // 这里只是一个示例，实际中需要包含更多的信息
}