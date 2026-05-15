/////////////////////////////////////////////////////////
// File: SQLiteManagerTests.cpp
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Tests SQLiteManager database helpers
/////////////////////////////////////////////////////////

#include "../src/database/SQLiteManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class SQLiteManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void createDatabase_opensConnectionAndCreatesFile();
    void createTable_insertAndSelect_roundTripsValues();
    void updateAndRemove_applyWhereBindings();
    void transactionRollback_discardsInsertedRows();
    void dropTable_removesExistingTable();
    void invalidQuery_setsLastErrorAndEmitsSignal();

    // Connection
    void openDatabase_alreadyOpen_returnsTrueWithoutReopening();
    void openDatabase_invalidPath_returnsFalseAndSetsError();
    void isDatabaseOpen_afterClose_returnsFalse();

    // File management
    void createDatabase_nestedDirectories_createsPathAndFile();
    void deleteDatabase_removesWalAndShmSideFiles();
    void databaseFileExists_missingFile_returnsFalse();

    // CRUD edge cases
    void selectAll_multipleRows_returnsAllInInsertOrder();
    void selectWhere_noMatch_returnsEmptyList();
    void selectFirst_noMatch_returnsEmptyMap();
    void selectWhere_withColumnFilter_returnsOnlyRequestedColumns();
    void count_withAndWithoutWhere();
    void insert_emptyMap_returnsFalse();
    void update_withoutWhere_updatesAllRows();

    // Transactions
    void transactionCommit_persistsInsertedRows();
    void transaction_nestedBegin_returnsFalse();

    // Schema
    void createTable_idempotent_secondCallSucceeds();
    void tableExists_nonExistentTable_returnsFalse();
    void executeSql_createIndex_succeeds();
    void foreignKey_insertOrphanRow_fails();

