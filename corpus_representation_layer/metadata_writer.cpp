#include "metadata_writer.hpp"

#include <sqlite3.h>

#include <stdexcept>

namespace teknegram {

    namespace {
        void ExecuteSQL(sqlite3* db, const char* sql, const char* context) {
            char* err_msg = 0;
            if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
                const std::string err = err_msg ? err_msg : "unknown sqlite exec error";
                sqlite3_free(err_msg);
                throw std::runtime_error(std::string(context) + ": " + err);
            }
        }
    } // namespace

    MetadataWriter::MetadataWriter(const std::string& db_path)
        : db_path_(db_path) {
        sqlite3* db = 0;
        if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
            const std::string err = db ? sqlite3_errmsg(db) : "unknown sqlite open error";
            if (db) {
                sqlite3_close(db);
            }
            throw std::runtime_error("Failed to open metadata DB: " + err);
        }

        ExecuteSQL(db, "DROP TABLE IF EXISTS document_segment;", "Failed to reset metadata schema");
        ExecuteSQL(db, "DROP TABLE IF EXISTS folder_segment;", "Failed to reset metadata schema");
        ExecuteSQL(db, "DROP TABLE IF EXISTS document_group;", "Failed to reset metadata schema");
        ExecuteSQL(db, "DROP TABLE IF EXISTS semantic_value;", "Failed to reset metadata schema");
        ExecuteSQL(db, "DROP TABLE IF EXISTS semantic_key;", "Failed to reset metadata schema");
        ExecuteSQL(db, "DROP TABLE IF EXISTS document;", "Failed to reset metadata schema");
        ExecuteSQL(db, "DROP TABLE IF EXISTS subcorpus;", "Failed to reset metadata schema");

        const char* document_sql =
            "CREATE TABLE IF NOT EXISTS document ("
            "document_id INTEGER PRIMARY KEY,"
            "title TEXT NOT NULL,"
            "author TEXT NOT NULL,"
            "relative_path TEXT NOT NULL UNIQUE"
            ");";
        ExecuteSQL(db, document_sql, "Failed to ensure document schema");

        const char* folder_segment_sql =
            "CREATE TABLE IF NOT EXISTS folder_segment ("
            "segment_id INTEGER PRIMARY KEY,"
            "segment_text TEXT NOT NULL UNIQUE"
            ");";
        ExecuteSQL(db, folder_segment_sql, "Failed to ensure folder_segment schema");

        const char* document_segment_sql =
            "CREATE TABLE IF NOT EXISTS document_segment ("
            "document_id INTEGER NOT NULL,"
            "depth INTEGER NOT NULL,"
            "segment_id INTEGER NOT NULL,"
            "PRIMARY KEY(document_id, depth),"
            "FOREIGN KEY(document_id) REFERENCES document(document_id),"
            "FOREIGN KEY(segment_id) REFERENCES folder_segment(segment_id)"
            ");";
        ExecuteSQL(db, document_segment_sql, "Failed to ensure document_segment schema");

        const char* semantic_key_sql =
            "CREATE TABLE IF NOT EXISTS semantic_key ("
            "key_id INTEGER PRIMARY KEY,"
            "key_name TEXT NOT NULL UNIQUE"
            ");";
        ExecuteSQL(db, semantic_key_sql, "Failed to ensure semantic_key schema");

        const char* semantic_value_sql =
            "CREATE TABLE IF NOT EXISTS semantic_value ("
            "value_id INTEGER PRIMARY KEY,"
            "key_id INTEGER NOT NULL,"
            "value_text TEXT NOT NULL,"
            "UNIQUE(key_id, value_text),"
            "FOREIGN KEY(key_id) REFERENCES semantic_key(key_id)"
            ");";
        ExecuteSQL(db, semantic_value_sql, "Failed to ensure semantic_value schema");

        const char* document_group_sql =
            "CREATE TABLE IF NOT EXISTS document_group ("
            "document_id INTEGER NOT NULL,"
            "key_id INTEGER NOT NULL,"
            "value_id INTEGER NOT NULL,"
            "PRIMARY KEY(document_id, key_id),"
            "FOREIGN KEY(document_id) REFERENCES document(document_id),"
            "FOREIGN KEY(key_id) REFERENCES semantic_key(key_id),"
            "FOREIGN KEY(value_id) REFERENCES semantic_value(value_id)"
            ");";
        ExecuteSQL(db, document_group_sql, "Failed to ensure document_group schema");

        ExecuteSQL(db, "CREATE INDEX IF NOT EXISTS idx_document_relative_path ON document(relative_path);",
                   "Failed to ensure metadata indexes");
        ExecuteSQL(db, "CREATE INDEX IF NOT EXISTS idx_folder_segment_text ON folder_segment(segment_text);",
                   "Failed to ensure metadata indexes");
        ExecuteSQL(db, "CREATE INDEX IF NOT EXISTS idx_document_segment_depth_segment ON "
                       "document_segment(depth, segment_id, document_id);",
                   "Failed to ensure metadata indexes");
        ExecuteSQL(db, "CREATE INDEX IF NOT EXISTS idx_document_segment_document_id ON "
                       "document_segment(document_id, depth);",
                   "Failed to ensure metadata indexes");
        ExecuteSQL(db, "CREATE INDEX IF NOT EXISTS idx_semantic_key_name ON semantic_key(key_name);",
                   "Failed to ensure metadata indexes");
        ExecuteSQL(db, "CREATE INDEX IF NOT EXISTS idx_semantic_value_key_text ON "
                       "semantic_value(key_id, value_text);",
                   "Failed to ensure metadata indexes");
        ExecuteSQL(db, "CREATE INDEX IF NOT EXISTS idx_document_group_key_value_doc ON "
                       "document_group(key_id, value_id, document_id);",
                   "Failed to ensure metadata indexes");
        ExecuteSQL(db, "CREATE INDEX IF NOT EXISTS idx_document_group_document_id ON "
                       "document_group(document_id, key_id);",
                   "Failed to ensure metadata indexes");

        sqlite3_close(db);
    }

    void MetadataWriter::upsert_document(std::uint32_t document_id,
                                        const std::string& title,
                                        const std::string& author,
                                        const std::string& relative_path) {
        sqlite3* db = 0;
        if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
            const std::string err = db ? sqlite3_errmsg(db) : "unknown sqlite open error";
            if (db) {
                sqlite3_close(db);
            }
            throw std::runtime_error("Failed to open metadata DB: " + err);
        }

        const char* sql =
            "INSERT OR REPLACE INTO document(document_id, title, author, relative_path) "
            "VALUES(?1, ?2, ?3, ?4);";

        sqlite3_stmt* stmt = 0;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Failed to prepare metadata upsert: " + err);
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(document_id));
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, author.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, relative_path.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error("Failed to execute metadata upsert: " + err);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    void MetadataWriter::upsert_folder_segment(std::uint32_t segment_id,
                                               const std::string& segment_text) {
        sqlite3* db = 0;
        if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
            const std::string err = db ? sqlite3_errmsg(db) : "unknown sqlite open error";
            if (db) {
                sqlite3_close(db);
            }
            throw std::runtime_error("Failed to open metadata DB: " + err);
        }

        const char* sql =
            "INSERT OR REPLACE INTO folder_segment(segment_id, segment_text) "
            "VALUES(?1, ?2);";

        sqlite3_stmt* stmt = 0;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Failed to prepare folder segment upsert: " + err);
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(segment_id));
        sqlite3_bind_text(stmt, 2, segment_text.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error("Failed to execute folder segment upsert: " + err);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    void MetadataWriter::upsert_document_segment(std::uint32_t document_id,
                                                 std::uint32_t depth,
                                                 std::uint32_t segment_id) {
        sqlite3* db = 0;
        if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
            const std::string err = db ? sqlite3_errmsg(db) : "unknown sqlite open error";
            if (db) {
                sqlite3_close(db);
            }
            throw std::runtime_error("Failed to open metadata DB: " + err);
        }

        const char* sql =
            "INSERT OR REPLACE INTO document_segment(document_id, depth, segment_id) "
            "VALUES(?1, ?2, ?3);";

        sqlite3_stmt* stmt = 0;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Failed to prepare document segment upsert: " + err);
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(document_id));
        sqlite3_bind_int(stmt, 2, static_cast<int>(depth));
        sqlite3_bind_int(stmt, 3, static_cast<int>(segment_id));

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error("Failed to execute document segment upsert: " + err);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    void MetadataWriter::upsert_semantic_key(std::uint32_t key_id,
                                             const std::string& key_name) {
        sqlite3* db = 0;
        if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
            const std::string err = db ? sqlite3_errmsg(db) : "unknown sqlite open error";
            if (db) {
                sqlite3_close(db);
            }
            throw std::runtime_error("Failed to open metadata DB: " + err);
        }

        const char* sql =
            "INSERT OR REPLACE INTO semantic_key(key_id, key_name) "
            "VALUES(?1, ?2);";

        sqlite3_stmt* stmt = 0;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Failed to prepare semantic key upsert: " + err);
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(key_id));
        sqlite3_bind_text(stmt, 2, key_name.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error("Failed to execute semantic key upsert: " + err);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    void MetadataWriter::upsert_semantic_value(std::uint32_t value_id,
                                               std::uint32_t key_id,
                                               const std::string& value_text) {
        sqlite3* db = 0;
        if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
            const std::string err = db ? sqlite3_errmsg(db) : "unknown sqlite open error";
            if (db) {
                sqlite3_close(db);
            }
            throw std::runtime_error("Failed to open metadata DB: " + err);
        }

        const char* sql =
            "INSERT OR REPLACE INTO semantic_value(value_id, key_id, value_text) "
            "VALUES(?1, ?2, ?3);";

        sqlite3_stmt* stmt = 0;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Failed to prepare semantic value upsert: " + err);
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(value_id));
        sqlite3_bind_int(stmt, 2, static_cast<int>(key_id));
        sqlite3_bind_text(stmt, 3, value_text.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error("Failed to execute semantic value upsert: " + err);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    void MetadataWriter::upsert_document_group(std::uint32_t document_id,
                                               std::uint32_t key_id,
                                               std::uint32_t value_id) {
        sqlite3* db = 0;
        if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
            const std::string err = db ? sqlite3_errmsg(db) : "unknown sqlite open error";
            if (db) {
                sqlite3_close(db);
            }
            throw std::runtime_error("Failed to open metadata DB: " + err);
        }

        const char* sql =
            "INSERT OR REPLACE INTO document_group(document_id, key_id, value_id) "
            "VALUES(?1, ?2, ?3);";

        sqlite3_stmt* stmt = 0;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Failed to prepare document group upsert: " + err);
        }

        sqlite3_bind_int(stmt, 1, static_cast<int>(document_id));
        sqlite3_bind_int(stmt, 2, static_cast<int>(key_id));
        sqlite3_bind_int(stmt, 3, static_cast<int>(value_id));

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            const std::string err = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error("Failed to execute document group upsert: " + err);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

} // namespace teknegram
