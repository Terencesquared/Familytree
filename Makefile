# Makefile for Family Tree web application

CC = gcc
CFLAGS = -Wall -Wextra -g -I./ -I"C:/Users/teren/OneDrive/Documents/Familytree project"
# Removed LDFLAGS because you're compiling sqlite3.c manually

# Source files
SRCS = main.c database.c web_interface.c sqlite3.c

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = family_tree.cgi

# Default target
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Rule to compile .c files to .o files
%.o: %.c family_tree.h
	$(CC) $(CFLAGS) -c $< -o $@

# Static files
STATIC_FILES = styles.css family-tree.js

# Install the application
install: $(TARGET) $(STATIC_FILES)
	mkdir -p $(DESTDIR)/cgi-bin
	mkdir -p $(DESTDIR)/htdocs
	install -m 755 $(TARGET) $(DESTDIR)/cgi-bin/
	install -m 644 $(STATIC_FILES) $(DESTDIR)/htdocs/
	@echo "Installation complete. Make sure your web server is configured to serve:"
	@echo "  - CGI scripts from $(DESTDIR)/cgi-bin/"
	@echo "  - Static files from $(DESTDIR)/htdocs/"

# Initialize the database with sample data
init_db: $(TARGET)
	./init_db.sh

# Create a CSS file
styles.css:
	@echo "Extracting CSS from HTML template..."
	@sed -n '/<style>/,/<\/style>/p' family-tree-frontend.html | \
		sed '1d;$$d' > styles.css

# Create a JavaScript file
family-tree.js:
	@echo "Extracting JavaScript from HTML template..."
	@sed -n '/<script>/,/<\/script>/p' family-tree-frontend.html | \
		grep -v '<script>' | grep -v '</script>' > family-tree.js

# Clean up
clean:
	rm -f $(OBJS) $(TARGET) styles.css family-tree.js

.PHONY: all install init_db_
