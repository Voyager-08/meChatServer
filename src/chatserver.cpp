#include "chatserver.h"
#include <QtGlobal>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QThread>
#include <QFile>
#include <QFileInfo>

ChatServer::ChatServer(QObject *parent)
    : QTcpServer(parent)
{
    m_database.connect("../sql/sqlite/mechat.sqlite"); // 连接到 SQLite 数据库文件
}

void ChatServer::incomingConnection(qintptr socketDescriptor)//每当有客户端连接就会触发
{
    QTcpSocket *socket = new QTcpSocket(this);
    socket->setSocketDescriptor(socketDescriptor);

    connect(socket, &QTcpSocket::disconnected, this, &ChatServer::onClientDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &ChatServer::onClientReadyRead);
    
    qInfo() << "新的连接建立:" << socketDescriptor;
}

void ChatServer::onClientDisconnected()//用户断开
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());//这里的sender就在incomingConnection中创建的对象
    if (!socket) return;

    // 清理用户绑定
    if (m_socketToUserId.contains(socket)) {
        QString userId = m_socketToUserId[socket];
        m_userIdToSocket.remove(userId);
        m_socketToUserId.remove(socket);
        qInfo() << "用户离线:" << userId;
    }
    qInfo() << "客户端断开连接:" << socket->socketDescriptor();
    socket->deleteLater();
}


void ChatServer::onClientReadyRead()//收到客户端发送的数据时触发
{
    qInfo() << "客户端发送数据:" << static_cast<QTcpSocket*>(sender())->socketDescriptor();
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    while (socket->canReadLine()) {
        QByteArray line = socket->readLine().trimmed();
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(line, &error);

        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            qInfo() << "无效JSON:" << line;
            return;
        }

        QJsonObject obj = doc.object();
        QString type = obj["type"].toString();

        // 处理登录（仅允许未登录的连接执行）
        if (type == "login")typeUserLogin(obj,socket);
        // 情况2：处理用户状态（必须已登录）
        else if (type == "user_status") typeUserStatus(obj,socket);
        // 情况3：处理私聊消息（必须已登录）
        else if (type == "message") typeUserMessage(obj,socket);
        // 情况4：用户注册
        else if(type == "register")typeUserRegister(obj,socket);
        // 情况5：用户信息初始化
        else if(type== "user_info")typeUserInfo(obj,socket);
        // 情况6：用户好友列表初始化
        else if(type == "user_friends") typeUserLoadFriends(obj,socket);
        // 情况7：处理心跳消息
        else if(type == "heartbeat") typeHeartbeat(obj,socket);
        // 情况8：添加好友
        else if(type == "add_friend") typeAddFriend(obj,socket);
        // 情况9：搜索好友列表
        else if(type == "search_friend_list") typeSearchFriendList(obj,socket);
        // 情况10：用户注销
        else if(type == "logout") typeUserLogout(obj,socket);
        // 情况11：接收头像数据
        else if(type == "avatar_file") receiveAvatarFromClient(socket,obj);
        // 情况12：删除好友
        else if(type == "delete_friend") typeDeleteFriend(obj,socket);
        // 未知类型
        else {
            qInfo() << "未知消息类型:" << type;
        }
    }
}

