all: wasm_runner.c sqlite3.c
	gcc -o wasm_runner wasm_runner.c sqlite3.c -I wasmtime/include/ wasmtime/lib/libwasmtime.a -lm
