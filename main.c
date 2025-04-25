/* main.c - Main entry point for family tree CGI application */

#include "family_tree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sqlite3.h>
#include <ctype.h>

// Structure definitions (moved from family_tree.h for completeness)

// Function declarations
void free_person(Person *person);
int get_person_by_id(sqlite3 *db, int id, Person *person);
int get_parents(sqlite3 *db, int child_id, Person *father, Person *mother);
int get_spouse(sqlite3 *db, int person_id, Person *spouse);
int update_person(sqlite3 *db, Person *person);
int add_person(sqlite3 *db, Person *person);
int add_relationship(sqlite3 *db, Relationship *rel);
char *html_escape(const char *str);
void print_html_header(const char *title);
void print_html_footer();
int init_database(sqlite3 **db);
int create_tables(sqlite3 *db);
void render_family_tree(sqlite3 *db, int root_id, int levels);
void render_person_profile(sqlite3 *db, int person_id);


CGIParams parse_query_string(const char *query_string) {
    CGIParams params = {NULL, 0};
    if (!query_string || !*query_string) {
        return params;
    }
    
    // Count parameters
    const char *p = query_string;
    params.count = 1;
    while (*p) {
        if (*p == '&') {
            params.count++;
        }
        p++;
    }
    
    // Allocate memory
    params.params = malloc(sizeof(CGIParam) * params.count);
    if (!params.params) {
        params.count = 0;
        return params;
    }
    
    // Parse parameters
    char *qs = strdup(query_string);
    char *token = strtok(qs, "&");
    int i = 0;
    
    while (token && i < params.count) {
        char *value = strchr(token, '=');
        if (value) {
            *value = '\0'; // Split name and value
            value++;
            
            // URL decode
            params.params[i].name = strdup(token);
            params.params[i].value = strdup(value);
            
            // Decode URL-encoded characters (simplified)
            char *src, *dst;
            for (src = dst = params.params[i].value; *src; src++, dst++) {
                if (*src == '+') {
                    *dst = ' ';
                } else if (*src == '%' && src[1] && src[2]) {
                    int hex;
                    sscanf(src + 1, "%2x", &hex);
                    *dst = hex;
                    src += 2;
                } else {
                    *dst = *src;
                }
            }
            *dst = '\0';
        } else {
            params.params[i].name = strdup(token);
            params.params[i].value = strdup("");
        }
        
        token = strtok(NULL, "&");
        i++;
    }
    
    free(qs);
    return params;
}

char* get_cgi_param(CGIParams params, const char *name) {
    for (int i = 0; i < params.count; i++) {
        if (strcmp(params.params[i].name, name) == 0) {
            return params.params[i].value;
        }
    }
    return NULL; // Return NULL if the parameter is not found
}

void free_cgi_params(CGIParams params) {
    for (int i = 0; i < params.count; i++) {
        free(params.params[i].name);
        free(params.params[i].value);
    }
    free(params.params);
}

// HTML helper functions





// Database functions




// Person and relationship functions
void free_person(Person *person) {
    if (!person) return;
    
    free(person->first_name);
    free(person->last_name);
    free(person->birth_date);
    free(person->death_date);
    free(person->bio);
    free(person->photo_url);
    
    // Reset to avoid double-free
    person->first_name = NULL;
    person->last_name = NULL;
    person->birth_date = NULL;
    person->death_date = NULL;
    person->bio = NULL;
    person->photo_url = NULL;
}

int get_person_by_id(sqlite3 *db, int id, Person *person) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT * FROM people WHERE id = ?;";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        person->id = sqlite3_column_int(stmt, 0);
        person->first_name = sqlite3_column_text(stmt, 1) ? strdup((const char*)sqlite3_column_text(stmt, 1)) : NULL;
        person->last_name = sqlite3_column_text(stmt, 2) ? strdup((const char*)sqlite3_column_text(stmt, 2)) : NULL;
        person->gender = sqlite3_column_text(stmt, 3) ? *((const char*)sqlite3_column_text(stmt, 3)) : '\0';
        person->birth_date = sqlite3_column_text(stmt, 4) ? strdup((const char*)sqlite3_column_text(stmt, 4)) : NULL;
        person->death_date = sqlite3_column_text(stmt, 5) ? strdup((const char*)sqlite3_column_text(stmt, 5)) : NULL;
        person->bio = sqlite3_column_text(stmt, 6) ? strdup((const char*)sqlite3_column_text(stmt, 6)) : NULL;
        person->photo_url = sqlite3_column_text(stmt, 7) ? strdup((const char*)sqlite3_column_text(stmt, 7)) : NULL;
        person->created_at = sqlite3_column_int64(stmt, 8);
        person->updated_at = sqlite3_column_int64(stmt, 9);
        
        sqlite3_finalize(stmt);
        return 0;
    }
    
    sqlite3_finalize(stmt);
    return 1; // Person not found
}

