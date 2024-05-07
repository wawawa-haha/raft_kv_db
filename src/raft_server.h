#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <arpa/inet.h> // for networking
#include <unistd.h>
#include "KVDATABASE.h" // Assuming this is your KVDatabase class
#include <set>
#include "message.h"
enum server_state{
    Leader = 0,
    Follower = 1,
    Candidate
};
const int MAX_SERVERS = 10000;
class raft_server {
private:
    int epoll_fd; // epoll 文件描述符
    
    server_state state;
    int server_id;
    std::vector<std::pair<int,std::string> > server_addresses; // Map of server IDs to their IP addresses
    
    // Socket related variables
    int native_port;
    int server_listen_fd;//用来监听连接请求的的套接字。
    //std::mutex socket_mutex; // Mutex for socket operations
    std::set<int> cluster_socket_fds; // Array of socket descriptors for cluster communication
    //struct sockaddr_in server_addresses[MAX_SERVERS]; // Array of server addresses
    int num_servers; 
    int init_native_fd(int port){};


    // Raft related variables
    int votes_received; // Number of votes received in the current term
    int leader_id; // ID of the current leader
    bool election_in_progress; // Flag to indicate if an election is in progress
    int current_term;
    int voted_for;
    std::vector<std::string> log; // Log entries
    std::queue<std::string> pending_logs; // Pending log entries waiting to be committed
    //std::mutex log_mutex; // Mutex for log operations
    std::condition_variable log_cv; // Condition variable for log operations
    
    // Key-Value Database
    KVDatabase kv_db;

    //vote相关
    void start_election(){};//开始一次领导者选举。
    //void vote_request(){};// 发送投票请求给所有其他节点。
    void handle_vote_request(int fd,const std::string& message);//处理投票请求
    void handle_vote_response(const std::string& message);// 处理投票应答。
    //message相关
    std::string create_vote_request_message() {};
    //日志相关
    void append_entries(){}; //领导者用于将日志项复制给所有跟随者。
    void commit_log_vector(){};//在收到来自leader的提交请求后提交日志
    void return_commit_res(){}//在提交完成后将完成结果返回给leader
    void execute_log(const std::string& log_entry);//执行日志，该操作会改变复制状态向量机。
    
    //心跳机制
    void send_heartbeats(){};//领导者定期向跟随者发送心跳以维持其领导者状态。
    void handle_heartbeat(){};//追随者处理接收到的信息。

   
    // Function to handle client requests
    void handle_client_request();
    //发送消息函数
    void send_message(const std::string& message, int destination_server_fd);//所有类型的消息都需要函数发送
    void send_message_to_all(const std::string& message){}//
    std::string receive_message(int sender_server_id);//从某个服务器接受消息

public:
    
    raft_server(int id, const std::vector<std::pair<int,std::string>>& addresses,int port);
    void init_cluster_sockets();
    // Function to start the server
    void start();

    // Function to stop the server
    void stop();

    // Function to append log entry
    void append_log(const std::string& log_entry);

    // Function to send request to set key-value pair
    std::string set(int key, const std::string& value);

    // Function to send request to get value by key
    std::string get(int key);

    // Function to send request to delete key-value pair by key
    std::string del(int key);

    // Function to handle receiving log entry from leader
    void receive_log_entry(const std::string& log_entry);
};
