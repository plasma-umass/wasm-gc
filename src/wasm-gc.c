/*
Example of instantiating of the WebAssembly module and invoking its exported
function.

You can compile and run this example on Linux with:

   cargo build --release -p wasmtime-c-api
   cc examples/hello.c \
       -I crates/c-api/include \
       -I crates/c-api/wasm-c-api/include \
       target/release/libwasmtime.a \
       -lpthread -ldl -lm \
       -o hello
   ./hello

Note that on Windows and macOS the command will be similar, but you'll need
to tweak the `-lpthread` and such annotations as well as the name of the
`libwasmtime.a` file on Windows.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wasm.h>
#include <wasmtime.h>

static void exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap);

static __attribute__((noinline))  wasm_trap_t* hello_callback(
    void *env,
    wasmtime_caller_t *caller,
    const wasmtime_val_t *args,
    size_t nargs,
    wasmtime_val_t *results,
    size_t nresults
) {
  void *stack_ptr;
  asm ("movq %%rsp, %0" : "=r"(stack_ptr));
  printf("stack_ptr %p\n", stack_ptr);
  printf("nresults %d\n", nresults);
  results->kind = WASMTIME_I32;
  results->of.i32 = 0;
  // *results = NULL;
  return NULL;
}

int main() {
  int ret = 0;
  // Set up our compilation context. Note that we could also work with a
  // `wasm_config_t` here to configure what feature are enabled and various
  // compilation settings.
  printf("Initializing...\n");
  wasm_config_t *config = wasm_config_new();
  wasmtime_config_static_memory_maximum_size_set(config, 1 << 30);
  assert(config != NULL);
  wasm_engine_t *engine = wasm_engine_new_with_config(config);
  assert(engine != NULL);

  // With an engine we can create a *store* which is a long-lived group of wasm
  // modules. Note that we allocate some custom data here to live in the store,
  // but here we skip that and specify NULL.
  wasmtime_store_t *store = wasmtime_store_new(engine, NULL, NULL);
  assert(store != NULL);
  wasmtime_memory_t memory;
  wasmtime_error_t *error;
  
  
  wasmtime_linker_t *linker = wasmtime_linker_new(engine);
  error = wasmtime_linker_define_wasi(linker);
  if (error != NULL)
    exit_with_error("failed to link wasi", error, NULL);

  wasmtime_context_t *context = wasmtime_store_context(store);
  
  // Instantiate wasi
  wasi_config_t *wasi_config = wasi_config_new();
  assert(wasi_config);
  wasi_config_inherit_argv(wasi_config);
  wasi_config_inherit_env(wasi_config);
  wasi_config_inherit_stdin(wasi_config);
  wasi_config_inherit_stdout(wasi_config);
  wasi_config_inherit_stderr(wasi_config);
  wasm_trap_t *trap = NULL;
  error = wasmtime_context_set_wasi(context, wasi_config);
  if (error != NULL)
    exit_with_error("failed to instantiate WASI", error, NULL);

  // Parse the wat into the binary wasm format
  wasm_byte_vec_t wasm;
  
  FILE* file = fopen("./test.wasm", "rb");;//fopen("examples/hello.wat", "r");
  assert(file != NULL);
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_new_uninitialized(&wasm, file_size);
  assert(fread(wasm.data, file_size, 1, file) == 1);
  fclose(file);

  // wasmtime_error_t *error = wasmtime_wat2wasm(wat.data, wat.size, &wasm);
  // if (error != NULL)
  //   exit_with_error("failed to parse wat", error, NULL);

  // Now that we've got our binary webassembly we can compile our module.
  printf("Compiling module...\n");
  wasmtime_module_t *module = NULL;
  error = wasmtime_module_new(engine, (uint8_t*) wasm.data, wasm.size, &module);
  wasm_byte_vec_delete(&wasm);
  if (error != NULL)
    exit_with_error("failed to compile module", error, NULL);

  // Next up we need to create the function that the wasm module imports. Here
  // we'll be hooking up a thunk function to the `hello_callback` native
  // function above. Note that we can assign custom data, but we just use NULL
  // for now).
  printf("Creating callback...\n");
  wasm_functype_t *hello_ty = wasm_functype_new_1_1(wasm_valtype_new(WASM_I32), wasm_valtype_new(WASM_I32));
  wasmtime_func_t hello;
  wasmtime_func_new(context, hello_ty, hello_callback, NULL, NULL, &hello);

  error = wasmtime_linker_define_func(linker, "env", 3, "__malloc", 8, hello_ty, hello_callback, NULL, NULL);
  // Instantiate the module
  wasmtime_instance_t instance;
  error = wasmtime_linker_instantiate(linker, context, module, &instance, &trap);
  if (error != NULL)
    exit_with_error("failed to instantiate module", error, NULL);

  printf("Extracting export...\n");
  wasmtime_extern_t run;
  bool ok = wasmtime_instance_export_get(context, &instance, "main", 4, &run);
  assert(ok);
  assert(run.kind == WASMTIME_EXTERN_FUNC);

  wasmtime_extern_t item;
  ok = wasmtime_instance_export_get(context, &instance, "memory", strlen("memory"), &item);
  assert(ok && item.kind == WASMTIME_EXTERN_MEMORY);
  memory = item.of.memory;

  size_t memory_size = wasmtime_memory_size(context, &memory);
  printf("current memory size %ld\n", memory_size);

  size_t data_size = wasmtime_memory_data_size(context, &memory);
  printf("current memory data size %ld\n", data_size);

  wasm_memorytype_t* memty;
  memty = wasmtime_memory_type(context, &memory);

  size_t max;
  wasmtime_memorytype_maximum(memty, &max);
  printf("max memory %ld\n", max);

  // size_t prev_size;
  // error = wasmtime_memory_grow(context, &memory, max - memory_size, &prev_size);
  // if (error != NULL)
  //   exit_with_error("failed to instantiate module", error, NULL);
  
  memory_size = wasmtime_memory_size(context, &memory);
  printf("memory size after growing %ld\n", memory_size);

  data_size = wasmtime_memory_data_size(context, &memory);
  printf("data size after growing %ld\n", data_size);

  wasm_exporttype_vec_t exports;
  wasmtime_instancetype_t* instancety = wasmtime_instance_type(context, &instance);
  wasmtime_instancetype_exports(instancety, &exports);

  for (int i = 0; i < exports.size; i++) {
    wasm_exporttype_t* export = exports.data[i];
    const wasm_name_t* exportname = wasm_exporttype_name(export);
    if (strcmp(exportname->data, "memory") == 0 || strcmp(exportname->data, "main") == 0 || strcmp(exportname->data, "_start") == 0)
      continue;
    
    printf("%s\n", exportname->data);

    ok = wasmtime_instance_export_get(context, &instance, exportname->data, exportname->size, &item);
    assert(ok && item.kind == WASMTIME_EXTERN_GLOBAL);

    wasmtime_global_t global = item.of.global;
    wasmtime_val_t globalval;
    wasmtime_global_get(context, &global, &globalval);
    printf("kind %d i32 %d\n", globalval.kind, globalval.of.i32);
  }
  // And call it!
  printf("Calling export...\n");
  wasmtime_val_t results;
  error = wasmtime_func_call(context, &run.of.func, NULL, 0, &results, 1, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("failed to call function", error, trap);

  
  // Clean up after ourselves at this point
  printf("All finished!\n");
  ret = 0;

  for (int i = 0; i < 40; i+=4) {
    uint32_t j = (wasmtime_memory_data(context, &memory)[i]) | (wasmtime_memory_data(context, &memory)[i+1] << 8) | (wasmtime_memory_data(context, &memory)[i+2] << 16) | ((wasmtime_memory_data(context, &memory)[i+3]) << 24);
    printf("%d\n", j);
  }
  // Clean up after ourselves at this point
  wasmtime_module_delete(module);
  wasmtime_store_delete(store);
  wasm_engine_delete(engine);
  return 0;

  
#if 0
  // With our callback function we can now instantiate the compiled module,
  // giving us an instance we can then execute exports from. Note that
  // instantiation can trap due to execution of the `start` function, so we need
  // to handle that here too.
  printf("Instantiating module...\n");
  trap = NULL;
  wasmtime_instance_t instance;
  wasmtime_extern_t import;
  import.kind = WASMTIME_EXTERN_FUNC;
  import.of.func = hello;
  error = wasmtime_instance_new(context, module, &import, 1, &instance, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("failed to instantiate", error, trap);

  // Lookup our `run` export function
  printf("Extracting export...\n");
  wasmtime_extern_t run;
  bool ok = wasmtime_instance_export_get(context, &instance, "run", 3, &run);
  assert(ok);
  assert(run.kind == WASMTIME_EXTERN_FUNC);

  // And call it!
  printf("Calling export...\n");
  error = wasmtime_func_call(context, &run.of.func, NULL, 0, NULL, 0, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("failed to call function", error, trap);

  // Clean up after ourselves at this point
  printf("All finished!\n");
  ret = 0;

  wasmtime_module_delete(module);
  wasmtime_store_delete(store);
  wasm_engine_delete(engine);
  return ret;
#endif
}

static void exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap) {
  fprintf(stderr, "error: %s\n", message);
  wasm_byte_vec_t error_message;
  if (error != NULL) {
    wasmtime_error_message(error, &error_message);
    wasmtime_error_delete(error);
  } else {
    wasm_trap_message(trap, &error_message);
    wasm_trap_delete(trap);
  }
  fprintf(stderr, "%.*s\n", (int) error_message.size, error_message.data);
  wasm_byte_vec_delete(&error_message);
  exit(1);
}
