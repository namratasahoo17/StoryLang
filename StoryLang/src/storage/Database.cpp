#include "Database.h"

namespace storylang {

// ------------------------------------------------------------- Statement
Statement::Statement(sqlite3* db, const std::string& sql) : db_(db) {
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK)
        throw std::runtime_error(std::string("SQL prepare failed: ") + sqlite3_errmsg(db) + "  [" +
                                 sql + "]");
}

Statement::~Statement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

Statement::Statement(Statement&& other) noexcept : db_(other.db_), stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_) sqlite3_finalize(stmt_);
        db_ = other.db_;
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

void Statement::check(int rc) const {
    if (rc != SQLITE_OK) throw std::runtime_error(std::string("SQL error: ") + sqlite3_errmsg(db_));
}

void Statement::bind(int index, int value) { check(sqlite3_bind_int(stmt_, index, value)); }

void Statement::bind(int index, const std::string& value) {
    check(sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()),
                            SQLITE_TRANSIENT));
}

void Statement::bindNull(int index) { check(sqlite3_bind_null(stmt_, index)); }

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw std::runtime_error(std::string("SQL step failed: ") + sqlite3_errmsg(db_));
}

int Statement::columnInt(int col) const { return sqlite3_column_int(stmt_, col); }

std::string Statement::columnText(int col) const {
    const unsigned char* p = sqlite3_column_text(stmt_, col);
    return p ? reinterpret_cast<const char*>(p) : "";
}

void Statement::reset() { sqlite3_reset(stmt_); }

// -------------------------------------------------------------- Database
Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Cannot open database '" + path + "': " + msg);
    }
    exec("PRAGMA foreign_keys = ON;");
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQL error: " + msg);
    }
}

long long Database::lastInsertId() const { return sqlite3_last_insert_rowid(db_); }
int Database::changes() const { return sqlite3_changes(db_); }

}  // namespace storylang
