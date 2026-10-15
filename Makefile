CC ?= cc
CXX ?= c++
ISPC ?= ispc
ISPC_TARGET ?= avx2-i32x8
ifeq ($(origin FC),default)
FC = gfortran
endif
CPPFLAGS := -Iinclude -Isrc -Ivendor/neuroroute/include -Ivendor/surreal/include
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
CXXFLAGS := -std=c++23 -O2 -Wall -Wextra -Wpedantic -Werror
ASAN_OPTIONS ?= detect_leaks=1:halt_on_error=1
C_OBJECTS := build/neuro_mantle.o build/neuro_mantle_ecc.o build/neuro_bioclock.o build/neuro_sandbox.o build/routing.o build/spatial_pool.o build/primitiveq.o build/neuro_world.o build/improve.o
CXX_OBJECTS := build/observer.o build/main.o build/qmemory.o build/ai.o build/atlas.o build/state_tree.o build/world.o build/hash.o build/exact.o build/causal.o build/entropy.o
ifneq ($(shell command -v $(FC) 2>/dev/null),)
F_OBJECTS := build/qmemory_decode.o
F_LIBS := -lgfortran
else
F_OBJECTS := build/qmemory_decode_c.o
F_LIBS :=
endif
.PHONY: all test sanitize clean devcli vanta-check companions qecc-check benchmark bench-ispc
all: devcli train-libs bin/neuromantle
bin/nm-improve.so: training/improve.c include/neuro_improve.h | bin
	$(CC) $(CPPFLAGS) -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -fPIC -shared training/improve.c -o $@
.PHONY: train-libs
train-libs: bin/nm-train.so bin/nm-improve.so
bin/nm-train.so: training/optimizer.c training/optimizer.h | bin
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -fPIC -shared training/optimizer.c -o $@
bin/test_train_native: tests/train_native.c training/optimizer.c training/optimizer.h | bin
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror tests/train_native.c training/optimizer.c -o $@
devcli:
	$(MAKE) -C vendor/devcli devcli
vanta-check:
	$(MAKE) -C vendor/vanta test
companions: all
	python3 tests/check_companions.py
qecc-check: bin/neuromantle
	python3 tests/qecc_integration.py
build bin:
	mkdir -p $@
build/neuro_mantle.o: core/neuro_mantle.c include/neuro_mantle.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/primitiveq.o: core/primitiveq.c include/primitiveq.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/neuro_world.o: core/neuro_world.c include/neuro_world.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/improve.o: training/improve.c include/neuro_improve.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/neuro_mantle_ecc.o: core/neuro_mantle_ecc.c include/neuro_mantle_ecc.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/neuro_bioclock.o: core/neuro_bioclock.c include/neuro_bioclock.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/neuro_sandbox.o: core/neuro_sandbox.c include/neuro_sandbox.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/routing.o: vendor/neuroroute/src/routing.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/spatial_pool.o: vendor/neuroroute/src/spatial_pool.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/observer.o: src/observer.cpp src/observer.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/main.o: src/main.cpp src/atlas.hpp src/observer.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/qmemory.o: src/qmemory.cpp src/qmemory.hpp include/primitiveq.h include/qmemory_readout.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/qmemory_decode.o: kernels/qmemory_decode.f90 | build
	$(FC) -O3 -funroll-loops -Wall -Wextra -Werror -fcheck=bounds -Jbuild -c $< -o $@
build/qmemory_decode_c.o: core/qmemory_decode.c include/qmemory_readout.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/ai.o: src/ai.cpp src/ai.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/atlas.o: src/atlas.cpp src/atlas.hpp include/neuro_bioclock.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/state_tree.o: src/state_tree.cpp src/state_tree.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/world.o: src/world.cpp src/world.hpp src/observer.hpp include/neuro_world.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/%.o: vendor/surreal/src/%.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
bin/neuromantle: $(C_OBJECTS) $(CXX_OBJECTS) $(F_OBJECTS) | bin
	$(CXX) $(CXXFLAGS) $^ $(F_LIBS) -o $@
	chmod 755 $@
