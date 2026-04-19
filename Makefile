CXX      = g++
CXXFLAGS = -O2 -std=c++17 -Xpreprocessor -fopenmp \
           -I/opt/homebrew/opt/libomp/include
LDFLAGS  = -L/opt/homebrew/opt/libomp/lib -lomp
TARGET   = sort_bench

.PHONY: all run clean

all: $(TARGET)

$(TARGET): src/main.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
