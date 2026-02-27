#include "manifast/VM/VM.h"
#include "manifast/VM/Compiler.h"
#include "manifast/Lexer.h"
#include "manifast/Parser.h"
#include <string>
#include <vector>
#include "manifast/Runtime.h"

extern "C" {

void manifast_call_native(const char* mod_name, const char* fn_name, ::Any* args, int nargs) {
    ::Any* mod = manifast_impor(mod_name);
    if (!mod || mod->type != 7) return;
    ::Any* fn = manifast_object_get(mod, fn_name);
    if (!fn || (fn->type != 4 && fn->type != 5)) return;
    
    // For module methods, we need to inject the module as 'self' (args[0])
    // manifast_call_dynamic already handles this if we pass args correctly.
    // In our case, the WASM VM passes args WITHOUT self.
    // But manifast_call_dynamic expects args[0..nargs-1].
    // If it's a native function that expects self, we should provide it.
    
    std::vector<::Any> new_args;
    new_args.push_back(*mod);
    for(int i=0; i<nargs; i++) new_args.push_back(args[i]);
    
    manifast_call_dynamic(fn, new_args.data(), (int)new_args.size());
}

void wasm_print(manifast::vm::VM* vm, ::Any* args, int nargs);

static std::string g_wasm_output;

void wasm_clear_output(manifast::vm::VM* vm, ::Any* args, int nargs) {
    g_wasm_output += "[CLR]";
}

void wasm_sleep(manifast::vm::VM* vm, ::Any* args, int nargs) {
    int idx = 0; if (nargs >= 1 && args[0].type != 0) idx++;
    int ms = (nargs - idx >= 1 && args[idx].type == 0) ? (int)args[idx].number : 100;
    g_wasm_output += "[DLY:" + std::to_string(ms) + "]";
}

static const char* b64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = ((uint32_t)data[i]) << 16;
        if (i + 1 < len) n |= ((uint32_t)data[i+1]) << 8;
        if (i + 2 < len) n |= data[i+2];
        out += b64_chars[(n >> 18) & 0x3F];
        out += b64_chars[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? b64_chars[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? b64_chars[n & 0x3F] : '=';
    }
    return out;
}

static void encode_bmp_to_buffer(const uint8_t* rgba, int w, int h, std::vector<uint8_t>& out) {
    int row_stride = w * 3;
    int padding = (4 - (row_stride % 4)) % 4;
    int data_size = (row_stride + padding) * h;
    int file_size = 54 + data_size;
    
    out.resize(file_size);
    memset(out.data(), 0, 54);
    out[0] = 'B'; out[1] = 'M';
    memcpy(&out[2], &file_size, 4);
    int offset = 54; memcpy(&out[10], &offset, 4);
    int dib_size = 40; memcpy(&out[14], &dib_size, 4);
    memcpy(&out[18], &w, 4);
    int neg_h = -h; memcpy(&out[22], &neg_h, 4);
    uint16_t planes = 1; memcpy(&out[26], &planes, 2);
    uint16_t bpp = 24; memcpy(&out[28], &bpp, 2);
    memcpy(&out[34], &data_size, 4);
    
    int idx = 54;
    uint8_t pad_bytes[3] = {0, 0, 0};
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int si = (y * w + x) * 4;
            out[idx++] = rgba[si + 2]; // B
            out[idx++] = rgba[si + 1]; // G
            out[idx++] = rgba[si + 0]; // R
        }
        for (int p = 0; p < padding; p++) out[idx++] = 0;
    }
}

void wasm_print_any(::Any* val, int depth = 0) {
    if (depth > 32) { g_wasm_output += "..."; return; }

    if (val->type == 0) {
        char buf[64];
        if (val->number == (long long)val->number) sprintf(buf, "%lld", (long long)val->number);
        else sprintf(buf, "%g", val->number);
        g_wasm_output += buf;
    }
    else if (val->type == 1 && val->ptr) g_wasm_output += (char*)val->ptr;
    else if (val->type == 2) g_wasm_output += (val->number ? "benar" : "salah");
    else if (val->type == 3) g_wasm_output += "nil";
    else if (val->type == 6 && val->ptr) {
        ManifastArray* arr = (ManifastArray*)val->ptr;
        g_wasm_output += "[";
        for (uint32_t i = 0; i < arr->size; i++) {
            if (i > 0) g_wasm_output += ", ";
            wasm_print_any(&arr->elements[i], depth + 1);
        }
        g_wasm_output += "]";
    }
    else if (val->type == 7 && val->ptr) {
        ManifastObject* obj = (ManifastObject*)val->ptr;
        g_wasm_output += "{";
        for (uint32_t i = 0; i < obj->size; i++) {
            if (i > 0) g_wasm_output += ", ";
            if (obj->entries[i].key) g_wasm_output += obj->entries[i].key;
            g_wasm_output += ": ";
            wasm_print_any(&obj->entries[i].value, depth + 1);
        }
        g_wasm_output += "}";
    }
    else if (val->type == 8 && val->ptr) {
        ManifastClass* klass = (ManifastClass*)val->ptr;
        g_wasm_output += "[Kelas ";
        g_wasm_output += (klass->name ? klass->name : "?");
        g_wasm_output += "]";
    }
    else if (val->type == 9 && val->ptr) {
        ManifastInstance* inst = (ManifastInstance*)val->ptr;
        g_wasm_output += "[Instance of ";
        g_wasm_output += (inst->klass->name ? inst->klass->name : "?");
        g_wasm_output += "]";
    }
    else if (val->type == 4) g_wasm_output += "[Fungsi Native]";
    else if (val->type == 5) g_wasm_output += "[Fungsi]";
    else g_wasm_output += "{Objek}";
}

