/* web_interface.c - Web interface functions for family tree application */

#include "family_tree.h"
void generate_tree_json(sqlite3 *db, int person_id, int levels);
char* html_escape(const char *str);
void render_person_card(Person *person);



void print_html_header(const char *title) {
    printf("Content-Type: text/html\n\n");
    printf("<!DOCTYPE html>\n");
    printf("<html lang=\"en\">\n");
    printf("<head>\n");
    printf("  <meta charset=\"UTF-8\">\n");
    printf("  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    printf("  <title>%s</title>\n", title);
    printf("  <link rel=\"stylesheet\" href=\"/styles.css\">\n");
    printf("  <script src=\"https://cdnjs.cloudflare.com/ajax/libs/d3/7.8.5/d3.min.js\"></script>\n");
    printf("  <script src=\"/family-tree.js\"></script>\n");
    printf("</head>\n");
    printf("<body>\n");
    printf("  <header>\n");
    printf("    <h1>%s</h1>\n", title);
    printf("    <nav>\n");
    printf("      <ul>\n");
    printf("        <li><a href=\"/\">Home</a></li>\n");
    printf("        <li><a href=\"/tree\">Family Tree</a></li>\n");
    printf("        <li><a href=\"/login\">Login</a></li>\n");
    printf("      </ul>\n");
    printf("    </nav>\n");
    printf("  </header>\n");
    printf("  <main>\n");
}

void print_html_footer() {
    printf("  </main>\n");
    printf("  <footer>\n");
    printf("    <p>&copy; %d Family Tree Project</p>\n", 
           localtime(&(time_t){time(NULL)})->tm_year + 1900);
    printf("  </footer>\n");
    printf("</body>\n");
    printf("</html>\n");
}

void render_person_profile(sqlite3 *db, int person_id) {
    Person person;
    if (get_person_by_id(db, person_id, &person) != 0) {
        printf("<p>Person not found.</p>");
        return;
    }

    // Person info
    printf("<div class=\"person-profile\">\n");
    printf("  <h2>%s %s</h2>\n", 
           html_escape(person.first_name), 
           html_escape(person.last_name));
    
    if (person.photo_url) {
        printf("  <img src=\"%s\" alt=\"%s %s\" class=\"profile-photo\">\n", 
               html_escape(person.photo_url),
               html_escape(person.first_name),
               html_escape(person.last_name));
    }
    
    printf("  <div class=\"person-details\">\n");
    printf("    <p><strong>Birth:</strong> %s</p>\n", 
           person.birth_date ? html_escape(person.birth_date) : "Unknown");
    
    if (person.death_date) {
        printf("    <p><strong>Death:</strong> %s</p>\n", 
               html_escape(person.death_date));
    }
    
    if (person.bio) {
        printf("    <div class=\"bio\">\n");
        printf("      <h3>Biography</h3>\n");
        printf("      <p>%s</p>\n", html_escape(person.bio));
        printf("    </div>\n");
    }
    printf("  </div>\n");
    
    // Parents
    Person father, mother;
    int has_father = (get_parents(db, person_id, &father, &mother) == 0 && father.id > 0);
    int has_mother = (mother.id > 0);
    
    if (has_father || has_mother) {
        printf("  <div class=\"family-section parents\">\n");
        printf("    <h3>Parents</h3>\n");
        printf("    <ul>\n");
        
        if (has_father) {
            printf("      <li><a href=\"/person?id=%d\">%s %s</a></li>\n", 
                   father.id, 
                   html_escape(father.first_name), 
                   html_escape(father.last_name));
            free_person(&father);
        }
        
        if (has_mother) {
            printf("      <li><a href=\"/person?id=%d\">%s %s</a></li>\n", 
                   mother.id, 
                   html_escape(mother.first_name), 
                   html_escape(mother.last_name));
            free_person(&mother);
        }
        
        printf("    </ul>\n");
        printf("  </div>\n");
    }
    
    // Spouse
    Person spouse;
    int has_spouse = (get_spouse(db, person_id, &spouse) == 0 && spouse.id > 0);
    
    if (has_spouse) {
        printf("  <div class=\"family-section spouse\">\n");
        printf("    <h3>Spouse</h3>\n");
        printf("    <ul>\n");
        printf("      <li><a href=\"/person?id=%d\">%s %s</a></li>\n", 
               spouse.id, 
               html_escape(spouse.first_name), 
               html_escape(spouse.last_name));
        printf("    </ul>\n");
        printf("  </div>\n");
        free_person(&spouse);
    }
    
    // Children
    Person *children = NULL;
    int child_count = 0;
    
    if (get_children(db, person_id, &children, &child_count) == 0 && child_count > 0) {
        printf("  <div class=\"family-section children\">\n");
        printf("    <h3>Children</h3>\n");
        printf("    <ul>\n");
        
        for (int i = 0; i < child_count; i++) {
            printf("      <li><a href=\"/person?id=%d\">%s %s</a></li>\n", 
                   children[i].id, 
                   html_escape(children[i].first_name), 
                   html_escape(children[i].last_name));
            free_person(&children[i]);
        }
        
        printf("    </ul>\n");
        printf("  </div>\n");
        free(children);
    }
    
    // Edit button for authenticated users
    printf("  <div class=\"edit-section\">\n");
    printf("    <form action=\"/edit_person\" method=\"get\">\n");
    printf("      <input type=\"hidden\" name=\"id\" value=\"%d\">\n", person_id);
    printf("      <button type=\"submit\" class=\"edit-button\">Edit Information</button>\n");
    printf("    </form>\n");
    printf("  </div>\n");
    
    printf("</div>\n");
    
    free_person(&person);
}

void render_family_tree(sqlite3 *db, int root_id, int levels) {
    if (levels <= 0) return;
    
    Person person;
    if (get_person_by_id(db, root_id, &person) != 0) {
        printf("<p>Person not found.</p>");
        return;
    }
    
    printf("<div class=\"tree-level\">\n");
    render_person_card(&person);
    
    // Get spouse
    Person spouse;
    if (get_spouse(db, root_id, &spouse) == 0) {
        render_person_card(&spouse);
        free_person(&spouse);
    }
    
    // Get parents and render them recursively
    if (levels > 1) {
        Person father, mother;
        if (get_parents(db, root_id, &father, &mother) == 0) {
            printf("<div class=\"tree-parents\">\n");
            
            if (father.id) {
                render_family_tree(db, father.id, levels - 1);
                free_person(&father);
            }
            
            if (mother.id) {
                render_family_tree(db, mother.id, levels - 1);
                free_person(&mother);
            }
            
            printf("</div>\n");
        }
    }
    
    // Get children
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT p.* FROM people p "
        "JOIN relationships r ON p.id = r.person2_id "
        "WHERE r.person1_id = ? AND r.relationship_type = 'parent-child';";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, root_id);
        
        printf("<div class=\"tree-children\">\n");
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Person child;
            child.id = sqlite3_column_int(stmt, 0);
            child.first_name = sqlite3_column_text(stmt, 1) ? strdup((const char*)sqlite3_column_text(stmt, 1)) : NULL;
            child.last_name = sqlite3_column_text(stmt, 2) ? strdup((const char*)sqlite3_column_text(stmt, 2)) : NULL;
            child.gender = sqlite3_column_text(stmt, 3) ? *((const char*)sqlite3_column_text(stmt, 3)) : '\0';
            child.birth_date = sqlite3_column_text(stmt, 4) ? strdup((const char*)sqlite3_column_text(stmt, 4)) : NULL;
            child.death_date = sqlite3_column_text(stmt, 5) ? strdup((const char*)sqlite3_column_text(stmt, 5)) : NULL;
            child.bio = sqlite3_column_text(stmt, 6) ? strdup((const char*)sqlite3_column_text(stmt, 6)) : NULL;
            child.photo_url = sqlite3_column_text(stmt, 7) ? strdup((const char*)sqlite3_column_text(stmt, 7)) : NULL;
            
            render_person_card(&child);
            free_person(&child);
        }
        
        printf("</div>\n");
        sqlite3_finalize(stmt);
    }
    
    printf("</div>\n");
    
    free_person(&person);
}