bin/test_core: tests/test_core.c build/neuro_mantle.o | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
bin/test_primitiveq: tests/test_primitiveq.c core/primitiveq.c include/primitiveq.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_primitiveq.c core/primitiveq.c -o $@
bin/five_circuit_probe: tests/five_circuit_probe.c core/primitiveq.c include/primitiveq.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/five_circuit_probe.c core/primitiveq.c -o $@
bin/test_qmemory: tests/test_qmemory.cpp build/qmemory.o build/primitiveq.o build/hash.o build/neuro_mantle.o $(F_OBJECTS) | bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(F_LIBS) -o $@
bin/test_qmemory_readout: tests/test_qmemory_readout.cpp $(F_OBJECTS) include/qmemory_readout.h | bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_qmemory_readout.cpp $(F_OBJECTS) $(F_LIBS) -o $@
bin/primitiveq_oracle: tests/primitiveq_oracle.cpp build/primitiveq.o | bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/primitiveq_oracle.cpp build/primitiveq.o -o $@
bin/test_world: tests/test_world.c core/neuro_world.c core/primitiveq.c core/neuro_mantle.c include/neuro_world.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_world.c core/neuro_world.c core/primitiveq.c core/neuro_mantle.c -o $@
bin/bench_primitiveq: bench/bench_primitiveq.c core/primitiveq.c include/primitiveq.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) bench/bench_primitiveq.c core/primitiveq.c -o $@
bin/bench_qmemory: bench/bench_qmemory.cpp $(F_OBJECTS) | bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(F_LIBS) -o $@
bin/bench_qmemory_restore: bench/bench_qmemory_restore.cpp build/qmemory.o build/primitiveq.o build/hash.o build/neuro_mantle.o $(F_OBJECTS) | bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(F_LIBS) -o $@
build/qmemory_classify_ispc.o: kernels/qmemory_classify.ispc | build
	$(ISPC) $< -O2 --target=$(ISPC_TARGET) -o $@ -h build/qmemory_classify_ispc.h
build/qmemory_classify_soa_ispc.o: kernels/qmemory_classify_soa.ispc | build
	$(ISPC) $< -O2 --target=$(ISPC_TARGET) -o $@ -h build/qmemory_classify_soa_ispc.h
bin/bench_classify_ispc: bench/bench_classify_trial.cpp build/qmemory_classify_ispc.o build/primitiveq.o | bin
	$(CXX) $(CPPFLAGS) -Ibuild $(CXXFLAGS) $^ -o $@
bin/bench_classify_soa: bench/bench_classify_soa.cpp build/qmemory_classify_soa_ispc.o build/primitiveq.o | bin
	$(CXX) $(CPPFLAGS) -Ibuild $(CXXFLAGS) $^ -o $@
bench-ispc: bin/bench_classify_ispc bin/bench_classify_soa
	bin/bench_classify_ispc
	bin/bench_classify_soa
bin/test_ecc: tests/test_ecc.c build/neuro_mantle_ecc.o | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
bin/test_bioclock: tests/test_bioclock.c build/neuro_bioclock.o | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
bin/test_state_tree: tests/state_tree_test.cpp build/state_tree.o build/hash.o build/neuro_mantle.o build/routing.o build/spatial_pool.o | bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@
bin/bigint_probe: tests/bigint_probe.cpp vendor/surreal/include/surreal/bigint.hpp | bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@
bin/state_probe: tests/state_probe.c build/neuro_mantle.o | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
bin/bench_core: bench/bench_core.c build/neuro_mantle.o | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
benchmark: bin/bench_core
	python3 scripts/run_benchmarks.py
	python3 scripts/run_end_to_end_benchmarks.py
test: all bin/test_core bin/test_ecc bin/test_bioclock bin/test_state_tree bin/bigint_probe bin/state_probe bin/test_train_native bin/test_primitiveq bin/five_circuit_probe bin/test_qmemory bin/test_qmemory_readout bin/test_world bin/primitiveq_oracle
	./bin/test_qmemory
	./bin/test_qmemory_readout
	./bin/test_primitiveq
	python3 tests/five_code_oracle.py
	python3 tests/five_circuit_oracle.py
	python3 tests/qmemory_integration.py
	python3 tests/entropy_seed_integration.py
	./bin/primitiveq_oracle
	./bin/test_world
	python3 tests/world_integration.py
	python3 tests/five_code_integration.py
	python3 tests/watchdog_integration.py
	./bin/test_train_native
	python3 tests/training_oracle.py
	./bin/test_core
	./bin/test_ecc
	./bin/test_bioclock
	./bin/test_state_tree
	python3 tests/integration.py
	python3 tests/bio_clock_integration.py
	python3 tests/intervention_integration.py
	python3 tests/atlas_integration.py
	python3 tests/sandbox_integration.py
	python3 tests/ai_integration.py
	python3 tests/socket_integration.py
	python3 tests/bigint_differential.py
	python3 tests/network_oracle.py
	python3 tests/observer_integration.py
sanitize:
	$(MAKE) clean
	NM_BIGINT_CASES_PER_SIZE=10 $(MAKE) CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' CXXFLAGS='-std=c++23 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' test
clean:
	rm -rf build bin
