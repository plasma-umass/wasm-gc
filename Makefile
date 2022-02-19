WASI_SDK=wasi-sdk-14.0/
CLANG=$(WASI_SDK)/bin/clang 
WASM_TIME_C_API=wasmtime-v0.34.1-x86_64-linux-c-api/
GCC=gcc

test.wasm: example/test.c
	$(CLANG) $< -o $@ -Wl,--allow-undefined -Wl,--export=aaa

wasm-gc: src/wasm-gc.c
	$(GCC) $< -I$(WASM_TIME_C_API)/include/ -L$(WASM_TIME_C_API)/lib -lwasmtime -o $@