int get_parents(sqlite3 *db, int child_id, Person *father, Person *mother) {
    // Initialize father and mother with default values
    memset(father, 0, sizeof(Person));
    memset(mother, 0, sizeof(Person));
    
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT p.* FROM people p "
        "JOIN relationships r ON p.id = r.person1_id "
        "WHERE r.person2_id = ? AND r.relationship_type = 'parent-child';";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    
    sqlite3_bind_int(stmt, 1, child_id);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Person *parent = NULL;
        char gender = *((const char*)sqlite3_column_text(stmt, 3)); // gender column
        
        if (gender == 'M') {
            parent = father;
        } else if (gender == 'F') {
            parent = mother;
        }
        
        if (parent) {
            parent->id = sqlite3_column_int(stmt, 0);
            parent->first_name = sqlite3_column_text(stmt, 1) ? strdup((const char*)sqlite3_column_text(stmt, 1)) : NULL;
            parent->last_name = sqlite3_column_text(stmt, 2) ? strdup((const char*)sqlite3_column_text(stmt, 2)) : NULL;
            parent->gender = gender;
            parent->birth_date = sqlite3_column_text(stmt, 4) ? strdup((const char*)sqlite3_column_text(stmt, 4)) : NULL;
            parent->death_date = sqlite3_column_text(stmt, 5) ? strdup((const char*)sqlite3_column_text(stmt, 5)) : NULL;
            parent->bio = sqlite3_column_text(stmt, 6) ? strdup((const char*)sqlite3_column_text(stmt, 6)) : NULL;
            parent->photo_url = sqlite3_column_text(stmt, 7) ? strdup((const char*)sqlite3_column_text(stmt, 7)) : NULL;
            parent->created_at = sqlite3_column_int64(stmt, 8);
            parent->updated_at = sqlite3_column_int64(stmt, 9);
        }
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

int get_spouse(sqlite3 *db, int person_id, Person *spouse) {
    // Initialize spouse with default values
    memset(spouse, 0, sizeof(Person));
    
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT p.* FROM people p "
        "JOIN relationships r ON p.id = (CASE WHEN r.person1_id = ? THEN r.person2_id ELSE r.person1_id END) "
        "WHERE (r.person1_id = ? OR r.person2_id = ?) AND r.relationship_type = 'spouse' "
        "AND (r.divorce_date IS NULL OR r.divorce_date = '');";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    
    sqlite3_bind_int(stmt, 1, person_id);
    sqlite3_bind_int(stmt, 2, person_id);
    sqlite3_bind_int(stmt, 3, person_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        spouse->id = sqlite3_column_int(stmt, 0);
        spouse->first_name = sqlite3_column_text(stmt, 1) ? strdup((const char*)sqlite3_column_text(stmt, 1)) : NULL;
        spouse->last_name = sqlite3_column_text(stmt, 2) ? strdup((const char*)sqlite3_column_text(stmt, 2)) : NULL;
        spouse->gender = sqlite3_column_text(stmt, 3) ? *((const char*)sqlite3_column_text(stmt, 3)) : '\0';
        spouse->birth_date = sqlite3_column_text(stmt, 4) ? strdup((const char*)sqlite3_column_text(stmt, 4)) : NULL;
        spouse->death_date = sqlite3_column_text(stmt, 5) ? strdup((const char*)sqlite3_column_text(stmt, 5)) : NULL;
        spouse->bio = sqlite3_column_text(stmt, 6) ? strdup((const char*)sqlite3_column_text(stmt, 6)) : NULL;
        spouse->photo_url = sqlite3_column_text(stmt, 7) ? strdup((const char*)sqlite3_column_text(stmt, 7)) : NULL;
        spouse->created_at = sqlite3_column_int64(stmt, 8);
        spouse->updated_at = sqlite3_column_int64(stmt, 9);
        
        sqlite3_finalize(stmt);
        return 0;
    }
    
    sqlite3_finalize(stmt);
    return 1; // Spouse not found
}

int update_person(sqlite3 *db, Person *person) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "UPDATE people SET "
        "first_name = ?, last_name = ?, gender = ?, birth_date = ?, "
        "death_date = ?, bio = ?, photo_url = ?, updated_at = ? "
        "WHERE id = ?;";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    
    time_t now = time(NULL);
    person->updated_at = now;
    
    sqlite3_bind_text(stmt, 1, person->first_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, person->last_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, &person->gender, 1, SQLITE_STATIC);
    
    if (person->birth_date)
        sqlite3_bind_text(stmt, 4, person->birth_date, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 4);
        
    if (person->death_date)
        sqlite3_bind_text(stmt, 5, person->death_date, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 5);
        
    if (person->bio)
        sqlite3_bind_text(stmt, 6, person->bio, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 6);
        
    if (person->photo_url)
        sqlite3_bind_text(stmt, 7, person->photo_url, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 7);
        
    sqlite3_bind_int64(stmt, 8, person->updated_at);
    sqlite3_bind_int(stmt, 9, person->id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc != SQLITE_DONE);
}




