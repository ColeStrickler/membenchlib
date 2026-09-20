RISCV_PREFIX ?= $(HOME)/opt/riscv-gcc14/bin/riscv64-unknown-linux-gnu-
CXX = $(RISCV_PREFIX)g++
TARGET ?= membenchpress
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic -march=rv64gc -mabi=lp64d

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.cpp membenchlib.cpp membenchlib.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) main.cpp membenchlib.cpp $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET)
