CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O2

SRCS     := Dfs.cpp Dominator.cpp Heap.cpp
TEST_SRC := test_red.cpp
TARGET   := test_red

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SRCS) $(TEST_SRC) Dfs.h Dominator.h Heap.h
	$(CXX) $(CXXFLAGS) $(SRCS) $(TEST_SRC) -o $(TARGET)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe *.o *.obj