// UI rendering functions
void render_person_card(Person *person) {
    if (!person || !person->id) return;
    
    char *escaped_first_name = html_escape(person->first_name);
    char *escaped_last_name = html_escape(person->last_name);
    char *gender_class = (person->gender == 'M') ? "male" : "female";
    
    printf("<div class=\"person-card %s\">\n", gender_class);
    if (person->photo_url) {
        char *escaped_photo_url = html_escape(person->photo_url);
        printf("  <img src=\"%s\" alt=\"%s %s\" class=\"person-photo\">\n", 
               escaped_photo_url, escaped_first_name, escaped_last_name);
        free(escaped_photo_url);
    } else {
        printf("  <div class=\"person-photo-placeholder\"></div>\n");
    }
    
    printf("  <h3>%s %s</h3>\n", escaped_first_name, escaped_last_name);
    
    if (person->birth_date) {
        char *escaped_birth = html_escape(person->birth_date);
        printf("  <p>Born: %s</p>\n", escaped_birth);
        free(escaped_birth);
    }
    
    if (person->death_date) {
        char *escaped_death = html_escape(person->death_date);
        printf("  <p>Died: %s</p>\n", escaped_death);
        free(escaped_death);
    }
    
    printf("  <a href=\"?action=view_profile&id=%d\" class=\"btn-primary\">View Profile</a>\n", person->id);
    
    printf("</div>\n");
    
    free(escaped_first_name);
    free(escaped_last_name);
}





void show_add_person_form(sqlite3 *db, int parent_id, const char *relationship_type) {
    printf("<h2>Add %s</h2>\n", relationship_type ? relationship_type : "Person");
    
    printf("<form action=\"?action=process_add_person\" method=\"post\">\n");
    
    if (parent_id > 0 && relationship_type) {
        printf("<input type=\"hidden\" name=\"parent_id\" value=\"%d\">\n", parent_id);
        printf("<input type=\"hidden\" name=\"relationship_type\" value=\"%s\">\n", relationship_type);
    }
    
    printf("<div class=\"form-group\">\n");
    printf("<label for=\"first_name\">First Name:</label>\n");
    printf("<input type=\"text\" id=\"first_name\" name=\"first_name\" class=\"form-control\" required>\n");
    printf("</div>\n");
    
    printf("<div class=\"form-group\">\n");
    printf("<label for=\"last_name\">Last Name:</label>\n");
    printf("<input type=\"text\" id=\"last_name\" name=\"last_name\" class=\"form-control\" required>\n");
    printf("</div>\n");
    
    printf("<div class=\"form-group\">\n");
    printf("<label for=\"gender\">Gender:</label>\n");
    printf("<select id=\"gender\" name=\"gender\" class=\"form-control\">\n");
    printf("<option value=\"M\">Male</option>\n");
    printf("<option value=\"F\">Female</option>\n");
    printf("</select>\n");
    printf("</div>\n");
    
    printf("<div class=\"form-group\">\n");
    printf("<label for=\"birth_date\">Birth Date:</label>\n");
    printf("<input type=\"date\" id=\"birth_date\" name=\"birth_date\" class=\"form-control\">\n");
    printf("</div>\n");
    
    printf("<div class=\"form-group\">\n");
    printf("<label for=\"death_date\">Death Date (if applicable):</label>\n");
    printf("<input type=\"date\" id=\"death_date\" name=\"death_date\" class=\"form-control\">\n");
    printf("</div>\n");
    
    printf("<div class=\"form-group\">\n");
    printf("<label for=\"bio\">Biography:</label>\n");
    printf("<textarea id=\"bio\" name=\"bio\" class=\"form-control\" rows=\"5\"></textarea>\n");
    printf("</div>\n");
    
    printf("<div class=\"form-group\">\n");
    printf("<label for=\"photo_url\">Photo URL:</label>\n");
    printf("<input type=\"url\" id=\"photo_url\" name=\"photo_url\" class=\"form-control\">\n");
    printf("</div>\n");
    
    if (strcmp(relationship_type, "spouse") == 0) {
        printf("<div class=\"form-group\">\n");
        printf("<label for=\"marriage_date\">Marriage Date:</label>\n");
        printf("<input type=\"date\" id=\"marriage_date\" name=\"marriage_date\" class=\"form-control\">\n");
        printf("</div>\n");
    }
    
    printf("<button type=\"submit\" class=\"btn-primary\">Add Person</button>\n");
    printf("<a href=\"?action=view_profile&id=%d\" class=\"btn-secondary\">Cancel</a>\n", parent_id);
    
    printf("</form>\n");
}