void ChatServer::typeUserLogin(const QJsonObject& obj,QTcpSocket* socket)
{//处理用户登录
    QString userId = obj["userId"].toString().trimmed();
    QString password = obj["password"].toString().trimmed();//.trimmed()去掉字符串前后的空格
    qInfo() << "用户登录请求(用户ID):" << userId;
    //这里实现查询账户密码操作，错误则返回，正确则继续运行，用sqlite数据库实现
    QSqlQuery query(m_database.database());
    query.prepare("SELECT password FROM users WHERE user_Id = :userId");
    query.bindValue(":userId", userId);
    
    if (!query.exec()) 
    {
        qWarning() << "Sqlite 查询失败:" << query.lastError().text();
        QJsonObject loginSignal;
        loginSignal["type"] = "login_result";
        loginSignal["reason"] = "Database error";
        loginSignal["result"] = false;
        socket->write(QJsonDocument(loginSignal).toJson(QJsonDocument::Compact) + "\n");
        return;
    }

    if (query.next()) 
    {
        QString storedPassword = query.value(0).toString();
        if (storedPassword != password) 
        {
            qInfo() << "登录失败: 密码错误，用户ID:" << userId;
            QJsonObject loginSignal;
            loginSignal["type"] = "login_result";
            loginSignal["reason"] = "账号或者密码错误";
            loginSignal["result"] = false;
            socket->write(QJsonDocument(loginSignal).toJson(QJsonDocument::Compact) + "\n");
            return;
        }
    }
    else 
    {
        qInfo() << "登录失败: 用户不存在，用户ID:" << userId;
        QJsonObject loginSignal;
        loginSignal["type"] = "login_result";
        loginSignal["reason"] = "用户不存在";
        loginSignal["result"] = false;
        socket->write(QJsonDocument(loginSignal).toJson(QJsonDocument::Compact) + "\n");
        return;
    }
    
    //绑定用户ID和socket（关键步骤）
    m_socketToUserId[socket] = userId;
    m_userIdToSocket[userId] = socket;

    // 回复登录成功
    QJsonObject loginSignal;
    loginSignal["type"] = "login_result";
    loginSignal["result"] = true;
    loginSignal["userId"] = userId;
    socket->write(QJsonDocument(loginSignal).toJson(QJsonDocument::Compact) + "\n");
    qInfo() << "用户登录成功:" << userId;
    socket->flush();//确保登录成功消息立即发送
    
    // 发送头像数据到客户端
    sendAvatarToClient(userId, socket);


}

void ChatServer::typeUserInfo(const QJsonObject& obj, QTcpSocket* socket)
{//处理用户信息初始化
    QString userId = obj["userId"].toString().trimmed();
    if (userId.isEmpty()) {
        qInfo() << "用户信息字段缺失";
        return;
    }
    qInfo() << "收到用户信息请求,用户ID:" << userId;
    
    QSqlQuery query(m_database.database());
    query.prepare("SELECT user_id, user_nick, email, created_at, motto, avatar_path, sex FROM users WHERE user_id = :userId");
    query.bindValue(":userId", userId);

    if (query.exec() && query.next()) { 
        QJsonObject userInfoSignal;
        userInfoSignal["type"] = "user_info";
        userInfoSignal["user_nick"] = query.value(1).toString(); // user_nick
        userInfoSignal["user_email"] = query.value(2).toString(); // email
        userInfoSignal["user_registration_date"] = query.value(3).toString(); // created_at
        userInfoSignal["user_motto"] = query.value(4).toString(); // motto
        userInfoSignal["user_sex"] = query.value(6).toString(); // sex
        userInfoSignal["user_status"] = "online";
        
        socket->write(QJsonDocument(userInfoSignal).toJson(QJsonDocument::Compact) + "\n");
    }
    else {
        qInfo() << "用户信息查询失败,用户ID:" << userId << ", 错误信息:" << query.lastError().text();
    }
}

void ChatServer::typeUserStatus(const QJsonObject& obj,QTcpSocket* socket)
{//处理用户状态更新
    if (!m_socketToUserId.contains(socket)) {
        qInfo() << "未登录用户尝试发送 user_status";
        socket->close();
        return;
    }

    QString userId = m_socketToUserId[socket];
    QString status = obj["status"].toString().trimmed();

    if (status.isEmpty()) {
        status = "online"; // 默认状态
    }
    // 广播状态变更（可选：包含 userId 而非依赖客户端传的 username）
    QJsonObject broadcast;
    broadcast["type"] = "user_status";
    broadcast["userId"] = userId;   // 使用服务端绑定的真实 ID
    broadcast["status"] = status;

    QByteArray data = QJsonDocument(broadcast).toJson(QJsonDocument::Compact) + "\n";

    // 广播给所有其他已登录用户
    for (auto it = m_userIdToSocket.constBegin(); it != m_userIdToSocket.constEnd(); ++it) {
        if (it.value() != socket) {
            it.value()->write(data);
        }
    }

    qInfo() << "用户状态更新:" << userId << "->" << status;
}

