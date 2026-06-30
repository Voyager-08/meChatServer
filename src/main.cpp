#include <QCoreApplication>
#include <QDebug>
#include "chatserver.h"
#include "database.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    ChatServer server;// 创建服务器实例
    quint16 port = 6452; // 你可以改成任意端口

    if (!server.listen(QHostAddress::Any, port)) {
        qInfo() << "服务开启失败,错误原因:" << server.errorString();
        return -1;
    }

    qInfo() << "服务开始监听（端口）:" << port;

    return app.exec();
}