private:
    QString dbPath(QTemporaryDir &d, const QString &f = "test.sqlite") const;
    bool openedDb(SQLiteManager &m, const QString &conn, QTemporaryDir &d) const;
    bool makePeople(SQLiteManager &m, const QString &conn) const;
    QVariantMap person(const QString &name, int age) const;
};

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::initTestCase()
{
    qputenv("QT_LOGGING_RULES", "*.debug=true");
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::createDatabase_opensConnectionAndCreatesFile()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    QSignalSpy statusSpy(&m, &SQLiteManager::signalConnectionStatusChanged);
    const QString conn = "create_connection";
    const QString path = dbPath(d);

    QVERIFY(m.createDatabase(conn, path));
    QVERIFY(m.isDatabaseOpen(conn));
    QVERIFY(m.databaseFileExists(path));

    m.closeDatabase(conn);
    QVERIFY(!m.isDatabaseOpen(conn));

    QCOMPARE(statusSpy.count(), 2);
    QCOMPARE(statusSpy.at(0).at(0).toBool(), true);
    QCOMPARE(statusSpy.at(0).at(1).toString(), conn);
    QCOMPARE(statusSpy.at(1).at(0).toBool(), false);
    QCOMPARE(statusSpy.at(1).at(1).toString(), conn);

    QVERIFY(m.deleteDatabase(conn, path));
    QVERIFY(!QFileInfo::exists(path));
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::createTable_insertAndSelect_roundTripsValues()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "crud_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));
    QVERIFY(m.tableExists(conn, "people"));

    QVERIFY(m.insert(conn, "people", person("Ada", 36)));
    QCOMPARE(m.count(conn, "people"), 1);

    const QVariantList rows = m.selectAll(conn, "people", {"name", "age"});
    QCOMPARE(rows.size(), 1);
    const QVariantMap row = rows.first().toMap();
    QCOMPARE(row.value("name").toString(), QString("Ada"));
    QCOMPARE(row.value("age").toInt(), 36);

    const QVariantMap first = m.selectFirst(conn, "people", "name = ?", {"Ada"});
    QCOMPARE(first.value("name").toString(), QString("Ada"));
    QCOMPARE(first.value("age").toInt(), 36);
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::updateAndRemove_applyWhereBindings()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "update_remove_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));

    QVERIFY(m.insert(conn, "people", person("Ada", 36)));
    QVERIFY(m.insert(conn, "people", person("Grace", 85)));

    QVERIFY(m.update(conn, "people", {{"age", 37}}, "name = ?", {"Ada"}));
    QCOMPARE(m.selectFirst(conn, "people", "name = ?", {"Ada"}).value("age").toInt(), 37);
    QCOMPARE(m.selectFirst(conn, "people", "name = ?", {"Grace"}).value("age").toInt(), 85);

    QVERIFY(m.remove(conn, "people", "name = ?", {"Grace"}));
    QCOMPARE(m.count(conn, "people"), 1);
    QCOMPARE(m.count(conn, "people", "name = ?", {"Grace"}), 0);
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::transactionRollback_discardsInsertedRows()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "transaction_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));

    QVERIFY(m.beginTransaction(conn));
    QVERIFY(m.insert(conn, "people", person("Linus", 56)));
    QCOMPARE(m.count(conn, "people"), 1);
    QVERIFY(m.rollbackTransaction(conn));
    QCOMPARE(m.count(conn, "people"), 0);
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::dropTable_removesExistingTable()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "drop_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));
    QVERIFY(m.tableExists(conn, "people"));
    QVERIFY(m.dropTable(conn, "people"));
    QVERIFY(!m.tableExists(conn, "people"));
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::invalidQuery_setsLastErrorAndEmitsSignal()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    QSignalSpy errorSpy(&m, &SQLiteManager::signalDatabaseError);
    const QString conn = "error_connection";
    QVERIFY(openedDb(m, conn, d));

    QVERIFY(!m.executeSql(conn, "SELECT * FROM missing_table"));

    const QString captured = errorSpy.first().first().toString();
    QVERIFY(!captured.isEmpty());
    QVERIFY(captured.contains("executeSql:"));
    QVERIFY(captured.contains("missing_table"));
    QCOMPARE(m.lastError(), captured);
    QCOMPARE(errorSpy.count(), 1);
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::openDatabase_alreadyOpen_returnsTrueWithoutReopening()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    QSignalSpy statusSpy(&m, &SQLiteManager::signalConnectionStatusChanged);
    const QString conn = "reopen_connection";

    QVERIFY(m.openDatabase(conn, dbPath(d)));
    QVERIFY(m.openDatabase(conn, dbPath(d))); // second call - already open
    // Signal must have fired exactly once (no reconnect)
    QCOMPARE(statusSpy.count(), 1);
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::openDatabase_invalidPath_returnsFalseAndSetsError()
{
    SQLiteManager m;
    QSignalSpy errorSpy(&m, &SQLiteManager::signalDatabaseError);

    // A path into a non-creatable location (root-owned directory on Linux/Windows)
    const bool ok = m.openDatabase("bad", "/no_such_root_dir/sub/bad.db");
    QVERIFY(!ok);
    QVERIFY(!m.lastError().isEmpty());
    QCOMPARE(errorSpy.count(), 1);
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::isDatabaseOpen_afterClose_returnsFalse()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "open_close_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(m.isDatabaseOpen(conn));
    m.closeDatabase(conn);
    QVERIFY(!m.isDatabaseOpen(conn));
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::createDatabase_nestedDirectories_createsPathAndFile()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString nested = QDir(d.path()).filePath("a/b/c/deep.sqlite");

    QVERIFY(m.createDatabase("deep", nested));
    QVERIFY(m.isDatabaseOpen("deep"));
    QVERIFY(QFileInfo::exists(nested));
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::deleteDatabase_removesWalAndShmSideFiles()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "wal_connection";
    const QString path = dbPath(d, "wal.sqlite");

    QVERIFY(m.createDatabase(conn, path));
    // A write forces WAL file creation
    QVERIFY(m.executeSql(conn, "CREATE TABLE t (x INTEGER)"));
    QVERIFY(m.executeSql(conn, "INSERT INTO t VALUES (1)"));

    QVERIFY(m.deleteDatabase(conn, path));
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(!QFileInfo::exists(path + "-wal"));
    QVERIFY(!QFileInfo::exists(path + "-shm"));
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::databaseFileExists_missingFile_returnsFalse()
{
    SQLiteManager m;
    QVERIFY(!m.databaseFileExists("/tmp/this_file_should_not_exist_lymalink.sqlite"));
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::selectAll_multipleRows_returnsAllInInsertOrder()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "multi_row_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));

    QVERIFY(m.insert(conn, "people", person("Ada", 36)));
    QVERIFY(m.insert(conn, "people", person("Grace", 85)));
    QVERIFY(m.insert(conn, "people", person("Linus", 56)));

    const QVariantList rows = m.selectAll(conn, "people");
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).toMap().value("name").toString(), QString("Ada"));
    QCOMPARE(rows.at(1).toMap().value("name").toString(), QString("Grace"));
    QCOMPARE(rows.at(2).toMap().value("name").toString(), QString("Linus"));
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::selectWhere_noMatch_returnsEmptyList()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "no_match_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));
    QVERIFY(m.insert(conn, "people", person("Ada", 36)));

    const QVariantList rows = m.selectWhere(conn, "people", "name = ?", {"Nobody"});
    QVERIFY(rows.isEmpty());
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::selectFirst_noMatch_returnsEmptyMap()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "first_empty_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));

    const QVariantMap result = m.selectFirst(conn, "people", "name = ?", {"Ghost"});
    QVERIFY(result.isEmpty());
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::selectWhere_withColumnFilter_returnsOnlyRequestedColumns()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "col_filter_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));

    QVERIFY(m.insert(conn, "people", person("Ada", 36)));
    QVERIFY(m.insert(conn, "people", person("Grace", 85)));

    const QVariantList rows = m.selectWhere(conn, "people", "age > ?", {50}, {"name"});

    QCOMPARE(rows.size(), 1);
    const QVariantMap row = rows.first().toMap();
    QCOMPARE(row.value("name").toString(), QString("Grace"));

    QVERIFY(!row.contains("id"));
    QVERIFY(!row.contains("age"));
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::count_withAndWithoutWhere()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "count_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));

    QVERIFY(m.insert(conn, "people", person("Ada", 36)));
    QVERIFY(m.insert(conn, "people", person("Grace", 85)));
    QVERIFY(m.insert(conn, "people", person("Linus", 56)));

    QCOMPARE(m.count(conn, "people"), 3);
    QCOMPARE(m.count(conn, "people", "age > ?", {50}), 2);
    QCOMPARE(m.count(conn, "people", "age > ?", {90}), 0);
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::insert_emptyMap_returnsFalse()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "empty_insert_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));

    QVERIFY(!m.insert(conn, "people", {}));
    QVERIFY(!m.lastError().isEmpty());
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::update_withoutWhere_updatesAllRows()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "update_all_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));

    QVERIFY(m.insert(conn, "people", person("Ada", 36)));
    QVERIFY(m.insert(conn, "people", person("Grace", 85)));

    // Empty whereClause - must update every row
    QVERIFY(m.update(conn, "people", {{"age", 0}}, {}));
    QCOMPARE(m.count(conn, "people", "age = ?", {0}), 2);
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::transactionCommit_persistsInsertedRows()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "commit_connection";
    const QString path = dbPath(d);
    QVERIFY(m.createDatabase(conn, path));
    QVERIFY(makePeople(m, conn));

    QVERIFY(m.beginTransaction(conn));
    QVERIFY(m.insert(conn, "people", person("Ada", 36)));
    QVERIFY(m.commitTransaction(conn));

    // Re-open to confirm persistence
    m.closeDatabase(conn);
    QVERIFY(m.openDatabase(conn, path));
    QCOMPARE(m.count(conn, "people"), 1);
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::transaction_nestedBegin_returnsFalse()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "nested_tx_connection";
    QVERIFY(openedDb(m, conn, d));

    QVERIFY(m.beginTransaction(conn));
    // SQLite does not support nested transactions - second begin must fail
    QVERIFY(!m.beginTransaction(conn));
    m.rollbackTransaction(conn); // clean up
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::createTable_idempotent_secondCallSucceeds()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "idempotent_connection";
    QVERIFY(openedDb(m, conn, d));

    QVERIFY(makePeople(m, conn));
    QVERIFY(makePeople(m, conn)); // IF NOT EXISTS - must not fail
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::tableExists_nonExistentTable_returnsFalse()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "noexist_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(!m.tableExists(conn, "ghost_table"));
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::executeSql_createIndex_succeeds()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "index_connection";
    QVERIFY(openedDb(m, conn, d));
    QVERIFY(makePeople(m, conn));

    QVERIFY(m.executeSql(conn, "CREATE INDEX IF NOT EXISTS idx_people_name ON people(name)"));
}