void process_add_person(sqlite3 *db, CGIParams params) {
    // Extract params
    char *first_name = get_cgi_param(params, "first_name");
    char *last_name = get_cgi_param(params, "last_name");
    char *gender_str = get_cgi_param(params, "gender");
    char gender = gender_str ? gender_str[0] : 'M';
    char *birth_date = get_cgi_param(params, "birth_date");
    char *death_date = get_cgi_param(params, "death_date");
    char *bio = get_cgi_param(params, "bio");
    char *photo_url = get_cgi_param(params, "photo_url");
    char *parent_id_str = get_cgi_param(params, "parent_id");
    char *relationship_type = get_cgi_param(params, "relationship_type");
    char *marriage_date = get_cgi_param(params, "marriage_date");
    
    // Convert parent_id to int
    int parent_id = parent_id_str ? atoi(parent_id_str) : 0;
    
    // Create and add new person
    Person new_person;
    memset(&new_person, 0, sizeof(Person));
    
    new_person.first_name = first_name ? strdup(first_name) : strdup("");
    new_person.last_name = last_name ? strdup(last_name) : strdup("");
    new_person.gender = gender;
    new_person.birth_date = birth_date && *birth_date ? strdup(birth_date) : NULL;
    new_person.death_date = death_date && *death_date ? strdup(death_date) : NULL;
    new_person.bio = bio && *bio ? strdup(bio) : NULL;
    new_person.photo_url = photo_url && *photo_url ? strdup(photo_url) : NULL;
    
    if (add_person(db, &new_person) == 0) {
        // Person added successfully
        int new_person_id = new_person.id;
        
        // Add relationship if parent_id is provided
        if (parent_id > 0 && relationship_type) {
            Relationship rel;
            memset(&rel, 0, sizeof(Relationship));
            
            if (strcmp(relationship_type, "parent-child") == 0) {
                rel.person1_id = parent_id;
                rel.person2_id = new_person_id;
                rel.relationship_type = strdup("parent-child");
                rel.marriage_date = NULL;
                rel.divorce_date = NULL;
            } else if (strcmp(relationship_type, "spouse") == 0) {
                rel.person1_id = parent_id;
                rel.person2_id = new_person_id;
                rel.relationship_type = strdup("spouse");
                rel.marriage_date = marriage_date && *marriage_date ? strdup(marriage_date) : NULL;
                rel.divorce_date = NULL;
            } else if (strcmp(relationship_type, "child") == 0) {
                rel.person1_id = new_person_id;
                rel.person2_id = parent_id;
                rel.relationship_type = strdup("parent-child");
                rel.marriage_date = NULL;
                rel.divorce_date = NULL;
            }
            
            add_relationship(db, &rel);
            free(rel.relationship_type);
            free(rel.marriage_date);
            free(rel.divorce_date);
        }
        
        printf("<p>Person added successfully.</p>\n");
        printf("<a href=\"?action=view_profile&id=%d\" class=\"btn-primary\">View Profile</a>\n", new_person_id);
    } else {
        printf("<p>Error adding person.</p>\n");
    }
    
    free_person(&new_person);
}

