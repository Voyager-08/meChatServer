#include "database.h"
#include <QSqlDatabase>
#include <QUuid>

Database::~Database()
{
    if (db.isOpen()) {
        db.close();
        qDebug() << "Database 连接" << m_connectionName << "closed in destructor";
    }
}

void Database::connect(const QString &dbName)
{
    // 生成唯一连接名（避免冲突）
    QString newConnectionName = "SQLITE_" + QUuid::createUuid().toString(QUuid::Id128).replace("{", "").replace("}", "").replace("-", "_");

    // 如果已有同名连接（理论上不会，但安全起见），先移除
    if (QSqlDatabase::contains(newConnectionName)) {
        QSqlDatabase::removeDatabase(newConnectionName);
    }

    // 创建新的 SQLite 连接
    db = QSqlDatabase::addDatabase("QSQLITE", newConnectionName);
    db.setDatabaseName(dbName); // dbName 是 .sqlite 文件路径

    if (!db.open()) {
        qCritical() << "SQLite database打开失败:" << dbName
                    << "Error:" << db.lastError().text();
        return ;
    }

    m_connectionName = newConnectionName;
    qInfo() << "SQLite database 打开成功:"
             << dbName << "(connection:" << m_connectionName << ")";
    return ;
}

void Database::close()
{
    if (db.isOpen()) {
        db.close();
        qInfo() << "SQLite database connection:" << m_connectionName << "closed";
    }
}

QString Database::connectionName() const
{
    return m_connectionName;
}