#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QJsonDocument>
#include <QJsonObject>

#include "database.h"

struct networkData /** * @brief 网络数据结构体，用于存储网络通信中的相关信息 */
{
    QString senderId; // /< 发送方ID，标识消息的发送者
    QString receiverId; // /< 接收方ID，标识消息的接收者
    QString content; // /< 消息内容，实际传输的数据
    QDateTime timestamp; // /< 时间戳，记录消息发送或接收的时间
};
class ChatServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit ChatServer(QObject *parent = nullptr);

protected:
    //这个函数是QTcpServer的虚函数，当有新的连接时会被调用，所以也需要重写它来处理新连接
    void incomingConnection(qintptr socketDescriptor) override;// 重写 incomingConnection 方法以处理新连接

private slots:
    void onClientDisconnected();// 处理客户端断开连接
    void onClientReadyRead();// 处理客户端发送的数据

private:
    // 存储所有已连接的客户端 socket
    QMap<QTcpSocket*,QString> m_socketToUserId ;//用socket映射UserId
    QMap<QString,QTcpSocket*>m_userIdToSocket ;//用userId映射socket
    Database m_database; // 数据库实例，用于存储用户信息和消息记录

    //函数
    //处理用户登录
    void typeUserLogin(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户注销
    void typeUserLogout(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户信息
    void typeUserInfo(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户状态
    void typeUserStatus(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户消息
    void typeUserMessage(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户注册
    void typeUserRegister(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户加载好友
    void typeUserLoadFriends(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户心跳
    void typeHeartbeat(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户添加好友
    void typeAddFriend(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户搜索好友
    void typeSearchFriendList(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户删除好友
    void typeDeleteFriend(const QJsonObject& obj,QTcpSocket* socket);
    //处理用户离线消息
    void offlineMessage(QTcpSocket* socket,const QString& userId);
    void sendAvatarToClient(const QString& userId, QTcpSocket* socket);
    void receiveAvatarFromClient(QTcpSocket* socket,const QJsonObject& obj);
};

#endif // CHATSERVER_H