void generate_tree_json(sqlite3 *db, int person_id, int levels) {
    if (levels <= 0) {
        printf("null");
        return;
    }
    
    Person person;
    if (get_person_by_id(db, person_id, &person) != 0) {
        printf("null");
        return;
    }
    
    printf("{\n");
    printf("  \"id\": %d,\n", person.id);
    
    // For name
    char *escaped_first_name = html_escape(person.first_name);
    char *escaped_last_name = html_escape(person.last_name);
    printf("  \"name\": \"%s %s\",\n", escaped_first_name, escaped_last_name);
    free(escaped_first_name);
    free(escaped_last_name);
    
    // For birth date
    if (person.birth_date) {
        char *escaped_birth_date = html_escape(person.birth_date);
        printf("  \"birthDate\": \"%s\",\n", escaped_birth_date);
        free(escaped_birth_date);
    } else {
        printf("  \"birthDate\": \"Unknown\",\n");
    }
    
    // For death date
    if (person.death_date) {
        char *escaped_death_date = html_escape(person.death_date);
        printf("  \"deathDate\": \"%s\",\n", escaped_death_date);
        free(escaped_death_date);
    }
    
    // For spouse
    Person spouse;
    if (get_spouse(db, person_id, &spouse) == 0 && spouse.id > 0) {
        printf("  \"spouse\": {\n");
        printf("    \"id\": %d,\n", spouse.id);
        
        char *escaped_spouse_first = html_escape(spouse.first_name);
        char *escaped_spouse_last = html_escape(spouse.last_name);
        printf("    \"name\": \"%s %s\"\n", escaped_spouse_first, escaped_spouse_last);
        free(escaped_spouse_first);
        free(escaped_spouse_last);
        
        printf("  },\n");
        free_person(&spouse);
    }
    
    // For children
    Person *children = NULL;
    int child_count = 0;
    
    if (get_children(db, person_id, &children, &child_count) == 0 && child_count > 0) {
        printf("  \"children\": [\n");
        
        for (int i = 0; i < child_count; i++) {
            if (i > 0) printf(",\n");
            generate_tree_json(db, children[i].id, levels - 1);
            free_person(&children[i]);
        }
        
        printf("\n  ]");
        free(children);
    } else {
        printf("  \"children\": []");
    }
    
    printf("\n}");
    
    free_person(&person);
}



char *html_escape(const char *str) {
    if (!str) return strdup("");
    
    size_t len = strlen(str);
    size_t escaped_len = len;
    
    // Count extra space needed for special characters
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '&': escaped_len += 4; break; // &amp;
            case '<': escaped_len += 3; break; // &lt;
            case '>': escaped_len += 3; break; // &gt;
            case '"': escaped_len += 5; break; // &quot;
            case '\'': escaped_len += 5; break; // &#039;
        }
    }
    
    char *escaped = malloc(escaped_len + 1);
    if (!escaped) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '&':
                strcpy(escaped + j, "&amp;");
                j += 5;
                break;
            case '<':
                strcpy(escaped + j, "&lt;");
                j += 4;
                break;
            case '>':
                strcpy(escaped + j, "&gt;");
                j += 4;
                break;
            case '"':
                strcpy(escaped + j, "&quot;");
                j += 6;
                break;
            case '\'':
                strcpy(escaped + j, "&#039;");
                j += 6;
                break;
            default:
                escaped[j++] = str[i];
                break;
        }
    }
    
    escaped[j] = '\0';
    return escaped;
}