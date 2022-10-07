# Running arbitrary WebAssembly functions in libSQL

This repository contains a proof-of-concept implementation of a libSQL function capable of running other functions created in WebAssembly.

Its core is `static void run_wasm(sqlite3_context *context, int argc, sqlite3_value **argv)` function from `wasm_runner.c`. This meta-function takes the WebAssembly module source code (in .wat form) as its first parameter, the function to be called from the compiled WebAssembly module as a second parameter, and forwards all of the remaining parameters directly to the WebAssembly function.

The .wat source code for the WebAssembly module is currently hardcoded for simplicity, but `run_wasm` function takes it as a parameter, so it can be provided during runtime.

Based on this proof of concept, we should consider extending the supported dialect with `CREATE FUNCTION` (https://github.com/libsql/libsql/issues/18), which compiles a Wasm function and stores it in an internal table.
Then, libSQL can perform an additional lookup in this table when trying to find a user-defined function by its name, and run it via the `run_wasm` interface if one is found.

## Limitations

This example implements only INTEGER and DOUBLE types, as these are natively supported in WebAssembly too. Supporting strings and blobs is not particularly hard, but involves memory management and thus is left for later.

## How the example function was compiled to WebAssembly

The source code can be found in `./libsql_bindgen` directory, which contains a very simple definition of a single function:
```rust
#[no_mangle]
pub fn fib(n: i64) -> i64 {
    match n {
        0 | 1 => n,
        _ => fib(n - 1) + fib(n - 2)
    }
}
```

In order to compile it to readable Wasm:
```sh
# only needed once, if you don't have this target already installed
rustup target add wasm32-unknown-unknown 

# remember about the release target, otherwise the generated code
# will have one billion lines of boilerplate
cargo build --release --target wasm32-unknown-unknown

wasm2wat target/wasm32-unknown-unknown/release/libsql_bindgen.wasm
```

## Compile and run the example app
This repository already contains sqlite3.c amalgamation and wasmtime bindings downloaded from their repo.
```
make
./wasm_runner
```

