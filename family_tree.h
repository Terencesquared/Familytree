/* family_tree.h - Main header file for family tree application */

#ifndef FAMILY_TREE_H
#define FAMILY_TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>

/* Data structures */
typedef struct {
    int id;
    char* first_name;
    char* last_name;
    char gender;          // 'M' or 'F'
    char* birth_date;     // YYYY-MM-DD format
    char* death_date;     // YYYY-MM-DD format or NULL
    char* bio;
    char* photo_url;
    time_t created_at;
    time_t updated_at;
} Person;

typedef struct {
    int id;
    int person1_id;
    int person2_id;
    char* relationship_type;  // "parent-child", "spouse"
    char* marriage_date;      // For spouse relationships
    char* divorce_date;       // For spouse relationships
    time_t created_at;
    time_t updated_at;
} Relationship;

/* CGI parameter structure */
typedef struct {
    char *name;
    char *value;
} CGIParam;

typedef struct {
    CGIParam *params;
    int count;
} CGIParams;

/* Function declarations */
int init_database(sqlite3 **db);
int create_tables(sqlite3 *db);
int add_person(sqlite3 *db, Person *person);
int update_person(sqlite3 *db, Person *person);
int get_person_by_id(sqlite3 *db, int id, Person *person);
int get_children(sqlite3 *db, int parent_id, Person **children, int *count);
int get_parents(sqlite3 *db, int child_id, Person *father, Person *mother);
int get_spouse(sqlite3 *db, int person_id, Person *spouse);
int add_relationship(sqlite3 *db, Relationship *rel);

void print_html_header(const char *title);
void print_html_footer();
void render_person_profile(sqlite3 *db, int person_id);
void render_family_tree(sqlite3 *db, int root_person_id, int levels);
void handle_form_submission(sqlite3 *db);

char* html_escape(const char *str);
char* get_cgi_param(CGIParams params, const char *name);
void free_person(Person *person);

#endif