void ChatServer::typeUserMessage(const QJsonObject& obj,QTcpSocket* socket)
{//处理用户消息发送
    QString sender = obj["sender"].toString().trimmed();
    QString receiver = obj["receiver"].toString().trimmed();
    QString content = obj["content"].toString();
    QString messageType = obj["messageType"].toString();
    QString datetime = obj["timestamp"].toString(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    if (messageType.isEmpty()) {
        messageType = "text";
    }

    if (messageType.isEmpty()) {
        messageType = "text";
    }

    if (sender.isEmpty() || receiver.isEmpty() || content.isEmpty()) {
        qInfo() << "消息字段缺失";
        return;
    }

    qInfo() << "收到消息，发送者:" << sender << "→ 接收者:" << receiver<<"时间:"<<datetime;

    // 将消息保存到数据库
    QSqlQuery query(m_database.database());
    query.prepare("INSERT INTO messages (sender_id, receiver_id, comment, message_type, datetime) VALUES (:senderId, :receiverId, :content, :messageType, :datetime)");
    query.bindValue(":senderId", sender);
    query.bindValue(":receiverId", receiver);
    query.bindValue(":content", content);
    query.bindValue(":messageType", messageType);
    query.bindValue(":datetime", datetime);

    if (!query.exec()) {
        qWarning() << "保存消息到数据库失败:" << query.lastError().text();
    } else {
        qInfo() << "消息已保存到数据库，发送者:" << sender << "→ 接收者:" << receiver;
    }

    // 检查目标用户是否在线，避免对 nullptr 调用 write
    QTcpSocket *target = m_userIdToSocket.value(receiver, nullptr); // 使用 .value(key, defaultValue) 更安全
    if(target){
        target->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n");
    } else {
        qInfo() << "目标用户不在线:" << receiver;
        // 生成消息UUID
        QString messageUuid = QUuid::createUuid().toString();
        // 将消息存储到离线消息表
        QSqlQuery offlineQuery(m_database.database());
        offlineQuery.prepare("INSERT INTO offline_messages (message_uuid, sender_id, receiver_id, content, message_type, datetime) VALUES (:uuid, :senderId, :receiverId, :content, :messageType, :datetime)");
        offlineQuery.bindValue(":uuid", messageUuid);
        offlineQuery.bindValue(":senderId", sender);
        offlineQuery.bindValue(":receiverId", receiver);
        offlineQuery.bindValue(":content", content);
        offlineQuery.bindValue(":messageType", messageType);
        offlineQuery.bindValue(":datetime", datetime);

        if (!offlineQuery.exec()) {
            qWarning() << "保存离线消息失败:" << offlineQuery.lastError().text();
        } else {
            qInfo() << "离线消息已存储，发送者:" << sender << "→ 接收者:" << receiver;
        }
    }
}

void ChatServer::typeHeartbeat(const QJsonObject& obj, QTcpSocket* socket)
{//处理心跳消息
    QString userId = obj["user_id"].toString().trimmed();

    if (userId.isEmpty()) {
        qInfo() << "心跳消息字段缺失";
        return;
    }

    qInfo() << "收到心跳消息，用户ID:" << userId;

    // 构建心跳响应
    QJsonObject heartbeatResponse;
    heartbeatResponse["type"] = "heartbeat_response";
    heartbeatResponse["user_id"] = userId;
    heartbeatResponse["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    socket->write(QJsonDocument(heartbeatResponse).toJson(QJsonDocument::Compact) + "\n");
    qInfo() << "响应心跳消息给用户:" << userId;
}

void ChatServer::typeUserRegister(const QJsonObject& obj,QTcpSocket* socket)
{//处理用户注册
    QString userId = obj["userId"].toString().trimmed();
    QString userNick = obj["userNick"].toString().trimmed();
    QString password = obj["password"].toString().trimmed();
    if (userId.isEmpty() || userNick.isEmpty() || password.isEmpty()) {
         qInfo() << "注册字段缺失";
         QJsonObject registerSignal;
         registerSignal["type"] = "register_result";
         registerSignal["reason"] = "注册字段缺失";
         registerSignal["result"] = false;
         socket->write(QJsonDocument(registerSignal).toJson(QJsonDocument::Compact) + "\n");
         qInfo()<<"注册失败:注册字段缺失" ;
         return;
    }
    qInfo() << "收到注册请求,用户ID:" << userId << "，昵称:" << userNick << "，密码:" << password;
     
    // 在数据库中检查用户是否存在                        
    QSqlQuery query(m_database.database());
    query.prepare("SELECT user_Id FROM users WHERE user_Id = :userId");
    query.bindValue(":userId", userId);
    if (query.exec() && query.next()) {
        qInfo() << "注册失败: 用户已存在，用户ID:" << userId;
        QJsonObject registerSignal;
        registerSignal["type"] = "register_result";
        registerSignal["reason"] = "用户ID已存在";
        registerSignal["result"] = false;
        socket->write(QJsonDocument(registerSignal).toJson(QJsonDocument::Compact) + "\n");
        return;
    }
    // 写入sqlite数据库
    query.prepare("INSERT INTO users (user_Id, user_Nick, password) VALUES (:userId, :userNick, :password)");
    query.bindValue(":userId", userId);
    query.bindValue(":userNick", userNick);
    query.bindValue(":password", password);
    if (query.exec()) {
        QJsonObject registerSignal;
        registerSignal["type"] = "register_result";
        registerSignal["result"] = true;
        registerSignal["userId"] = userId;
        socket->write(QJsonDocument(registerSignal).toJson(QJsonDocument::Compact) + "\n");
        qInfo()<<"注册成功,用户ID:" << userId << "，昵称:" << userNick;
    }
    else {
        QJsonObject registerSignal;
        registerSignal["type"] = "register_result";
        registerSignal["reason"] = "操作无效";
        registerSignal["result"] = false;
        socket->write(QJsonDocument(registerSignal).toJson(QJsonDocument::Compact) + "\n");
        qInfo()<<"注册失败,用户ID:" << userId << "，昵称:" << userNick<<query.lastError().text();
        return;
    }
}

void ChatServer::typeUserLoadFriends(const QJsonObject& obj, QTcpSocket* socket)
{//处理用户加载好友列表
    
    QString userId = m_socketToUserId[socket];

    if (userId.isEmpty()) {
        qInfo() << "加载好友字段缺失";
        return;
    }
    qInfo() << "收到加载好友请求,用户ID:" << userId;
    
    // 1. 查询好友关系表，获取好友ID和备注
    QSqlQuery query(m_database.database());
    query.prepare("SELECT friend_Id, friend_note FROM friendships WHERE user_Id = :userId");
    query.bindValue(":userId", userId);

    QJsonArray friendListArray;

    if (query.exec()) {
        while (query.next()) {
            QString friendId = query.value(0).toString();
            QString friendNote = query.value(1).toString();
            
            // 2. 根据好友ID去用户表查询详细信息
            QSqlQuery userQuery(m_database.database());
            userQuery.prepare("SELECT user_nick, avatar_path, motto, email FROM users WHERE user_id = :friendId");
            userQuery.bindValue(":friendId", friendId);

            if(userQuery.exec() && userQuery.next())
            {
                // 获取从 users 表查到的信息
                QString friendNick = userQuery.value(0).toString(); // user_nick
                QString avatarPath = userQuery.value(1).toString(); // avatar_path
                QString motto = userQuery.value(2).toString();      // motto
                QString email = userQuery.value(3).toString();      // email

                qInfo() << "用户" << userId << "的好友: ID=" << friendId 
                        << ", 备注=" << friendNote 
                        << ", 昵称=" << friendNick;
                
                // 3. 将所有信息封装成一个JSON对象
                QJsonObject singleFriend;
                singleFriend["friendId"] = friendId;      // 好友ID
                singleFriend["friendNote"] = friendNote;  // 好友备注
                singleFriend["friendNick"] = friendNick;  // 好友昵称
                singleFriend["motto"] = motto;            // 个性签名
                singleFriend["email"] = email;            // 邮箱
                friendListArray.append(singleFriend);
                // 发送好友头像到客户端
                sendAvatarToClient(friendId, socket);
            } 
            else 
            {
                // 如果在 users 表中找不到该好友ID，说明数据不一致，可以选择跳过或返回默认信息
                qWarning() << "在 users 表中未找到好友ID:" << friendId << "，来自用户:" << userId;
                // 可选：为丢失的用户创建一个默认信息
                QJsonObject singleFriend;
                singleFriend["friendId"] = friendId;
                singleFriend["friendNote"] = friendNote;
                singleFriend["friendNick"] = "未知用户"; // 默认昵称
                singleFriend["motto"] = "该用户不存在或信息有误";
                singleFriend["email"] = "";
                friendListArray.append(singleFriend);

            }
        }
        
        // 构建并发送最终的回复 JSON
        QJsonObject friendSignal;
        friendSignal["type"] = "friend_list";
        friendSignal["userId"] = userId;
        friendSignal["friendList"] = friendListArray;
        
        qInfo() << "加载好友成功,用户ID:" << userId << "，好友数量:" << friendListArray.size();
        socket->write(QJsonDocument(friendSignal).toJson(QJsonDocument::Compact) + "\n");
    }
    else {
        qInfo() << "查询好友列表失败,用户ID:" << userId << ", 错误信息:" << query.lastError().text();
    }
    socket->flush();
    offlineMessage(socket,userId);
}

void ChatServer::offlineMessage(QTcpSocket* socket,const QString& userId)
{//处理离线消息发送
    QSqlQuery offlineQuery(m_database.database());
    offlineQuery.prepare("SELECT id, message_uuid, sender_id, content, message_type, datetime FROM offline_messages WHERE receiver_id = :userId ORDER BY datetime ASC");
    offlineQuery.bindValue(":userId", userId);

    if (offlineQuery.exec()) {
        int offlineMsgCount = 0;
        while (offlineQuery.next()) {
            int msgId = offlineQuery.value(0).toInt();
            QString messageUuid = offlineQuery.value(1).toString();
            QString senderId = offlineQuery.value(2).toString();
            QString content = offlineQuery.value(3).toString();
            QString messageType = offlineQuery.value(4).toString();
            QString datetime = offlineQuery.value(5).toString();

            // 构建离线消息
            QJsonObject offlineMsg;
            offlineMsg["type"] = "offline_message";
            offlineMsg["sender"] = senderId;
            offlineMsg["receiver"] = userId;
            offlineMsg["content"] = content;
            offlineMsg["messageType"] = messageType;
            offlineMsg["timestamp"] = datetime;

            socket->write(QJsonDocument(offlineMsg).toJson(QJsonDocument::Compact) + "\n");
            offlineMsgCount++;

            // 删除已发送的离线消息
            QSqlQuery deleteQuery(m_database.database());
            deleteQuery.prepare("DELETE FROM offline_messages WHERE id = :id");
            deleteQuery.bindValue(":id", msgId);
            if (!deleteQuery.exec()) {
                qWarning() << "删除离线消息失败:" << deleteQuery.lastError().text();
            }
        }
        if (offlineMsgCount > 0) {
            qInfo() << "已发送" << offlineMsgCount << "条离线消息给用户:" << userId;
        }
    } else {
        qWarning() << "查询离线消息失败:" << offlineQuery.lastError().text();
    }
}
void ChatServer::typeAddFriend(const QJsonObject& obj,QTcpSocket* socket)
{//处理用户添加好友
    QString userId = m_socketToUserId[socket];
    QString friendId = obj["friendId"].toString().trimmed();
    //直接在friendships表中添加，friendNick在users中的user_nick字段

    // 获取好友昵称作为备注
    QString friendNick;
    QSqlQuery query(m_database.database());
    query.prepare("SELECT user_nick FROM users WHERE user_id = :friendId");
    query.bindValue(":friendId", friendId);
    if (query.exec() && query.next()) {
        friendNick = query.value(0).toString();
    }
    else {
        qInfo() << "添加好友失败,用户ID:" << userId <<" 原因:"<< query.lastError().text();
        QJsonObject addFriendSignal;
        addFriendSignal["type"] = "add_friend_result";
        addFriendSignal["reason"] = "添加失败";
        addFriendSignal["result"] = false;
        socket->write(QJsonDocument(addFriendSignal).toJson(QJsonDocument::Compact) + "\n");
        return;
    }

    // 写入friendships表
    query.prepare("INSERT INTO friendships (user_Id, friend_Id, friend_note) VALUES (:userId, :friendId, :friendNick)");
    query.bindValue(":userId", userId);
    query.bindValue(":friendId", friendId);
    query.bindValue(":friendNick", friendNick);
    if (query.exec()) {
        QJsonObject addFriendSignal;
        addFriendSignal["type"] = "add_friend_result";
        addFriendSignal["result"] = true;
        addFriendSignal["friendId"] = friendId;
        addFriendSignal["friendNick"] = friendNick;
        socket->write(QJsonDocument(addFriendSignal).toJson(QJsonDocument::Compact) + "\n");
        qInfo() << "添加好友成功,用户ID:" << userId << "，好友ID:" << friendId << "，昵称:" << friendNick;
        sendAvatarToClient(friendId, socket);//发送好友头像到客户端
    }
    else {
        QJsonObject addFriendSignal;
        addFriendSignal["type"] = "add_friend_result";
        addFriendSignal["reason"] = query.lastError().text();
        addFriendSignal["result"] = false;
        socket->write(QJsonDocument(addFriendSignal).toJson(QJsonDocument::Compact) + "\n");
        qInfo() << "添加好友失败,用户ID:" << userId <<" 原因:"<< query.lastError().text();
    }
}
   
void ChatServer::typeSearchFriendList(const QJsonObject& obj, QTcpSocket* socket)
{//处理用户搜索好友
    QString userId = m_socketToUserId[socket];
    // 验证输入参数
    if (!obj.contains("friendStr")) {
        qWarning() << "搜索好友字段缺失";
        return;
    }
    
    QString friendStr = obj["friendStr"].toString().trimmed();
    if (friendStr.isEmpty()) {
        qWarning() << "搜索好友字段为空";
        return;
    }

    qInfo() << "收到搜索好友请求,用户搜索字段:" << friendStr;

    QSqlQuery query(m_database.database());
    QJsonArray friendListArray;

    QString queryString = QString(
        "SELECT DISTINCT user_id, user_nick FROM users "
        "WHERE (user_nick LIKE ? OR user_id LIKE ?) "  // 搜索条件
        "AND user_id != ? "                            // 排除自己
        "AND user_id NOT IN ( "                        // 排除好友
        "    SELECT friend_id FROM friendships "
        "    WHERE user_id = ? "
        ")"
    );
    
    query.prepare(queryString);
    // 4. 绑定参数 (注意顺序要与 SQL 中的 ? 一一对应)
    query.addBindValue("%" + friendStr + "%");  // 1. 搜索昵称
    query.addBindValue("%" + friendStr + "%");  // 2. 搜索ID
    query.addBindValue(userId);          // 3. 排除自己
    query.addBindValue(userId);          // 4. 子查询：查找我的好友列表
    
    if (!query.exec()) {
        qWarning() << "搜索好友查询失败:" << query.lastError().text();
        // 发送错误响应
        QJsonObject response;
        response["type"] = "search_friend_list_result";
        response["success"] = false;
        response["error"] = query.lastError().text();
        response["friendList"] = QJsonArray();
        socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + "\n");
    } 
    else {
        while (query.next()) {
            QString userId = query.value(0).toString();
            QString userNick = query.value(1).toString();
            qInfo() << "搜索到用户:" << userId << "，昵称:" << userNick;
            
            QJsonObject userInfo;
            userInfo["friendId"] = userId;
            userInfo["friendNick"] = userNick;
            friendListArray.append(userInfo);
            sendAvatarToClient(userId, socket);//发送用户头像到客户端
        }
        
        qInfo() << "搜索完成，找到" << friendListArray.size() << "个匹配的用户";
        
        // 发送结果给客户端
        QJsonObject response;
        response["type"] = "search_friend_list";
        response["result"] = true;
        response["friendList"] = friendListArray;
        response["count"] = friendListArray.size();
        socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + "\n");
    }
}