void show_login_form() {
    printf("<h2>Login</h2>\n");
    
    printf("<div id=\"login-error\" class=\"error-message\"></div>\n");
    
    printf("<form onsubmit=\"return handleLogin()\">\n");
    
    printf("<div class=\"form-group\">\n");
    printf("<label for=\"username\">Username:</label>\n");
    printf("<input type=\"text\" id=\"username\" name=\"username\" class=\"form-control\" required>\n");
    printf("</div>\n");
    
    printf("<div class=\"form-group\">\n");
    printf("<label for=\"password\">Password:</label>\n");
    printf("<input type=\"password\" id=\"password\" name=\"password\" class=\"form-control\" required>\n");
    printf("</div>\n");
    
    printf("<button type=\"submit\" class=\"btn-primary\">Login</button>\n");
    
    printf("</form>\n");
    
    printf("<p>Note: This is a demo application. Login functionality is not fully implemented.</p>\n");
}

void show_home_page(sqlite3 *db) {
    printf("<h2>Welcome to Family Tree Application</h2>\n");
    
    printf("<div class=\"home-actions\">\n");
    printf("<a href=\"?action=add_person\" class=\"btn-primary\">Add New Person</a>\n");
    printf("<a href=\"?action=view_tree\" class=\"btn-primary\">View Family Tree</a>\n");
    printf("</div>\n");
    
    // Show recently added people
    printf("<h3>Recently Added People</h3>\n");
    
    sqlite3_stmt *stmt;
    const char *sql = "SELECT * FROM people ORDER BY created_at DESC LIMIT 5;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        printf("<div class=\"recent-people\">\n");
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Person person;
            person.id = sqlite3_column_int(stmt, 0);
            person.first_name = sqlite3_column_text(stmt, 1) ? strdup((const char*)sqlite3_column_text(stmt, 1)) : NULL;
            person.last_name = sqlite3_column_text(stmt, 2) ? strdup((const char*)sqlite3_column_text(stmt, 2)) : NULL;
            person.gender = sqlite3_column_text(stmt, 3) ? *((const char*)sqlite3_column_text(stmt, 3)) : '\0';
            person.birth_date = sqlite3_column_text(stmt, 4) ? strdup((const char*)sqlite3_column_text(stmt, 4)) : NULL;
            person.death_date = sqlite3_column_text(stmt, 5) ? strdup((const char*)sqlite3_column_text(stmt, 5)) : NULL;
            person.bio = sqlite3_column_text(stmt, 6) ? strdup((const char*)sqlite3_column_text(stmt, 6)) : NULL;
            person.photo_url = sqlite3_column_text(stmt, 7) ? strdup((const char*)sqlite3_column_text(stmt, 7)) : NULL;
            
            render_person_card(&person);
            free_person(&person);
        }
        
        printf("</div>\n");
        sqlite3_finalize(stmt);
    }
}

