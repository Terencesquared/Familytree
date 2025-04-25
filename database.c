/* database.c - Database functions for family tree application */

#include "family_tree.h"

int init_database(sqlite3 **db) {
    int rc = sqlite3_open("family_tree.db", db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        return 1;
    }
    return 0;
}

int create_tables(sqlite3 *db) {
    const char *people_sql = 
        "CREATE TABLE IF NOT EXISTS people ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "first_name TEXT NOT NULL,"
        "last_name TEXT NOT NULL,"
        "gender TEXT CHECK(gender IN ('M', 'F')),"
        "birth_date TEXT,"
        "death_date TEXT,"
        "bio TEXT,"
        "photo_url TEXT,"
        "created_at INTEGER NOT NULL,"
        "updated_at INTEGER NOT NULL"
        ");";
    
    const char *relationships_sql = 
        "CREATE TABLE IF NOT EXISTS relationships ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "person1_id INTEGER NOT NULL,"
        "person2_id INTEGER NOT NULL,"
        "relationship_type TEXT NOT NULL CHECK(relationship_type IN ('parent-child', 'spouse')),"
        "marriage_date TEXT,"
        "divorce_date TEXT,"
        "created_at INTEGER NOT NULL,"
        "updated_at INTEGER NOT NULL,"
        "FOREIGN KEY (person1_id) REFERENCES people (id),"
        "FOREIGN KEY (person2_id) REFERENCES people (id)"
        ");";
    
    const char *users_sql = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL UNIQUE,"
        "password_hash TEXT NOT NULL,"
        "person_id INTEGER,"
        "is_admin INTEGER DEFAULT 0,"
        "created_at INTEGER NOT NULL,"
        "updated_at INTEGER NOT NULL,"
        "FOREIGN KEY (person_id) REFERENCES people (id)"
        ");";
    
    char *error_msg = NULL;
    int rc;
    
    rc = sqlite3_exec(db, people_sql, NULL, NULL, &error_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", error_msg);
        sqlite3_free(error_msg);
        return 1;
    }
    
    rc = sqlite3_exec(db, relationships_sql, NULL, NULL, &error_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", error_msg);
        sqlite3_free(error_msg);
        return 1;
    }
    
    rc = sqlite3_exec(db, users_sql, NULL, NULL, &error_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", error_msg);
        sqlite3_free(error_msg);
        return 1;
    }
    
    return 0;
}

int add_person(sqlite3 *db, Person *person) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO people (first_name, last_name, gender, birth_date, death_date, bio, photo_url, created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    
    time_t now = time(NULL);
    person->created_at = now;
    person->updated_at = now;
    
    sqlite3_bind_text(stmt, 1, person->first_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, person->last_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, &person->gender, 1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, person->birth_date ? person->birth_date : NULL, -1, SQLITE_STATIC);
    
    sqlite3_bind_text(stmt, 5, person->death_date ? person->death_date : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, person->bio ? person->bio : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, person->photo_url ? person->photo_url : NULL, -1, SQLITE_STATIC);
    
    sqlite3_bind_int64(stmt, 8, person->created_at);
    sqlite3_bind_int64(stmt, 9, person->updated_at);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert person: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 1;
    }
    
    person->id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return 0;
}

int add_relationship(sqlite3 *db, Relationship *rel) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO relationships (person1_id, person2_id, relationship_type, marriage_date, divorce_date, created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?);";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    
    time_t now = time(NULL);
    rel->created_at = now;
    rel->updated_at = now;
    
    sqlite3_bind_int(stmt, 1, rel->person1_id);
    sqlite3_bind_int(stmt, 2, rel->person2_id);
    sqlite3_bind_text(stmt, 3, rel->relationship_type, -1, SQLITE_STATIC);
    
    sqlite3_bind_text(stmt, 4, rel->marriage_date ? rel->marriage_date : NULL, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, rel->divorce_date ? rel->divorce_date : NULL, -1, SQLITE_STATIC);
    
    sqlite3_bind_int64(stmt, 6, rel->created_at);
    sqlite3_bind_int64(stmt, 7, rel->updated_at);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert relationship: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return 1;
    }
    
    rel->id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return 0;
}

int get_children(sqlite3 *db, int parent_id, Person **children, int *count) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT p.* FROM people p "
                      "JOIN relationships r ON p.id = r.person2_id "
                      "WHERE r.person1_id = ? AND r.relationship_type = 'parent-child';";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    
    sqlite3_bind_int(stmt, 1, parent_id);
    
    // Count the children first
    *count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        (*count)++;
    }
    
    sqlite3_reset(stmt);
    
    if (*count > 0) {
        *children = (Person*)malloc(sizeof(Person) * (*count));
        if (!*children) {
            fprintf(stderr, "Memory allocation failed\n");
            sqlite3_finalize(stmt);
            return 1;
        }
        
        int i = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Person *child = &(*children)[i++];
            child->id = sqlite3_column_int(stmt, 0);
            child->first_name = strdup((const char*)sqlite3_column_text(stmt, 1));
            child->last_name = strdup((const char*)sqlite3_column_text(stmt, 2));
            child->gender = *((const char*)sqlite3_column_text(stmt, 3));
            
            child->birth_date = sqlite3_column_type(stmt, 4) != SQLITE_NULL ? strdup((const char*)sqlite3_column_text(stmt, 4)) : NULL;
            child->death_date = sqlite3_column_type(stmt, 5) != SQLITE_NULL ? strdup((const char*)sqlite3_column_text(stmt, 5)) : NULL;
            child->bio = sqlite3_column_type(stmt, 6) != SQLITE_NULL ? strdup((const char*)sqlite3_column_text(stmt, 6)) : NULL;
            child->photo_url = sqlite3_column_type(stmt, 7) != SQLITE_NULL ? strdup((const char*)sqlite3_column_text(stmt, 7)) : NULL;
            
            child->created_at = sqlite3_column_int64(stmt, 8);
            child->updated_at = sqlite3_column_int64(stmt, 9);
        }
    }
    
    sqlite3_finalize(stmt);
    return 0;
}