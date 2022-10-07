#include <stdio.h>
#include <stdlib.h>
#include <wasmtime/error.h>

#include "sqlite3.h"
#include "wasm.h"
#include "wasmtime.h"

#define FIB_WASM_SOURCE_EXAMPLE \
"(module\n" \
"  (type (;0;) (func (param i64) (result i64)))\n" \
"  (func $fib (type 0) (param i64) (result i64)\n" \
"    (local i64)\n" \
"    i64.const 0\n" \
"    local.set 1\n" \
"    block  ;; label = @1\n" \
"      local.get 0\n" \
"      i64.const 2\n" \
"      i64.lt_u\n" \
"      br_if 0 (;@1;)\n" \
"      i64.const 0\n" \
"      local.set 1\n" \
"      loop  ;; label = @2\n" \
"        local.get 0\n" \
"        i64.const -1\n" \
"        i64.add\n" \
"        call $fib\n" \
"        local.get 1\n" \
"        i64.add\n" \
"        local.set 1\n" \
"        local.get 0\n" \
"        i64.const -2\n" \
"        i64.add\n" \
"        local.tee 0\n" \
"        i64.const 1\n" \
"        i64.gt_u\n" \
"        br_if 0 (;@2;)\n" \
"      end\n" \
"    end\n" \
"    local.get 0\n" \
"    local.get 1\n" \
"    i64.add)\n" \
"  (memory (;0;) 16)\n" \
"  (global $__stack_pointer (mut i32) (i32.const 1048576))\n" \
"  (global (;1;) i32 (i32.const 1048576))\n" \
"  (global (;2;) i32 (i32.const 1048576))\n" \
"  (export \"memory\" (memory 0))\n" \
"  (export \"fib\" (func $fib)))\n" \

static void run_wasm(sqlite3_context *context, int argc, sqlite3_value **argv) {
   if (argc < 2) {
      sqlite3_result_error(context,
         "run_wasm function needs at least 2 parameters - the Wasm source code and the function name", -1);
      return;
   }

   const char *src = (const char *)sqlite3_value_text(argv[0]);  // Wasm source code - switch it to a compiled blob later
   const char *func_name = (const char *)sqlite3_value_text(argv[1]); // Function to call from the module

   wasm_engine_t *engine = wasm_engine_new();
   wasmtime_store_t *store = wasmtime_store_new(engine, NULL, NULL);
   wasmtime_context_t *wasm_ctx = wasmtime_store_context(store);

   wasm_byte_vec_t wasm;
   wasmtime_error_t *error = wasmtime_wat2wasm(src, strlen(src), &wasm);
   if (error) {
      wasm_name_t message;
      wasmtime_error_message(error, &message);
      fprintf(stderr, "error: %s\n", message.data);
      wasm_byte_vec_delete(&message);
      abort();
   }

   // Compile & instantiate the module (should be done once)
   wasmtime_module_t *module = NULL;
   wasmtime_module_new(engine, (uint8_t *)wasm.data, wasm.size, &module);
   wasm_byte_vec_delete(&wasm);

   wasm_trap_t *trap = NULL;
   wasmtime_instance_t instance;
   error = wasmtime_instance_new(wasm_ctx, module, NULL, 0, &instance, &trap);

   // Lookup the target function
   wasmtime_extern_t func;
   bool ok = wasmtime_instance_export_get(wasm_ctx, &instance, func_name, strlen(func_name), &func);
   assert(ok);
   assert(func.kind == WASMTIME_EXTERN_FUNC);

   wasmtime_val_t params[argc - 2];
   for (unsigned i = 0; i < argc - 2; ++i) {
      switch (sqlite3_value_type(argv[i + 2])) {
      case SQLITE_INTEGER:
         params[i].kind = WASMTIME_I64;
         params[i].of.i64 = sqlite3_value_int(argv[i + 2]);
         break;
      case SQLITE_FLOAT:
         params[i].kind = WASMTIME_F64;
         params[i].of.f64 = sqlite3_value_double(argv[i + 2]);
         break;
      case SQLITE_TEXT:
         assert(!"not implemented yet");
         //params[i].kind = WASMTIME_I32; // pointer
         // use sqlite3_value_text and copy it to module memory, then pass a pointer
         break;
      case SQLITE_BLOB:
         assert(!"not implemented yet");
         //params[i].kind = WASMTIME_I32; // pointer
         // use sqlite3_value_text and copy it to module memory, then pass a pointer + size, or make an indirect structure
         break;
      case SQLITE_NULL:
         params[i].kind = WASMTIME_I32;
         params[i].of.i32 = 0;
         break;
      }
   }

   wasmtime_val_t results[1];
   error = wasmtime_func_call(wasm_ctx, &func.of.func, params, 1, results, 1, &trap);

   switch (results[0].kind) {
   case WASMTIME_I64:
      sqlite3_result_int(context, results[0].of.i64);
      break;
   case WASMTIME_F64:
      sqlite3_result_double(context, results[0].of.f64);
      break;
   case WASMTIME_I32:
      assert(!"not implemented yet");
      break;
   }
}

void exec_sql(sqlite3 *db, const char *str) {
   sqlite3_stmt *stmt;
   sqlite3_prepare_v2(db, str, -1, &stmt, NULL);
   int rc;
   while ((rc = sqlite3_step(stmt)) != SQLITE_DONE) {
   }
   sqlite3_finalize(stmt);
}

int main(void) {
   sqlite3 *db;
   sqlite3_stmt *wasm_stmt;
   int rc = SQLITE_OK;

   sqlite3_open("/tmp/wasm_demo.db", &db);

   if (!db) {
      printf("Failed to open DB\n");
      return 1;
   }

   exec_sql(db, "create table if not exists wasm_test(id int primary key); ");
   exec_sql(db, "insert or replace into wasm_test(id) values (1); ");
   exec_sql(db, "insert or replace into wasm_test(id) values (2); ");
   exec_sql(db, "insert or replace into wasm_test(id) values (3); ");
   exec_sql(db, "insert or replace into wasm_test(id) values (4); ");
   exec_sql(db, "insert or replace into wasm_test(id) values (5); ");

   sqlite3_create_function(db, "run_wasm", -1, SQLITE_UTF8, NULL, &run_wasm, NULL, NULL);
   sqlite3_prepare_v2(db, "select id, run_wasm('" FIB_WASM_SOURCE_EXAMPLE "', 'fib', id) from wasm_test", -1, &wasm_stmt, NULL);

   printf("Results:\n");
   rc = SQLITE_OK;
   while ((rc = sqlite3_step(wasm_stmt)) != SQLITE_DONE) {
      if (rc != SQLITE_ROW) {
         fprintf(stderr, "Failed: %s\n", sqlite3_errstr(rc));
         break;
      }
      assert(sqlite3_column_count(wasm_stmt) == 2);
      assert(sqlite3_column_type(wasm_stmt, 0) == SQLITE_INTEGER);
      assert(sqlite3_column_type(wasm_stmt, 1) == SQLITE_INTEGER);
      printf("\tfib(%d) = %d\n", sqlite3_column_int(wasm_stmt, 0), sqlite3_column_int(wasm_stmt, 1));
   }
   sqlite3_finalize(wasm_stmt);
   sqlite3_close(db);

   return 0;
}