void ChatServer::sendAvatarToClient(const QString& userId, QTcpSocket* socket)
{//发送头像图片到客户端
    QString avatarPath = "/home/voyager/build/images/avatar/" + userId + ".png";
    QFile file(avatarPath);
    
    if (!file.exists()) {
        qInfo() << "头像文件不存在:" << avatarPath;
        return;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "打开头像文件失败:" << avatarPath;
        return;
    }
    
    QByteArray fileData = file.readAll();
    file.close();
    
    // 使用 Base64 编码图片数据，方便通过 JSON 传输
    QByteArray base64Data = fileData.toBase64();
    
    QJsonObject avatarMsg;
    avatarMsg["type"] = "avatar_data";
    avatarMsg["userId"] = userId;
    avatarMsg["avatarData"] = QString(base64Data);
    
    socket->write(QJsonDocument(avatarMsg).toJson(QJsonDocument::Compact) + "\n");
    qInfo() << "已发送头像数据给用户:" << userId << ", 大小:" << fileData.size() << "字节";
}

void ChatServer::typeUserLogout(const QJsonObject& obj,QTcpSocket* socket)
{//处理用户注销
    QString userId = m_socketToUserId[socket];
    m_socketToUserId.remove(socket);
    m_userIdToSocket.remove(userId);
    qInfo() << "用户注销,用户ID:" << userId;
}

