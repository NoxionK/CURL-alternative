# Which compiler to use
CC = g++

# Compiler flags
CFLAGS = -Wall -std=c++11

# Target file
TARGET = 21127382_21127474_21127614

$(TARGET): 21127382_21127474_21127614.cpp
	$(CC) $(CFLAGS) 21127382_21127474_21127614.cpp -o $(TARGET)

.PHONY: clean

clean:
	rm -f $(TARGET)