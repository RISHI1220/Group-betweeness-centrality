CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude
LDFLAGS  ?= -pthread

BUILD := build

# Library objects shared by the app and the tests.
CORE := $(BUILD)/csr.o $(BUILD)/gbc.o

.PHONY: all test clean run

all: $(BUILD)/gbc $(BUILD)/normalize-graph $(BUILD)/test-gbc

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/test_gbc.o: test/test_gbc.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/gbc: $(CORE) $(BUILD)/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD)/normalize-graph: $(BUILD)/normalize_graph.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/test-gbc: $(CORE) $(BUILD)/test_gbc.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Run the correctness suite (executed from the repo root so dataset paths resolve).
test: $(BUILD)/test-gbc
	./$(BUILD)/test-gbc

# Quick demo run on the small sample graph.
run: $(BUILD)/gbc
	./$(BUILD)/gbc datas/data4.txt --group 2,5

clean:
	rm -rf $(BUILD)
