#include <string>
#include <unordered_map>
#include </usr/local/include/cjson/cJSON.h>
#include <vector>
// 定义Raft消息类型
class LogEntry;
enum class MessageType {
    VoteRequest,
    VoteResponse,
    Heartbeat,
    AppendEntries,
    AppendEntriesResponse,
    // 可以添加更多消息类型
};

// 定义日志条目结构
class LogEntry {
public:
    int term;
    std::string command;  // 例如： "set key1 value1"
    // 可以添加更多字段
};


// Raft消息类
class Message {
public:
    public:
    MessageType type;  // 消息类型
    int term;           // 当前任期号，用于领导人选举和心跳机制
    int leader_id;      // 领导者ID（如果是领导者发的消息）
    std::vector<LogEntry> entries;  // 日志条目（如果是AppendEntries消息）

    // 其他可能需要的字段：
    int prev_log_index;        // 前一条日志的索引（用于日志匹配）
    int prev_log_term;         // 前一条日志的任期号
    bool success;              // 用于AppendEntriesResponse消息，指示日志复制是否成功
    // ...

    // 构造函数
    Message(MessageType type, int term, int leader_id = -1)
        : type(type), term(term), leader_id(leader_id) {}

    // 添加日志条目到消息中
    void add_entry(const LogEntry& entry) {
        entries.push_back(entry);
    }

    // 将消息转换为字符串，用于调试
    std::string serialize() const {
        cJSON *message_json = cJSON_CreateObject();
        
        // 添加消息类型
        cJSON_AddItemToObject(message_json, "type", cJSON_CreateNumber((int)type));
        
        // 添加任期号
        cJSON_AddItemToObject(message_json, "term", cJSON_CreateNumber(term));
        
        // 添加领导者ID
        cJSON_AddItemToObject(message_json, "leader_id", cJSON_CreateNumber(leader_id));
        
        // 添加日志条目
        cJSON *entries_json = cJSON_CreateArray();
        for (const auto& entry : entries) {
            cJSON *entry_json = cJSON_CreateObject();
            cJSON_AddItemToObject(entry_json, "term", cJSON_CreateNumber(entry.term));
            cJSON_AddItemToObject(entry_json, "command", cJSON_CreateString(entry.command.c_str()));
            cJSON_AddItemToArray(entries_json, entry_json);
        }

        cJSON_AddItemToObject(message_json, "entries", entries_json);

        // 转换JSON对象为字符串并释放对象
        char *serialized_message = cJSON_Print(message_json);
        cJSON_Delete(message_json);
        // 返回序列化后的字符串
        return serialized_message;
    }
    Message deserialize(const std::string& json_str) {
    cJSON *message_json = cJSON_Parse(json_str.c_str());
    if (!message_json) {
        // JSON 解析失败，处理错误
        throw std::runtime_error("JSON parsing failed");
    }

    // 从 JSON 对象中获取各个字段的值
    MessageType type = (MessageType)cJSON_GetObjectItem(message_json, "type")->valueint;
    int term = cJSON_GetObjectItem(message_json, "term")->valueint;
    int leader_id = cJSON_GetObjectItem(message_json, "leader_id")->valueint;

    // 解析日志条目数组
    cJSON *entries_json = cJSON_GetObjectItem(message_json, "entries");
    std::vector<LogEntry> entries;
    if (entries_json && cJSON_IsArray(entries_json)) {
        int array_size = cJSON_GetArraySize(entries_json);
        for (int i = 0; i < array_size; ++i) {
            cJSON *entry_json = cJSON_GetArrayItem(entries_json, i);
            int entry_term = cJSON_GetObjectItem(entry_json, "term")->valueint;
            const char *command = cJSON_GetObjectItem(entry_json, "command")->valuestring;
            entries.push_back({entry_term, command});
        }
    }

    // 释放 JSON 对象
    cJSON_Delete(message_json);

    // 构造 Message 对象并返回
    Message message(type, term, leader_id);
    for (const auto& entry : entries) {
        message.add_entry(entry);
    }

    return message;
}

};