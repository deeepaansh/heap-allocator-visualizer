CC     = clang
CFLAGS = -Wall -Wno-deprecated-declarations -Wno-int-conversion

TARGET = demo

SRCS = heap.c llist.c visualizer.c main.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
