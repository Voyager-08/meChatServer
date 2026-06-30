# meChatServer

一个基于 Qt5 的 TCP 聊天服务器，支持用户注册、登录、好友管理、实时消息收发等功能。

## 功能特性

- **用户管理**
  - 用户注册与登录验证
  - 用户信息管理（昵称、邮箱、个性签名、性别等）
  - 用户状态管理（在线/离线）
  - 用户头像上传与下载

- **好友系统**
  - 添加好友
  - 删除好友
  - 搜索好友（支持按用户ID或昵称模糊搜索）
  - 好友列表加载
  - 好友备注功能

- **消息系统**
  - 实时私聊消息收发
  - 离线消息存储与推送
  - 消息持久化（SQLite）
  - 支持多种消息类型（文本等）

- **连接管理**
  - 心跳检测机制
  - 自动清理断开连接
  - 多客户端并发支持

## 技术栈

- **语言**: C++17
- **框架**: Qt 5 (Core, Network, Sql)
- **数据库**: SQLite
- **构建工具**: CMake 3.16+
- **通信协议**: TCP + JSON

## 项目结构

```
meChatServer/
├── src/
│   ├── main.cpp           # 程序入口
│   ├── chatserver.h       # 聊天服务器头文件
│   ├── chatserver.cpp     # 聊天服务器实现
│   ├── database.h         # 数据库类头文件
│   └── database.cpp       # 数据库类实现
├── CMakeLists.txt         # CMake 配置文件
├── make.sh                # Linux 自动化编译脚本
└── README.md              # 项目说明文档
```

## 构建与运行

### 环境要求

- Qt 5.x（包含 Core、Network、Sql 模块）
- CMake 3.16 或更高版本
- C++17 兼容的编译器

### Linux 环境

使用提供的自动化编译脚本：

```bash
chmod +x make.sh
./make.sh
```

脚本会自动完成以下步骤：
1. 清理旧的构建目录
2. 创建新的构建目录
3. 运行 CMake 配置
4. 编译项目
5. 启动服务器

### 手动构建

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./meChatServer
```

### Windows 环境

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
.\Release\meChatServer.exe
```

## 配置说明

### 端口配置

默认监听端口为 **6452**，可在 `src/main.cpp` 中修改：

```cpp
quint16 port = 6452; // 修改为所需端口
```

### 数据库配置

数据库文件路径在 `src/chatserver.cpp` 构造函数中配置：

```cpp
m_database.connect("../sql/sqlite/mechat.sqlite");
```

确保数据库文件存在且包含以下表结构：

#### 数据表结构

**users 表**
```sql
CREATE TABLE users (
    user_Id TEXT PRIMARY KEY,
    user_Nick TEXT,
    password TEXT,
    email TEXT,
    created_at TEXT,
    motto TEXT,
    avatar_path TEXT,
    sex TEXT
);
```

**friendships 表**
```sql
CREATE TABLE friendships (
    user_Id TEXT,
    friend_Id TEXT,
    friend_note TEXT,
    PRIMARY KEY (user_Id, friend_Id)
);
```

**messages 表**
```sql
CREATE TABLE messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sender_id TEXT,
    receiver_id TEXT,
    comment TEXT,
    message_type TEXT,
    datetime TEXT
);
```

**offline_messages 表**
```sql
CREATE TABLE offline_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    message_uuid TEXT,
    sender_id TEXT,
    receiver_id TEXT,
    content TEXT,
    message_type TEXT,
    datetime TEXT
);
```

## 通信协议

客户端与服务器通过 TCP 连接通信，数据格式为 JSON，每条消息以换行符 `\n` 结尾。

### 消息类型

#### 1. 用户注册
**客户端发送:**
```json
{
  "type": "register",
  "userId": "用户ID",
  "userNick": "用户昵称",
  "password": "密码"
}
```

**服务器响应:**
```json
{
  "type": "register_result",
  "result": true,
  "userId": "用户ID"
}
```

