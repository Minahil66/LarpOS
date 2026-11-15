CXX = g++
CXXFLAGS = -std=c++17 -O2
LIBS = -lcapstone -lelf

SRCS = main.cpp disasm.cpp graph.cpp Dfs.cpp Dominator.cpp Heap.cpp Renderer.cpp

larpos: $(SRCS)
	$(CXX) $(CXXFLAGS) -o larpos $(SRCS) $(LIBS)

clean:
	rm -f larpos