void wasm_print(manifast::vm::VM* vm, ::Any* args, int nargs) {
    for (int i = 0; i < nargs; ++i) {
        if (i > 0) g_wasm_output += "\t";
        wasm_print_any(&args[i]);
    }
}

void wasm_println(manifast::vm::VM* vm, ::Any* args, int nargs) {
    wasm_print(vm, args, nargs);
    g_wasm_output += "\n";
}

void wasm_assert(manifast::vm::VM* vm, ::Any* args, int nargs) {
    int idx = 0; if (nargs >= 1 && args[0].type != ANY_NIL) idx = 0; // Fix arg index
    if (nargs < 1) {
        MANIFAST_THROW("Runtime Error: assert() membutuhkan minimal 1 argumen");
    }
    
    bool truth = false;
    if (args[0].type == ANY_BOOLEAN) truth = (args[0].number != 0);
    else if (args[0].type == ANY_NIL) truth = false;
    else if (args[0].type == ANY_NUMBER) truth = (args[0].number != 0);
    else truth = true;

    if (!truth) {
        std::string msg = "Assertion Gagal";
        if (nargs >= 2 && args[1].type == ANY_STRING) {
            msg = (char*)args[1].ptr;
        }
        MANIFAST_THROW("Runtime Error: [ASSERT GAGAL] " + msg);
    }
}

// Overrides for CodeGen/JIT
void manifast_print_any(::Any* val) {
    wasm_print_any(val);
}

void manifast_println_any(::Any* val) {
    wasm_print_any(val);
    g_wasm_output += "\n";
}

void manifast_assert(::Any* cond, ::Any* msg) {
    bool truth = false;
    if (cond->type == ANY_BOOLEAN) truth = (cond->number != 0);
    else if (cond->type == ANY_NIL) truth = false;
    else if (cond->type == ANY_NUMBER) truth = (cond->number != 0);
    else truth = true;

    if (!truth) {
        std::string errMsg = "Assertion Gagal";
        if (msg && msg->type == ANY_STRING) {
            errMsg = (char*)msg->ptr;
        }
        g_wasm_output += "\n[ASSERT GAGAL] " + errMsg + "\n";
    }
}

void wasm_len(manifast::vm::VM* vm, ::Any* args, int nargs) {
    if (nargs < 1) {
        args[-1] = {ANY_NIL, 0, nullptr};
        return;
    }
    args[-1] = {ANY_NUMBER, (double)manifast_array_len(&args[0]), nullptr};
}

void wasm_plot_for(manifast::vm::VM* vm, ::Any* args, int nargs) {
    if (nargs < 1) return;
    int offset = (args[0].type == ANY_OBJECT) ? 1 : 0;
    ::Any* y = &args[offset];
    ::Any* x = (nargs - offset >= 2 && args[offset+1].type == ANY_ARRAY) ? &args[offset+1] : nullptr;
    ::Any* cfg = (nargs - offset >= 3) ? &args[offset+2] : (nargs - offset >= 2 && args[offset+1].type == ANY_OBJECT) ? &args[offset+1] : nullptr;
    manifast_plot_for(y, x, cfg);
    args[-1] = {ANY_BOOLEAN, 1.0, nullptr};
}

void wasm_plot_line(manifast::vm::VM* vm, ::Any* args, int nargs) {
    manifast_call_native("plot", "line", args, nargs);
    args[-1] = {ANY_BOOLEAN, 1.0, nullptr};
}

void wasm_plot_scatter(manifast::vm::VM* vm, ::Any* args, int nargs) {
    manifast_call_native("plot", "scatter", args, nargs);
    args[-1] = {ANY_BOOLEAN, 1.0, nullptr};
}