// Main function
int main() {
    // Initialize database
    sqlite3 *db;
    if (init_database(&db) != 0) {
        printf("Content-Type: text/html\n\n");
        printf("<h1>Database Error</h1>");
        printf("<p>Could not initialize database.</p>");
        return 1;
    }
    
    // Create tables if they don't exist
    if (create_tables(db) != 0) {
        printf("Content-Type: text/html\n\n");
        printf("<h1>Database Error</h1>");
        printf("<p>Could not create tables.</p>");
        sqlite3_close(db);
        return 1;
    }
    
    // Parse query string
    char *query_string = getenv("QUERY_STRING");
    CGIParams params = parse_query_string(query_string);
    
    // Get action parameter
    char *action = get_cgi_param(params, "action");
    if (!action) action = "home";
    
    // Start HTML output
    print_html_header("Family Tree");
    
    // Process actions
    if (strcmp(action, "view_profile") == 0) {
        char *id_str = get_cgi_param(params, "id");
        int id = id_str ? atoi(id_str) : 0;
        
        if (id > 0) {
            render_person_profile(db, id);
        } else {
            printf("<p>Invalid person ID.</p>");
        }
    } else if (strcmp(action, "view_tree") == 0) {
        char *root_id_str = get_cgi_param(params, "root_id");
        int root_id = root_id_str ? atoi(root_id_str) : 1; // Default to ID 1
        
        char *levels_str = get_cgi_param(params, "levels");
        int levels = levels_str ? atoi(levels_str) : 3; // Default to 3 levels
        
        printf("<div class=\"tree-controls\">\n");
        printf("<h2>Family Tree</h2>\n");
        printf("<form action=\"?action=view_tree\" method=\"get\">\n");
        printf("<input type=\"hidden\" name=\"action\" value=\"view_tree\">\n");
        printf("<div class=\"form-group\">\n");
        printf("<label for=\"root_id\">Root Person ID:</label>\n");
        printf("<input type=\"number\" id=\"root_id\" name=\"root_id\" value=\"%d\" class=\"form-control\">\n", root_id);
        printf("</div>\n");
        printf("<div class=\"form-group\">\n");
        printf("<label for=\"levels\">Number of Generations:</label>\n");
        printf("<input type=\"number\" id=\"levels\" name=\"levels\" value=\"%d\" min=\"1\" max=\"5\" class=\"form-control\">\n", levels);
        printf("</div>\n");
        printf("<button type=\"submit\" class=\"btn-primary\">Update Tree</button>\n");
        printf("</form>\n");
        printf("</div>\n");
        
        printf("<div class=\"tree-container\">\n");
        render_family_tree(db, root_id, levels);
        printf("</div>\n");
    } else if (strcmp(action, "add_person") == 0) {
        // Check for parent_id and relationship_type for adding family members
        char *parent_id_str = get_cgi_param(params, "person_id");
        char *relationship_type = get_cgi_param(params, "relationship_type");
        int parent_id = parent_id_str ? atoi(parent_id_str) : 0;
        
        show_add_person_form(db, parent_id, relationship_type);
    } else if (strcmp(action, "process_add_person") == 0) {
        process_add_person(db, params);
    } else if (strcmp(action, "edit_person") == 0) {
        char *id_str = get_cgi_param(params, "id");
        int id = id_str ? atoi(id_str) : 0;
        
        if (id > 0) {
            Person person;
            if (get_person_by_id(db, id, &person) == 0) {
                printf("<h2>Edit Person</h2>\n");
                
                printf("<form action=\"?action=process_edit_person\" method=\"post\">\n");
                printf("<input type=\"hidden\" name=\"id\" value=\"%d\">\n", person.id);
                
                printf("<div class=\"form-group\">\n");
                printf("<label for=\"first_name\">First Name:</label>\n");
                char *escaped_first_name = html_escape(person.first_name);
                printf("<input type=\"text\" id=\"first_name\" name=\"first_name\" value=\"%s\" class=\"form-control\" required>\n", escaped_first_name);
                free(escaped_first_name);
                printf("</div>\n");
                
                printf("<div class=\"form-group\">\n");
                printf("<label for=\"last_name\">Last Name:</label>\n");
                char *escaped_last_name = html_escape(person.last_name);
                printf("<input type=\"text\" id=\"last_name\" name=\"last_name\" value=\"%s\" class=\"form-control\" required>\n", escaped_last_name);
                free(escaped_last_name);
                printf("</div>\n");
                
                printf("<div class=\"form-group\">\n");
                printf("<label for=\"gender\">Gender:</label>\n");
                printf("<select id=\"gender\" name=\"gender\" class=\"form-control\">\n");
                printf("<option value=\"M\"%s>Male</option>\n", person.gender == 'M' ? " selected" : "");
                printf("<option value=\"F\"%s>Female</option>\n", person.gender == 'F' ? " selected" : "");
                printf("</select>\n");
                printf("</div>\n");
                
                printf("<div class=\"form-group\">\n");
                printf("<label for=\"birth_date\">Birth Date:</label>\n");
                if (person.birth_date) {
                    char *escaped_birth_date = html_escape(person.birth_date);
                    printf("<input type=\"date\" id=\"birth_date\" name=\"birth_date\" value=\"%s\" class=\"form-control\">\n", escaped_birth_date);
                    free(escaped_birth_date);
                } else {
                    printf("<input type=\"date\" id=\"birth_date\" name=\"birth_date\" class=\"form-control\">\n");
                }
                printf("</div>\n");
                
                printf("<div class=\"form-group\">\n");
                printf("<label for=\"death_date\">Death Date (if applicable):</label>\n");
                if (person.death_date) {
                    char *escaped_death_date = html_escape(person.death_date);
                    printf("<input type=\"date\" id=\"death_date\" name=\"death_date\" value=\"%s\" class=\"form-control\">\n", escaped_death_date);
                    free(escaped_death_date);
                } else {
                    printf("<input type=\"date\" id=\"death_date\" name=\"death_date\" class=\"form-control\">\n");
                }
                printf("</div>\n");
                
                printf("<div class=\"form-group\">\n");
                printf("<label for=\"bio\">Biography:</label>\n");
                if (person.bio) {
                    char *escaped_bio = html_escape(person.bio);
                    printf("<textarea id=\"bio\" name=\"bio\" class=\"form-control\" rows=\"5\">%s</textarea>\n", escaped_bio);
                    free(escaped_bio);
                } else {
                    printf("<textarea id=\"bio\" name=\"bio\" class=\"form-control\" rows=\"5\"></textarea>\n");
                }
                printf("</div>\n");
                
                printf("<div class=\"form-group\">\n");
                printf("<label for=\"photo_url\">Photo URL:</label>\n");
                if (person.photo_url) {
                    char *escaped_photo_url = html_escape(person.photo_url);
                    printf("<input type=\"url\" id=\"photo_url\" name=\"photo_url\" value=\"%s\" class=\"form-control\">\n", escaped_photo_url);
                    free(escaped_photo_url);
                } else {
                    printf("<input type=\"url\" id=\"photo_url\" name=\"photo_url\" class=\"form-control\">\n");
                }
                printf("</div>\n");
                
                printf("<button type=\"submit\" class=\"btn-primary\">Update Person</button>\n");
                printf("<a href=\"?action=view_profile&id=%d\" class=\"btn-secondary\">Cancel</a>\n", person.id);
                
                printf("</form>\n");
                
                free_person(&person);
            } else {
                printf("<p>Person not found.</p>");
            }
        } else {
            printf("<p>Invalid person ID.</p>");
        }
    } else if (strcmp(action, "process_edit_person") == 0) {
        char *id_str = get_cgi_param(params, "id");
        int id = id_str ? atoi(id_str) : 0;
        
        if (id > 0) {
            Person person;
            if (get_person_by_id(db, id, &person) == 0) {
                // Free existing values before updating
                free(person.first_name);
                free(person.last_name);
                free(person.birth_date);
                free(person.death_date);
                free(person.bio);
                free(person.photo_url);
                
                // Update with form values
                char *first_name = get_cgi_param(params, "first_name");
                char *last_name = get_cgi_param(params, "last_name");
                char *gender_str = get_cgi_param(params, "gender");
                char *birth_date = get_cgi_param(params, "birth_date");
                char *death_date = get_cgi_param(params, "death_date");
                char *bio = get_cgi_param(params, "bio");
                char *photo_url = get_cgi_param(params, "photo_url");
                
                person.first_name = first_name ? strdup(first_name) : strdup("");
                person.last_name = last_name ? strdup(last_name) : strdup("");
                person.gender = gender_str ? gender_str[0] : 'M';
                person.birth_date = birth_date && *birth_date ? strdup(birth_date) : NULL;
                person.death_date = death_date && *death_date ? strdup(death_date) : NULL;
                person.bio = bio && *bio ? strdup(bio) : NULL;
                person.photo_url = photo_url && *photo_url ? strdup(photo_url) : NULL;
                
                if (update_person(db, &person) == 0) {
                    printf("<p>Person updated successfully.</p>\n");
                    printf("<a href=\"?action=view_profile&id=%d\" class=\"btn-primary\">View Profile</a>\n", person.id);
                } else {
                    printf("<p>Error updating person.</p>\n");
                }
                
                free_person(&person);
            } else {
                printf("<p>Person not found.</p>");
            }
        } else {
            printf("<p>Invalid person ID.</p>");
        }
    } else if (strcmp(action, "add_family_member") == 0) {
        char *person_id_str = get_cgi_param(params, "person_id");
        int person_id = person_id_str ? atoi(person_id_str) : 0;
        
        if (person_id > 0) {
            Person person;
            if (get_person_by_id(db, person_id, &person) == 0) {
                char *first_name = html_escape(person.first_name);
                char *last_name = html_escape(person.last_name);
                
                printf("<h2>Add Family Member for %s %s</h2>\n", first_name, last_name);
                
                printf("<div class=\"relationship-options\">\n");
                printf("<a href=\"?action=add_person&person_id=%d&relationship_type=parent-child\" class=\"btn-primary\">Add Parent</a>\n", person_id);
                printf("<a href=\"?action=add_person&person_id=%d&relationship_type=spouse\" class=\"btn-primary\">Add Spouse</a>\n", person_id);
                printf("<a href=\"?action=add_person&person_id=%d&relationship_type=child\" class=\"btn-primary\">Add Child</a>\n", person_id);
                printf("<a href=\"?action=view_profile&id=%d\" class=\"btn-secondary\">Cancel</a>\n", person_id);
                printf("</div>\n");
                
                free(first_name);
                free(last_name);
                free_person(&person);
            } else {
                printf("<p>Person not found.</p>");
            }
        } else {
            printf("<p>Invalid person ID.</p>");
        }
    } else if (strcmp(action, "login") == 0) {
        show_login_form();
    } else if (strcmp(action, "delete_person") == 0) {
        char *id_str = get_cgi_param(params, "id");
        int id = id_str ? atoi(id_str) : 0;
        
        if (id > 0) {
            // In a real application, this would check user permissions
            // and handle relationships before deleting
            
            // For demo purposes, just show a confirmation form
            printf("<h2>Delete Person</h2>\n");
            printf("<p>Are you sure you want to delete this person and all related relationships?</p>\n");
            
            printf("<form action=\"?action=process_delete_person\" method=\"post\">\n");
            printf("<input type=\"hidden\" name=\"id\" value=\"%d\">\n", id);
            printf("<button type=\"submit\" class=\"btn-primary\">Yes, Delete Person</button>\n");
            printf("<a href=\"?action=view_profile&id=%d\" class=\"btn-secondary\">Cancel</a>\n", id);
            printf("</form>\n");
        } else {
            printf("<p>Invalid person ID.</p>");
        }
    } else if (strcmp(action, "process_delete_person") == 0) {
        char *id_str = get_cgi_param(params, "id");
        int id = id_str ? atoi(id_str) : 0;
        
        if (id > 0) {
            // In a real application, this would check user permissions
            // and handle relationships before deleting
            
            // For demo purposes, just acknowledge the request
            printf("<p>Person deletion functionality is not implemented in this demo.</p>\n");
            printf("<a href=\"?action=home\" class=\"btn-primary\">Return to Home</a>\n");
        } else {
            printf("<p>Invalid person ID.</p>");
        }
    } else if (strcmp(action, "search") == 0) {
        char *search_term = get_cgi_param(params, "search_term");
        
        printf("<h2>Search Results</h2>\n");
        
        if (search_term && *search_term) {
            sqlite3_stmt *stmt;
            const char *sql = 
                "SELECT * FROM people WHERE first_name LIKE ? OR last_name LIKE ?;";
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                char search_pattern[256];
                snprintf(search_pattern, sizeof(search_pattern), "%%%s%%", search_term);
                
                sqlite3_bind_text(stmt, 1, search_pattern, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, search_pattern, -1, SQLITE_STATIC);
                
                printf("<div class=\"search-results\">\n");
                
                int found = 0;
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    found = 1;
                    Person person;
                    person.id = sqlite3_column_int(stmt, 0);
                    person.first_name = sqlite3_column_text(stmt, 1) ? strdup((const char*)sqlite3_column_text(stmt, 1)) : NULL;
                    person.last_name = sqlite3_column_text(stmt, 2) ? strdup((const char*)sqlite3_column_text(stmt, 2)) : NULL;
                    person.gender = sqlite3_column_text(stmt, 3) ? *((const char*)sqlite3_column_text(stmt, 3)) : '\0';
                    person.birth_date = sqlite3_column_text(stmt, 4) ? strdup((const char*)sqlite3_column_text(stmt, 4)) : NULL;
                    person.death_date = sqlite3_column_text(stmt, 5) ? strdup((const char*)sqlite3_column_text(stmt, 5)) : NULL;
                    person.bio = sqlite3_column_text(stmt, 6) ? strdup((const char*)sqlite3_column_text(stmt, 6)) : NULL;
                    person.photo_url = sqlite3_column_text(stmt, 7) ? strdup((const char*)sqlite3_column_text(stmt, 7)) : NULL;
                    
                    render_person_card(&person);
                    free_person(&person);
                }
                
                printf("</div>\n");
                
                if (!found) {
                    printf("<p>No results found for \"%s\".</p>\n", search_term);
                }
                
                sqlite3_finalize(stmt);
            }
        } else {
            printf("<p>Please enter a search term.</p>\n");
        }
        
        printf("<form action=\"?action=search\" method=\"get\">\n");
        printf("<input type=\"hidden\" name=\"action\" value=\"search\">\n");
        printf("<div class=\"form-group\">\n");
        printf("<label for=\"search_term\">Search:</label>\n");
        printf("<input type=\"text\" id=\"search_term\" name=\"search_term\" class=\"form-control\" required>\n");
        printf("</div>\n");
        printf("<button type=\"submit\" class=\"btn-primary\">Search</button>\n");
        printf("</form>\n");
    } else {
        // Default to home page
        show_home_page(db);
    }
    
    print_html_footer();
    
    // Cleanup
    free_cgi_params(params);
    sqlite3_close(db);
    
    return 0;
}