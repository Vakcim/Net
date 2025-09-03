.PHONY: install run clean test

PROGRAM_NAME = rere

install:
	test -d flat_hash_map || git clone https://github.com/skarupke/flat_hash_map.git

build:
	g++ -I./flat_hash_map -Iinclude src/net.cpp $(PROGRAM_NAME).cpp -o $(PROGRAM_NAME)

run: $(PROGRAM_NAME)
	./$(PROGRAM_NAME)

test: $(PROGRAM_NAME)
	g++ -I./flat_hash_map -Iinclude src/net.cpp $(PROGRAM_NAME).cpp -o $(PROGRAM_NAME) -g
	./$(PROGRAM_NAME)
clean:
	rm -f $(PROGRAM_NAME) *.csv