#ifndef MESS_H
#define MESS_H
#include <string.h>
#include <unordered_map>
#include <cjson/cJSON.h>
#include <vector>
// 定义Raft消息类型
class LogEntry;
enum class MessageType {
    VoteRequest,
    VoteResponse,
    Heartbeat,
    AppendEntries,
    AppendEntriesResponse,
    CommitLog
    // 可以添加更多消息类型
};

// 定义日志条目结构
class LogEntry {
public:
    int term;
    std::string command;  // 例如： "set key1 value1"
LogEntry(){};
LogEntry(int i,std::string s){
    term = i;
    s = command;
}
};


// Raft消息类
class Message {
public:
    public:
    int from_server_id;
    MessageType type;  // 消息类型
    int term;           // 当前任期号，用于领导人选举和心跳机制
    int leader_id;      // 领导者ID（如果是领导者发的消息）
    int entry_id;
    int appendornot;    //0不添加，1添加
    LogEntry logentry;
    //std::vector<LogEntry> entries;  // 日志条目（如果是AppendEntries消息）


    // 其他可能需要的字段：
    int vote_for;
    int prev_log_index;        // 前一条日志的索引（用于日志匹配）
    int prev_log_term;         // 前一条日志的任期号
    bool success;              // 用于AppendEntriesResponse消息，指示日志复制是否成功
    // ...
    Message (){};
    // 构造函数

    Message(MessageType type, int term, int leader_id,int rsid)
        : type(type), term(term), leader_id(leader_id),from_server_id(rsid) {
        }
 Message(MessageType type, int term, int leader_id ,int rsid,int vote_for)//votefor消息的构造函数
        : type(type), term(term), leader_id(leader_id),from_server_id(rsid),vote_for(vote_for) {
        }

   Message(MessageType type, int term, int leader_id,int rsid,std::string s)
        : type(type), term(term), leader_id(leader_id),from_server_id(rsid){
            logentry = LogEntry(term,s);
        }
    // 添加日志条目到消息中

std::string Message::serialize() const {
    cJSON *message_json = cJSON_CreateObject();
    
    // 添加消息类型
    cJSON_AddItemToObject(message_json, "type", cJSON_CreateNumber((int)type));
    
    // 添加任期号
    cJSON_AddItemToObject(message_json, "term", cJSON_CreateNumber(term));
    
    // 添加领导者ID
    cJSON_AddItemToObject(message_json, "leader_id", cJSON_CreateNumber(leader_id));
    cJSON_AddItemToObject(message_json, "entry_id", cJSON_CreateNumber(entry_id));
    
    // 添加单个日志条目
    cJSON *logentry_json = cJSON_CreateObject();
    cJSON_AddItemToObject(logentry_json, "term", cJSON_CreateNumber(logentry.term));
    cJSON_AddItemToObject(logentry_json, "command", cJSON_CreateString(logentry.command.c_str()));
    cJSON_AddItemToObject(message_json, "logentry", logentry_json);
    cJSON_AddItemToObject(message_json, "appendornot", cJSON_CreateNumber(appendornot));

    // 转换JSON对象为字符串并释放对象
    char *serialized_message = cJSON_Print(message_json);
    cJSON_Delete(message_json);
    // 返回序列化后的字符串
    return serialized_message;
}
Message Message::deserialize(const std::string& json_str) {
    cJSON *message_json = cJSON_Parse(json_str.c_str());
    if (!message_json) {
        // JSON 解析失败，处理错误
        throw std::runtime_error("JSON parsing failed");
    }

    // 从 JSON 对象中获取各个字段的值
    MessageType type = (MessageType)cJSON_GetObjectItem(message_json, "type")->valueint;
    int term = cJSON_GetObjectItem(message_json, "term")->valueint;
    int leader_id = cJSON_GetObjectItem(message_json, "leader_id")->valueint;
    int entry_id =  cJSON_GetObjectItem(message_json, "entry_id")->valueint;

    // 解析单个日志条目
    cJSON *logentry_json = cJSON_GetObjectItem(message_json, "logentry");
    int logentry_term = cJSON_GetObjectItem(logentry_json, "term")->valueint;
    const char *logentry_command = cJSON_GetObjectItem(logentry_json, "command")->valuestring;
    int appendornot = cJSON_GetObjectItem(message_json, "appendornot")->valueint;

    // 释放 JSON 对象
    cJSON_Delete(message_json);

    // 构造 Message 对象并返回
    Message message(type, term, leader_id, -1);
    message.entry_id = entry_id;
    message.logentry = LogEntry{logentry_term, logentry_command};

};
};
#endif