#### 2. 用户登录
**客户端发送:**
```json
{
  "type": "login",
  "userId": "用户ID",
  "password": "密码"
}
```

**服务器响应:**
```json
{
  "type": "login_result",
  "result": true,
  "userId": "用户ID"
}
```

#### 3. 获取用户信息
**客户端发送:**
```json
{
  "type": "user_info",
  "userId": "用户ID"
}
```

**服务器响应:**
```json
{
  "type": "user_info",
  "user_nick": "昵称",
  "user_email": "邮箱",
  "user_registration_date": "注册日期",
  "user_motto": "个性签名",
  "user_sex": "性别",
  "user_status": "online"
}
```

#### 4. 发送消息
**客户端发送:**
```json
{
  "type": "message",
  "sender": "发送者ID",
  "receiver": "接收者ID",
  "content": "消息内容",
  "messageType": "text",
  "timestamp": "2026-06-30 12:00:00"
}
```

#### 5. 加载好友列表
**客户端发送:**
```json
{
  "type": "user_friends",
  "userId": "用户ID"
}
```

**服务器响应:**
```json
{
  "type": "friend_list",
  "userId": "用户ID",
  "friendList": [
    {
      "friendId": "好友ID",
      "friendNote": "备注",
      "friendNick": "昵称",
      "motto": "个性签名",
      "email": "邮箱"
    }
  ]
}
```

#### 6. 添加好友
**客户端发送:**
```json
{
  "type": "add_friend",
  "friendId": "好友ID"
}
```

**服务器响应:**
```json
{
  "type": "add_friend_result",
  "result": true,
  "friendId": "好友ID",
  "friendNick": "好友昵称"
}
```

#### 7. 删除好友
**客户端发送:**
```json
{
  "type": "delete_friend",
  "friendId": "好友ID"
}
```

**服务器响应:**
```json
{
  "type": "delete_friend_result",
  "result": true,
  "friendId": "好友ID"
}
```

#### 8. 搜索好友
**客户端发送:**
```json
{
  "type": "search_friend_list",
  "friendStr": "搜索关键词"
}
```

**服务器响应:**
```json
{
  "type": "search_friend_list",
  "result": true,
  "friendList": [
    {
      "friendId": "用户ID",
      "friendNick": "昵称"
    }
  ],
  "count": 1
}
```

#### 9. 心跳检测
**客户端发送:**
```json
{
  "type": "heartbeat",
  "user_id": "用户ID"
}
```

**服务器响应:**
```json
{
  "type": "heartbeat_response",
  "user_id": "用户ID",
  "timestamp": "2026-06-30T12:00:00"
}
```

#### 10. 用户注销
**客户端发送:**
```json
{
  "type": "logout",
  "userId": "用户ID"
}
```

#### 11. 头像传输
**客户端发送头像:**
```json
{
  "type": "avatar_file",
  "userId": "用户ID",
  "fileData": "Base64编码的图片数据"
}
```

**服务器发送头像:**
```json
{
  "type": "avatar_data",
  "userId": "用户ID",
  "avatarData": "Base64编码的图片数据"
}
```

#### 12. 离线消息
**服务器推送:**
```json
{
  "type": "离线消息",
  "sender": "发送者ID",
  "receiver": "接收者ID",
  "content": "消息内容",
  "messageType": "text",
“timestamp”: “2026-06-30 12:00:00”
}
```

## 头像存储

用户头像文件存储在以下路径：
- 接收头像：`./images/avatar/{userId}.png`
- 发送头像：`/home/voyager/build/images/avatar/{userId}.png`

可根据实际部署环境修改 `chatserver.cpp` 中的路径配置。

## 日志输出

服务器运行时会输出详细的日志信息，包括：
- 客户端连接/断开
- 用户登录/注销
- 消息收发记录
- 数据库操作结果
- 错误信息

## 许可证

本项目仅供学习和参考使用。

## 贡献

欢迎提交 Issue 和 Pull Request！
