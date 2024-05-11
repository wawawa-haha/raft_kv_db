
#ifndef KVDATABASE_H
#define KVDATABASE_H

#include <iostream>
#include <vector>
#include <memory>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

class SkipList {
private:
    struct Node {
        int key;
        std::string value;
        std::vector<std::shared_ptr<Node> > next;

        Node(int key, std::string value, int level)
            : key(key), value(std::move(value)), next(level, nullptr) {}
    };



public:
    std::shared_ptr<Node> head; // 指向头节点

    int maxLevel;
    double p;

    int NextLevel(int currentLevel) {
        return currentLevel < maxLevel ? currentLevel + 1 : currentLevel;
    }

    int RandomLevel() {
        int level = 1;
        while (rand() / (double)RAND_MAX < p && level < maxLevel) {
            level++;
        }
        return level;
    }

    void Traverse() {
        std::shared_ptr<Node> current = head->next[0];
        int i = 1;
        while (current) {
            std::cout << "pair" << i << ": key_" << current->key << " value_" << current->value << std::endl;
            current = current->next[0];
            i++;
        }
    }

    std::vector<std::shared_ptr<Node> > Search(const int& key) {
        std::vector<std::shared_ptr<Node> > update(maxLevel, nullptr);
        std::shared_ptr<Node> x = head;

        for (int i = maxLevel - 1; i >= 0; --i) {
            while (x->next[i] && x->next[i]->key < key) {
                x = x->next[i];
                update[i] = x;
            }
            update[i] = x;
        }

        return update;
    }

    SkipList(double p = 0.5, int maxLevel = 16) : maxLevel(maxLevel), p(p), head(std::make_shared<Node>(0, "", maxLevel)) {
    }

    bool Insert(int key, std::string value) {
        if (value.size()==0) return false; // 空值不插入

        std::vector<std::shared_ptr<Node> > update = Search(key);
        std::shared_ptr<Node> x = head;

        for (int i = 0; i < maxLevel; ++i) {
            if (update[i]->next[i] && update[i]->next[i]->key == key) {
                return false; // 键已存在
            }
        }

        int level = RandomLevel();
        std::shared_ptr<Node> newNode = std::make_shared<Node>(key, value, level);

        for (int i = 0; i < level; ++i) {
            newNode->next[i] = update[i]->next[i]; 
            update[i]->next[i] = newNode;
        }
        return true;
    }

    std::shared_ptr<Node> Find(int key) {
        std::vector<std::shared_ptr<Node> > update = Search(key);
        std::shared_ptr<Node> x = head;

        for (int i = maxLevel - 1; i >= 0; --i) {
            if (update[i]->next[i] && update[i]->next[i]->key == key) {
                x = update[i]->next[i];
                break;
            }
        }
        return x;
    }

bool Remove(int key) {
    std::vector<std::shared_ptr<Node> > update = Search(key);
    std::shared_ptr<Node> x = head;
    bool found = false;

    // 从最高层开始向下层遍历，找到每一层中指向要删除节点的前驱节点
    for (int i = maxLevel - 1; i >= 0; --i) {
        x = update[i];
        if (x->next[i] && x->next[i]->key == key) {
            // 找到要删除的节点，更新前驱节点的指针
            x->next[i] = x->next[i]->next[i];
            found = true; // 标记找到要删除的节点
        } else {
            // 如果当前层没有找到，则退出循环，因为下面的层也不会有
            continue;
        }
    }

    // 如果没有找到要删除的节点，则返回false
    return found;
}
};

class KVDatabase {
private:
    SkipList skipList;

public:
    void cout_all_elm() {
        this->skipList.Traverse();
    }
    SkipList get_sk(){
        return skipList;
    }
    bool Persist(const std::string& filename) {
        std::ofstream outFile(filename);
        if (!outFile) {
            return false; // 打开文件失败
        }

        // 遍历跳表中的所有键值对并写入文件
        auto node = skipList.head->next[0]; // 假设最底层的链表包含所有元素
        while (node) {
            outFile << node->key << " " << node->value << std::endl;
            node = node->next[0];
        }

        outFile.close();
        return true; // 写入成功
    }

    bool Recover(const std::string& filename) {
        std::ifstream inFile(filename);
        if (!inFile) {
            return false; // 打开文件失败
        }
        int key;
        std::string value;
        // 从文件中读取键值对并插入跳表
        while (inFile >> key && std::getline(inFile, value)) {
            skipList.Insert(key, value);
        }

        inFile.close();
        return true; // 恢复成功
    }

    KVDatabase(double p = 0.5, int maxLevel = 32) : skipList(p, maxLevel) {
        std::cout << "SkipList initialized" << std::endl;
        std::srand(std::time(nullptr));
    }

    ~KVDatabase() {
        // 自动资源释放，无需手动操作
    }

    std::string ExecuteCommand(const std::string& commandStr) {
        std::stringstream ss(commandStr);
        std::string action;
        int key;
        std::string value;

        // 读取操作类型
        ss >> action;
        std::string tem;
        // 根据操作类型执行不同的操作
        if (action == "set") {
            std::cout<<"执行set操作"<<std::endl;
            ss>>tem;
            key = std::stoi(tem);
            ss >> value;
            bool success = skipList.Insert(key, value);
             std::cout<<"执行set操作完毕"<<std::endl;
            return success ? "success" : "false";
        } else if (action == "get") {
            ss>>tem;
            key = stoi(tem);
            auto node = skipList.Find(key);
            if (node) {
                return node->value;
            } else {
                return "not found";
            }
        } else if (action == "delete") {
            ss>>tem;
            key = stoi(tem);
            std::cout<<"要删除:"<<key<<std::endl;
            bool success = skipList.Remove(key);
            std::cout<<"删除完成"<<std::endl;
            return success ? "success" : "false";
        } else {
            return "unknown command";
        }
        return "";
    }
};
#endif /* KVDATABASE_H */
