#pragma once
#include <sqlite3.h>

#include <stdexcept>
#include <string>

namespace storylang {

// RAII wrapper around sqlite3_stmt*. Move-only. Parameter indices are 1-based.
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    void bind(int index, int value);
    void bind(int index, const std::string& value);
    void bindNull(int index);

    bool step();  // true = a row is available, false = finished
    int columnInt(int col) const;
    std::string columnText(int col) const;
    void reset();

private:
    void check(int rc) const;
    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

// RAII wrapper around sqlite3*.
class Database {
public:
    explicit Database(const std::string& path);  // ":memory:" works for tests
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void exec(const std::string& sql);
    Statement prepare(const std::string& sql) { return Statement(db_, sql); }
    sqlite3* handle() const { return db_; }
    long long lastInsertId() const;
    int changes() const;

private:
    sqlite3* db_ = nullptr;
};

// Rolls back automatically unless commit() is called.
class Transaction {
public:
    explicit Transaction(Database& db) : db_(db) { db_.exec("BEGIN"); }
    ~Transaction() {
        if (!done_) {
            try { db_.exec("ROLLBACK"); } catch (...) {}
        }
    }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    void commit() { db_.exec("COMMIT"); done_ = true; }

private:
    Database& db_;
    bool done_ = false;
};

}  // namespace storylang
