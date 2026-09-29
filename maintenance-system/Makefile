CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

maintenance: src/maintenance.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run: maintenance
	./maintenance

test: maintenance
	bash tests/smoke.sh

clean:
	rm -f maintenance

.PHONY: run test clean