/////////////////////////////////////////////////////////////////////

void SQLiteManagerTests::foreignKey_insertOrphanRow_fails()
{
    QTemporaryDir d; QVERIFY(d.isValid());
    SQLiteManager m;
    const QString conn = "fk_connection";
    QVERIFY(openedDb(m, conn, d));

    // Parent table
    QVERIFY(m.createTable(conn, "departments", {
        "id   INTEGER PRIMARY KEY",
        "name TEXT NOT NULL"
    }));

    // Child table with FK constraint
    QVERIFY(m.createTable(conn, "employees", {
        "id            INTEGER PRIMARY KEY AUTOINCREMENT",
        "name          TEXT NOT NULL",
        "department_id INTEGER NOT NULL REFERENCES departments(id)"
    }));

    // Valid insert - parent row exists
    QVERIFY(m.insert(conn, "departments", {{"id", 1}, {"name", "Engineering"}}));
    QVERIFY(m.insert(conn, "employees", {{"name", "Ada"}, {"department_id", 1}}));
    QCOMPARE(m.count(conn, "employees"), 1);

    // Invalid insert - department 99 does not exist
    // PRAGMA foreign_keys=ON is set in openDatabase(), so this must fail
    QVERIFY(!m.insert(conn, "employees", {{"name", "Ghost"}, {"department_id", 99}}));
    QVERIFY(!m.lastError().isEmpty());

    // Row count must remain 1 - the orphan was rejected
    QCOMPARE(m.count(conn, "employees"), 1);
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QString SQLiteManagerTests::dbPath(QTemporaryDir &d, const QString &f) const
{
    return QDir(d.path()).filePath(f);
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManagerTests::openedDb(SQLiteManager &m, const QString &conn, QTemporaryDir &d) const
{
    return m.createDatabase(conn, dbPath(d));
}

/////////////////////////////////////////////////////////////////////

bool SQLiteManagerTests::makePeople(SQLiteManager &m, const QString &conn) const
{
    return m.createTable(conn, "people", {
        "id   INTEGER PRIMARY KEY AUTOINCREMENT",
        "name TEXT    NOT NULL",
        "age  INTEGER NOT NULL"
    });
}

/////////////////////////////////////////////////////////////////////

QVariantMap SQLiteManagerTests::person(const QString &name, int age) const
{
    return {{"name", name}, {"age", age}};
}

/////////////////////////////////////////////////////////////////////

QTEST_MAIN(SQLiteManagerTests)
#include "SQLiteManagerTests.moc"