void ChatServer::receiveAvatarFromClient(QTcpSocket* socket,const QJsonObject& obj)
{//接收头像数据
    QString userId = obj["userId"].toString();
    //base64解码
    QByteArray avatarData = QByteArray::fromBase64(obj["fileData"].toString().toUtf8());
    QString avatarPath = "./images/avatar/" + userId + ".png";
    if (avatarData.isEmpty()) {
        qInfo() << "头像数据字段缺失";
        return;
    }
    // 保存头像数据到文件
    QFile file(avatarPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "打开头像文件失败:" << avatarPath;
        return;
    }
    file.write(avatarData);
    file.close();
    qInfo() << "已保存头像数据到文件:" << avatarPath;
}

void ChatServer::typeDeleteFriend(const QJsonObject& obj,QTcpSocket* socket)
{//处理用户删除好友
    QString userId = m_socketToUserId[socket];
    QString friendId = obj["friendId"].toString().trimmed();
    //直接在friendships表中删除好友关系
    QSqlQuery query(m_database.database());
    query.prepare("DELETE FROM friendships WHERE user_Id = :userId AND friend_Id = :friendId");
    query.bindValue(":userId", userId);
    query.bindValue(":friendId", friendId);
    if (query.exec()) {
        //发送删除好友成功消息
        QJsonObject deleteFriendSignal;
        deleteFriendSignal["type"] = "delete_friend_result";
        deleteFriendSignal["friendId"] = friendId;
        deleteFriendSignal["result"] = true;
        socket->write(QJsonDocument(deleteFriendSignal).toJson(QJsonDocument::Compact) + "\n");
        qInfo() << "删除好友成功,用户ID:" << userId << ",好友ID:" << friendId;
    }
    else {
        QJsonObject deleteFriendSignal;
        deleteFriendSignal["type"] = "delete_friend_result";
        deleteFriendSignal["friendId"] = friendId;
        deleteFriendSignal["reason"] = query.lastError().text();
        deleteFriendSignal["result"] = false;
        socket->write(QJsonDocument(deleteFriendSignal).toJson(QJsonDocument::Compact) + "\n");
        qInfo() << "删除好友失败,用户ID:" << userId <<" 原因:"<< query.lastError().text();
    }
}