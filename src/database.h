#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlError>

class Database
{
public:
    ~Database();
    
    QSqlDatabase database() const { return db; } // 获取数据库连接
    void connect(const QString &dbName);         // dbName 即数据库文件路径
    void close();
    QString connectionName() const;              // 返回当前连接名

private:
    QSqlDatabase db;
    QString m_connectionName; // 实际使用的连接名称（内部生成）
};

#endif // DATABASE_H