void wasm_plot_bar(manifast::vm::VM* vm, ::Any* args, int nargs) {
    manifast_call_native("plot", "bar", args, nargs);
    args[-1] = {ANY_BOOLEAN, 1.0, nullptr};
}

void wasm_plot_show(manifast::vm::VM* vm, ::Any* args, int nargs) {
    manifast_call_native("plot", "show", args, nargs);
    args[-1] = {ANY_BOOLEAN, 1.0, nullptr};
}

void wasm_plot_reset(manifast::vm::VM* vm, ::Any* args, int nargs) {
    manifast_call_native("plot", "reset", args, nargs);
    args[-1] = {ANY_NIL, 0.0, nullptr};
}

void wasm_plot_save(manifast::vm::VM* vm, ::Any* args, int nargs) {
    manifast_call_native("plot", "save", args, nargs);
    args[-1] = {ANY_BOOLEAN, 1.0, nullptr};
}

// Plot callback: receives RGBA framebuffer, encodes BMP, base64, emits to output
static std::vector<uint8_t> g_plot_bmp_buffer;

void wasm_plot_callback(const uint8_t* rgba, int w, int h) {
    encode_bmp_to_buffer(rgba, w, h, g_plot_bmp_buffer);
    std::string b64 = base64_encode(g_plot_bmp_buffer.data(), g_plot_bmp_buffer.size());
    g_wasm_output += "\n[IMG:data:image/bmp;base64," + b64 + "]\n";
}

// Exported for npm consumers
const uint8_t* mf_get_plot_buffer() {
    return g_plot_bmp_buffer.data();
}

int mf_get_plot_buffer_size() {
    return (int)g_plot_bmp_buffer.size();
}

// Event callback for async mode (EventEmitter-style)
// event_type: 0=log, 1=error, 2=plot, 3=clear, 4=done
typedef void (*MfEventCallback)(int event_type, const char* data);
static MfEventCallback g_event_callback = nullptr;

void mf_set_event_callback(MfEventCallback cb) {
    g_event_callback = cb;
}

static std::string g_event_output;

// Synchronous run: collects all output, returns as string
const char* mf_run_script_tier(const char* source, int tier) {
    g_wasm_output = "";
    g_plot_bmp_buffer.clear();
    
    manifast_set_plot_show_callback(wasm_plot_callback);
    manifast_set_clear_output_callback([](){ g_wasm_output += "[CLR]"; });
    
    manifast::SyntaxConfig config;
    manifast::Lexer lexer(source, config);
    manifast::Parser parser(lexer, source);
    parser.setErrorCallback([](const std::string& msg){
        g_wasm_output += msg;
    });
    
    auto statements = parser.parse();
    
    manifast::vm::VM vm;
    vm.setTier((manifast::vm::Tier)tier);
    
    vm.defineNative("print", wasm_print);
    vm.defineNative("println", wasm_println);
    vm.defineNative("assert", wasm_assert);
    vm.defineNative("len", wasm_len);
    vm.defineNative("clearOutput", wasm_clear_output);
    vm.defineNative("plotFor", wasm_plot_for);
    vm.defineNative("sleep", wasm_sleep);
    vm.defineNative("tunggu", wasm_sleep);

    // Module-style plot functions
    vm.defineNative("plot_line", wasm_plot_line);
    vm.defineNative("plot_scatter", wasm_plot_scatter);
    vm.defineNative("plot_bar", wasm_plot_bar);
    vm.defineNative("plot_show", wasm_plot_show);
    vm.defineNative("plot_reset", wasm_plot_reset);
    vm.defineNative("plot_save", wasm_plot_save);
    
#ifndef __EMSCRIPTEN__
    if (tier > 0) {
        manifast::CodeGen codegen;
        codegen.compile(statements);
        if (codegen.run()) {
        } else {
            g_wasm_output = "JIT/AOT Execution Failed";
        }
    } else {
#endif
        manifast::vm::Chunk chunk;
        manifast::vm::Compiler compiler;
        
        try {
            if (compiler.compile(statements, chunk)) {
                vm.interpret(&chunk, source);
                chunk.free();
            } else {
                g_wasm_output += "\n[ERROR] Compilation Failed\n";
            }
    } catch (const std::exception& e) {
        g_wasm_output += "\n[ERROR RUNTIME] ";
        g_wasm_output += e.what();
        g_wasm_output += "\n";
        if (chunk.code.size() > 0) chunk.free();
    }
    
    static std::string last_output;
    last_output = g_wasm_output;
    return last_output.c_str();
}

const char* mf_run_script(const char* source) {
    return mf_run_script_tier(source, 0);
}

}

