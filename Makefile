.PHONY: help configure build test sanitize benchmark example clean

help:
	@echo "Available targets:"
	@echo "  make build      - build the project in Release mode"
	@echo "  make test       - build and run correctness tests"
	@echo "  make sanitize   - run tests with ASan and UBSan"
	@echo "  make example    - run the small CSV example"
	@echo "  make benchmark  - run synthetic benchmarks and analysis"
	@echo "  make clean      - remove build directories and generated CSV files"


configure:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

build: configure
	cmake --build build --parallel

test: build
	ctest --test-dir build --output-on-failure

sanitize:
	cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
	cmake --build build-sanitize --parallel
	ctest --test-dir build-sanitize --output-on-failure

benchmark: build
	python3 scripts/run_benchmarks.py --model dag-pareto
	python3 scripts/analyze_powerlaw.py results/benchmark.csv
	./build/temporal_profile --model dag-pareto > results/profile.csv
	python3 scripts/analyze_profile.py results/profile.csv

example: build
	./build/temporal_index_cli --vertices 4 --threshold 2 --input data/example.csv --source 0 --target 3

clean:
	rm -rf build build-sanitize results/*.csv
