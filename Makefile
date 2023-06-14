CC=gcc-12

SOURCES=$(wildcard ./src/*.c ./src/**/*.c)
OBJECTS=$(SOURCES:.c=.o)

FRAMEWORKS=-F/Library/Frameworks -framework SDL2 -framework Accelerate

EXECUTABLE=./swarm

.PHONY: all
all: $(EXECUTABLE)
	$(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(FRAMEWORKS) $(OBJECTS)

$(OBJECTS): %.o: %.c
	$(CC) $(CFLAGS) -c -o $@ -Wall -Wextra -std=c99 -pedantic -flax-vector-conversions $(FRAMEWORKS) -Iheaders $<

.PHONY: clean
clean: 
	rm -f $(OBJECTS) $(EXECUTABLE)