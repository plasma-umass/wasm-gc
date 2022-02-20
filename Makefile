WASI_SDK=wasi-sdk-14.0/
CLANG=$(WASI_SDK)/bin/clang 
WASM_TIME_C_API=wasmtime-v0.34.1-x86_64-linux-c-api/
CXX=g++

test.wasm: example/test.c
	$(CLANG) $< -o $@ -Wl,--allow-undefined -Wl,--export=array -O0

wasm-gc: src/wasm-gc.cpp
	$(CXX) $< -I$(WASM_TIME_C_API)/include/ -L$(WASM_TIME_C_API)/lib -lwasmtime -o $@