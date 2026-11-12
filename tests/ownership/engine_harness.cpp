/*
 * Synthetic 4D engine boundary for ownership regressions.
 *
 * Compiles the real plugin entry point (4DPlugin-Speech-Synthesizer.cpp,
 * included below) and links the real support/4DPlugin-JSON.cpp, every class
 * of the bundled 4D Plugin API and the unmodified 4DPluginAPI.c. Only gCall4D
 * is replaced. Every engine resource (Unistrings, pictures, objects/
 * collections, handles) is a real heap block tracked in a live set, so leaks
 * are detected exactly and a dispose of something the engine no longer owns
 * (double free) is reported by name instead of being freed a second time.
 *
 * No speech is ever produced: PA_RunInMainProcess is answered by the
 * synthetic engine, which records the say_ctx and never runs speech::say,
 * NSSpeechSynthesizer +availableVoices returns a synthetic list, and
 * -startSpeakingString:(toURL:) abort the harness if they are ever reached.
 *
 * Engine semantics modelled (per the SDK contract):
 *  - PA_CreateVariable(eVK_Unistring/eVK_Picture) allocates an empty value.
 *  - PA_GetStringVariable returns a view borrowed from the variable.
 *  - PA_SetStringInArray takes ownership of the string it stores.
 *  - PA_SetObjectProperty deep-copies strings and pictures and retains
 *    objects/collections; the caller still owns its variable.
 *  - PA_GetObjectProperty returns an owned copy (or a retained object/
 *    collection) that must be cleared; on error the result is left
 *    uninitialised (poisoned here) and must be neither read nor cleared.
 *  - PA_ExecuteCommandByID returns an owned result.
 */

/* Resolved through -I <source-root>, so the same harness tests baseline and fixed trees. */
#include "4DPlugin-Speech-Synthesizer.cpp"

#include "PrivateTypes.h"
#include "EntryPoints.h"

#include <objc/runtime.h>

#include <map>
#include <set>
#include <string>
#include <vector>

extern "C" Call4DProcPtr gCall4D;

namespace {

typedef std::basic_string<PA_Unichar> u16;

struct Obj {
    int rc;
    bool coll;
    std::map<u16, PA_Variable> props;
    std::vector<PA_Variable> items;
};

/* data first: the SDK dereferences array handles directly (*(T**)handle). */
struct Handle { char *data; size_t size; };

std::set<void *> g_strings;
std::set<void *> g_pictures;
std::set<Obj *> g_objects;
std::set<void *> g_handles;
std::vector<std::string> g_violations;

/* PA_GetObjectProperty for this key fails: eb.fError is set and the result
   is left uninitialised. The poison is readable (tests fail by assertion,
   not by crashing) but never engine-owned, so clearing it is counted. */
std::string g_fail_key;
/* JSON Stringify (1217) fails the same way. */
bool g_fail_stringify = false;
const char kPoisonMark = 0x5A;
int g_poison_clears = 0;
PA_Unichar kPoisonBuf[] = { 'B', 'A', 'D', '!', 0 };
PA_Unichar *const kPoison = kPoisonBuf;

/* What SAY handed to PA_RunInMainProcess (speech::say is never run). */
struct SayCall { bool called; std::string text; std::string voice; bool has_voice; bool has_url; float rate; float volume; };
SayCall g_say;

PA_Variable undefined_var();

void poison(PA_Variable *out) {
    *out = undefined_var();
    out->fFiller = kPoisonMark;
    out->fType = eVK_Unistring;
    out->uValue.fString.fString = kPoison;
    out->uValue.fString.fLength = 4;
}

void violation(const std::string &what) {
    fprintf(stderr, "  ENGINE CONTRACT: %s\n", what.c_str());
    g_violations.push_back(what);
}

[[noreturn]] void die(const char *msg) {
    fprintf(stderr, "HARNESS ERROR: %s\n", msg);
    fflush(stderr);
    abort();
}

std::string narrow(const u16 &s) { std::string r; for(PA_Unichar c : s) r.push_back((char)c); return r; }
u16 widen(const std::string &s) { u16 r; for(char c : s) r.push_back((PA_Unichar)(unsigned char)c); return r; }

PA_Unistring new_string(const PA_Unichar *src, size_t len) {
    PA_Unichar *buf = (PA_Unichar *)malloc((len + 1) * sizeof(PA_Unichar));
    if(len) memcpy(buf, src, len * sizeof(PA_Unichar));
    buf[len] = 0;
    g_strings.insert(buf);
    PA_Unistring s;
    s.fLength = (PA_long32)len;
    s.fString = buf;
    s.fReserved1 = 0;
    s.fReserved2 = 0;
    return s;
}

void free_string(PA_Unistring *s) {
    if(!s->fString) return;
    if(s->fString == kPoison) {
        g_poison_clears++;
        fprintf(stderr, "  cleared the uninitialised result of a failed engine call\n");
    } else if(!g_strings.erase(s->fString)) {
        violation("double_dispose_string");
    } else {
        free(s->fString);
    }
    s->fString = 0;
    s->fLength = 0;
}

void *new_picture() {
    void *p = malloc(16);
    g_pictures.insert(p);
    return p;
}

void free_picture(void *p) {
    if(!p) return;
    if(!g_pictures.erase(p)) { violation("double_dispose_picture"); return; }
    free(p);
}

Handle *new_handle(size_t size) {
    Handle *h = (Handle *)malloc(sizeof(Handle));
    h->size = size;
    h->data = (char *)calloc(1, size ? size : 1);
    g_handles.insert(h);
    return h;
}

void free_handle(Handle *h) {
    if(!h) return;
    if(!g_handles.erase(h)) { violation("double_dispose_handle"); return; }
    free(h->data);
    free(h);
}

Handle *live_handle(void *p) {
    if(!g_handles.count(p)) die("used an unknown handle");
    return (Handle *)p;
}

void release_object(Obj *o);

void clear_var(PA_Variable *v) {
    if(v->fFiller == kPoisonMark) {
        g_poison_clears++;
        fprintf(stderr, "  cleared the uninitialised result of a failed engine call\n");
        v->fType = eVK_Undefined;
        v->fFiller = 0;
        return;
    }
    switch(v->fType) {
        case eVK_Unistring: free_string(&v->uValue.fString); break;
        case eVK_Picture: free_picture(v->uValue.fPicture); break;
        case eVK_Blob: free_handle((Handle *)v->uValue.fBlob.fHandle); break;
        case eVK_Object: if(v->uValue.fObject) release_object((Obj *)v->uValue.fObject); break;
        case eVK_Collection: if(v->uValue.fCollection) release_object((Obj *)v->uValue.fCollection); break;
        case eVK_ArrayUnicode: {
            Handle *h = live_handle(v->uValue.fArray.fData);
            PA_Unistring *s = (PA_Unistring *)h->data;
            for(PA_long32 i = 0; i <= v->uValue.fArray.fNbElements; i++) free_string(&s[i]);
            free_handle(h);
            break;
        }
        default: break;
    }
    v->fType = eVK_Undefined;
}

Obj *live(void *p) {
    if(!g_objects.count((Obj *)p)) die("used a dead object/collection");
    return (Obj *)p;
}

PA_Variable undefined_var() { PA_Variable v; memset(&v, 0, sizeof(v)); v.fType = eVK_Undefined; return v; }

PA_Variable copy_var(const PA_Variable &src) {
    PA_Variable dst = src;
    switch(src.fType) {
        case eVK_Unistring:
            dst.uValue.fString = new_string(src.uValue.fString.fString, src.uValue.fString.fLength);
            break;
        case eVK_Picture:
            if(!g_pictures.count(PA_Picture(src.uValue.fPicture))) die("stored a dead picture");
            dst.uValue.fPicture = new_picture();
            break;
        case eVK_Object:
            if(src.uValue.fObject) live(src.uValue.fObject)->rc++;
            break;
        case eVK_Collection:
            if(src.uValue.fCollection) live(src.uValue.fCollection)->rc++;
            break;
        default: break;
    }
    return dst;
}

Obj *new_object(bool coll = false) {
    Obj *o = new Obj();
    o->rc = 1;
    o->coll = coll;
    g_objects.insert(o);
    return o;
}

void release_object(Obj *o) {
    if(!g_objects.count(o)) { violation("double_release_object"); return; }
    if(--o->rc == 0) {
        for(auto &kv : o->props) clear_var(&kv.second);
        for(auto &v : o->items) clear_var(&v);
        g_objects.erase(o);
        delete o;
    }
}

u16 key_of(const PA_Unistring &k) { return u16(k.fString, k.fLength); }

PA_Variable object_result(Obj *o) {
    PA_Variable v = undefined_var();
    v.fType = o->coll ? eVK_Collection : eVK_Object;
    if(o->coll) v.uValue.fCollection = (PA_CollectionRef)o; else v.uValue.fObject = (PA_ObjectRef)o;
    return v;
}

void execute_command(EngineBlock *eb) {
    PA_Variable **params = (PA_Variable **)eb->fPtr1;
    PA_Variable *ret = (PA_Variable *)eb->fPtr2;
    switch(eb->fParam1) {
        case 1225: { /* OB Copy */
            Obj *src = live(params[0]->uValue.fObject);
            Obj *dst = new_object(false);
            for(auto &kv : src->props) dst->props[kv.first] = copy_var(kv.second);
            *ret = object_result(dst);
            break;
        }
        case 1217: { /* JSON Stringify */
            live(params[0]->uValue.fObject);
            if(g_fail_stringify) { poison(ret); eb->fError = -1; break; }
            u16 json = widen("{}");
            ret->fType = eVK_Unistring;
            ret->uValue.fString = new_string(json.c_str(), json.size());
            break;
        }
        case 1231: { /* OB Is defined */
            Obj *o = live(params[0]->uValue.fObject);
            ret->fType = eVK_Boolean;
            ret->uValue.fBoolean = o->props.count(key_of(params[1]->uValue.fString)) ? 1 : 0;
            break;
        }
        case 1471: *ret = object_result(new_object(false)); break; /* New Object */
        case 1472: *ret = object_result(new_object(true)); break; /* New Collection */
        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "unexpected PA_ExecuteCommandByID %d", (int)eb->fParam1);
            die(msg);
        }
    }
}

std::string utf8(NSString *s) { return s ? std::string([s UTF8String]) : std::string(); }

void run_in_main_process(EngineBlock *eb) {
    /* Never call eb->fPtr1 (speech::say): record the context and release
       the Cocoa objects SAY handed over, as speech::say would. */
    if(eb->fPtr1 != (void *)speech::say) die("unexpected PA_RunInMainProcess target");
    speech::say_ctx *ctx = (speech::say_ctx *)eb->fPtr2;
    g_say.called = true;
    g_say.text = utf8(ctx->text);
    g_say.has_voice = ctx->voice != nil;
    g_say.voice = utf8(ctx->voice);
    g_say.has_url = ctx->url != nil;
    g_say.rate = ctx->rate;
    g_say.volume = ctx->volume;
    [ctx->text release];
    [ctx->url release];
    [ctx->voice release];
}

void engine(short selector, EngineBlock *eb) {
    switch(selector) {
        case EX_CREATE_UNISTRING: {
            const PA_Unichar *src = (const PA_Unichar *)eb->fHandle;
            size_t len = 0;
            while(src[len]) len++;
            eb->fUniString1 = new_string(src, len);
            break;
        }
        case EX_DISPOSE_UNISTRING:
            free_string(&eb->fUniString1);
            break;
        case EX_VARIABLE_TO_STRING: {
            PA_Variable *v = (PA_Variable *)eb->fHandle;
            if(v->fType == eVK_Unistring) eb->fUniString1 = v->uValue.fString; /* borrowed */
            break;
        }
        case EX_CREATE_PICTURE:
            eb->fPicture = new_picture();
            break;
        case EX_DISPOSE_PICTURE:
            free_picture(eb->fPicture);
            break;
        case EX_CLEAR_VARIABLE:
            clear_var((PA_Variable *)eb->fPtr1);
            break;
        case EX_GET_INFORMATION:
            eb->fParam2 = 0x00002000; /* 4D v20: PA_CreateObject uses New Object */
            break;
        case EX_RUN_IN_MAIN_PROCESS:
            run_in_main_process(eb);
            break;
        case EX_SET_OBJ_VALUE: {
            Obj *o = live(eb->fPtr1);
            if(eb->fShort == eVK_Collection) die("collection set not modelled");
            PA_Variable value = copy_var(*(PA_Variable *)eb->fPtr2);
            u16 k = key_of(eb->fUniString1);
            auto it = o->props.find(k);
            if(it != o->props.end()) clear_var(&it->second);
            o->props[k] = value;
            break;
        }
        case EX_GET_OBJ_VALUE: {
            if(!eb->fPtr1) { violation("property_of_null_object"); *(PA_Variable *)eb->fPtr2 = undefined_var(); eb->fError = -1; break; }
            Obj *o = live(eb->fPtr1);
            PA_Variable *out = (PA_Variable *)eb->fPtr2;
            if(eb->fShort == eVK_Collection) die("collection get not modelled");
            u16 k = key_of(eb->fUniString1);
            if(!g_fail_key.empty() && narrow(k) == g_fail_key) {
                poison(out);
                eb->fError = -1;
                break;
            }
            auto it = o->props.find(k);
            *out = it == o->props.end() ? undefined_var() : copy_var(it->second);
            break;
        }
        case EX_EXECUTE_COMMAND_BY_ID:
            execute_command(eb);
            break;
        case EX_HANDLE_MANAGER: {
            switch(eb->fParam1) {
                case 1: eb->fHandle = (PA_Handle)new_handle((size_t)eb->fParam2); break;
                case 2: free_handle((Handle *)eb->fHandle); break;
                case 3: {
                    Handle *h = live_handle(eb->fHandle);
                    size_t n = (size_t)eb->fParam2;
                    char *d = (char *)calloc(1, n ? n : 1);
                    memcpy(d, h->data, n < h->size ? n : h->size);
                    free(h->data);
                    h->data = d;
                    h->size = n;
                    break;
                }
                case 4: eb->fParam2 = (sLONG_PTR)live_handle(eb->fHandle)->size; break;
                case 5: eb->fParam3 = (sLONG_PTR)live_handle(eb->fHandle)->data; break;
                case 6: break;
                default: die("unexpected handle manager op");
            }
            break;
        }
        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "unmodelled engine selector %d", selector);
            die(msg);
        }
    }
}

/* ---------- Cocoa stubs: no voices are queried and nothing is spoken ---------- */

id stub_available_voices(id, SEL) {
    return @[ @"com.example.synthetic.alpha", @"com.example.synthetic.beta" ];
}
BOOL stub_start_speaking(id, SEL, NSString *) { die("NSSpeechSynthesizer started speaking"); }
BOOL stub_start_speaking_to_url(id, SEL, NSString *, NSURL *) { die("NSSpeechSynthesizer started speaking"); }

void install_cocoa_stubs() {
    Class c = [NSSpeechSynthesizer class];
    method_setImplementation(class_getClassMethod(c, @selector(availableVoices)), (IMP)stub_available_voices);
    method_setImplementation(class_getInstanceMethod(c, @selector(startSpeakingString:)), (IMP)stub_start_speaking);
    method_setImplementation(class_getInstanceMethod(c, @selector(startSpeakingString:toURL:)), (IMP)stub_start_speaking_to_url);
}

/* ---------- test helpers ---------- */

PA_Unistring U(const char *ascii) {
    u16 s = widen(ascii);
    return new_string(s.c_str(), s.size());
}

void put(Obj *o, const char *key, PA_Variable v) {
    PA_Unistring k = U(key);
    PA_SetObjectProperty((PA_ObjectRef)o, &k, v);
    PA_DisposeUnistring(&k);
}

void put_text(Obj *o, const char *key, const char *value) {
    PA_Variable v = undefined_var();
    PA_Unistring u = U(value);
    PA_SetStringVariable(&v, &u);
    put(o, key, v);
    PA_ClearVariable(&v);
}

void put_real(Obj *o, const char *key, double d) {
    PA_Variable v = undefined_var();
    PA_SetRealVariable(&v, d);
    put(o, key, v);
}

Obj *put_object(Obj *o, const char *key, bool coll) {
    Obj *c = new_object(coll);
    PA_Variable v = object_result(c);
    put(o, key, v);
    PA_ClearVariable(&v);
    return c;
}

bool check(bool ok, const char *what) {
    if(!ok) fprintf(stderr, "  check failed: %s\n", what);
    return ok;
}

bool no_leaks() {
    bool ok = true;
    for(void *p : g_strings) {
        fprintf(stderr, "  LEAK: engine string \"%s\"\n", narrow(u16((PA_Unichar *)p)).c_str());
        ok = false;
    }
    if(!g_pictures.empty()) { fprintf(stderr, "  LEAK: %zu engine picture(s)\n", g_pictures.size()); ok = false; }
    if(!g_objects.empty()) { fprintf(stderr, "  LEAK: %zu engine object(s)/collection(s)\n", g_objects.size()); ok = false; }
    if(!g_handles.empty()) { fprintf(stderr, "  LEAK: %zu engine handle(s)\n", g_handles.size()); ok = false; }
    if(g_poison_clears) { fprintf(stderr, "  %d failed result(s) cleared\n", g_poison_clears); ok = false; }
    if(!g_violations.empty()) ok = false;
    return ok;
}

/* Calls the real `SAY` command (selector 2) with a borrowed options object. */
void call_say(const char *text, Obj *options, const char *path = "") {
    PA_Unistring p1 = U(text);
    PA_ObjectRef p2 = (PA_ObjectRef)options;
    PA_Unistring p3 = U(path);
    void *args[3] = { &p1, &p2, &p3 };
    PluginBlock pb;
    pb.fParameters = args;
    pb.fResult = NULL;
    g_say = SayCall();
    PluginMain(2, &pb);
    PA_DisposeUnistring(&p1);
    PA_DisposeUnistring(&p3);
}

/* Calls the real `VOICES LIST` command (selector 1) on a 4D-owned array. */
void call_voices_list(PA_Variable *arr) {
    void *args[1] = { arr };
    PluginBlock pb;
    pb.fParameters = args;
    pb.fResult = NULL;
    PluginMain(1, &pb);
}

std::vector<std::string> array_strings(const PA_Variable &arr) {
    std::vector<std::string> r;
    if(arr.fType != eVK_ArrayUnicode) return r;
    PA_Unistring *s = (PA_Unistring *)live_handle(arr.uValue.fArray.fData)->data;
    for(PA_long32 i = 0; i <= arr.uValue.fArray.fNbElements; i++)
        r.push_back(s[i].fString ? narrow(u16(s[i].fString, s[i].fLength)) : "<null>");
    return r;
}

/* ---------- LIVE defects (SAY) ---------- */

/* LIVE: SAY -> ob_get_s never cleared the owned copy of options.voice. */
bool defect_live_say_voice_option_not_cleared() {
    Obj *o = new_object(false);
    put_text(o, "voice", "com.example.synthetic.alpha");
    call_say("hello", o);
    bool ok = check(g_say.called && g_say.has_voice && g_say.voice == "com.example.synthetic.alpha", "voice handed to speech::say");
    ok &= check(g_say.text == "hello", "text handed to speech::say");
    ok &= check(g_objects.count(o) && o->rc == 1, "caller's options object untouched");
    release_object(o);
    return no_leaks() && ok;
}

/* LIVE: SAY -> ob_get_n never cleared a non-real options.rate/volume copy. */
bool defect_live_say_rate_wrong_kind_not_cleared() {
    Obj *o = new_object(false);
    put_text(o, "rate", "fast");
    put_text(o, "volume", "loud");
    call_say("hello", o);
    bool ok = check(g_say.called && g_say.rate == 0.0f && g_say.volume == 0.0f, "wrong kinds yield 0");
    release_object(o);
    return no_leaks() && ok;
}

/* HARDENING (LIVE): a failed PA_GetObjectProperty leaves its result
   uninitialised; SAY must neither read it as the voice nor clear it. */
bool hardening_live_say_voice_error_untouched() {
    Obj *o = new_object(false);
    put_text(o, "voice", "com.example.synthetic.alpha");
    g_fail_key = "voice";
    call_say("hello", o);
    g_fail_key.clear();
    bool ok = check(g_say.called && g_say.voice != "BAD!", "uninitialised result not used as the voice");
    release_object(o);
    return no_leaks() && ok;
}

/* ---------- DORMANT defects (support/4DPlugin-JSON.cpp, no caller) ---------- */

bool defect_dormant_ob_get_a_result_not_cleared() {
    Obj *o = new_object(false);
    put_text(o, "s", "text");
    CUTF16String s;
    bool r = ob_get_a((PA_ObjectRef)o, L"s", &s);
    release_object(o);
    bool ok = check(r && s == widen("text"), "value read");
    return no_leaks() && ok;
}

bool defect_dormant_ob_get_d_wrong_kind_not_cleared() {
    Obj *o = new_object(false);
    put_text(o, "d", "2025-01-01");
    short dd = -1, mm = -1, yyyy = -1;
    bool r = ob_get_d((PA_ObjectRef)o, L"d", &dd, &mm, &yyyy);
    release_object(o);
    bool ok = check(!r, "wrong kind is not a date");
    return no_leaks() && ok;
}

bool defect_dormant_ob_get_b_wrong_kind_not_cleared() {
    Obj *o = new_object(false);
    put_text(o, "flag", "yes");
    bool b = ob_get_b((PA_ObjectRef)o, L"flag");
    release_object(o);
    bool ok = check(!b, "wrong kind yields false");
    return no_leaks() && ok;
}

/* The retained object/collection reference was never released, so it
   outlived its owner. The returned ref is borrowed from the parent. */
bool defect_dormant_ob_get_o_c_reference_not_released() {
    Obj *o = new_object(false);
    Obj *child = put_object(o, "child", false);
    Obj *items = put_object(o, "items", true);
    PA_ObjectRef r1 = ob_get_o((PA_ObjectRef)o, L"child");
    PA_CollectionRef r2 = ob_get_c((PA_ObjectRef)o, L"items");
    bool ok = check(r1 == (PA_ObjectRef)child && r2 == (PA_CollectionRef)items, "references returned");
    ok &= check(child->rc == 1 && items->rc == 1, "returned references borrowed from the parent");
    release_object(o);
    return no_leaks() && ok;
}

/* ob_set_s/ob_set_a/ob_set_p overwrote the value allocated by
   PA_CreateVariable(eVK_Unistring/eVK_Picture). */
bool defect_dormant_ob_set_empty_owner_leaks() {
    Obj *o = new_object(false);
    ob_set_s((PA_ObjectRef)o, "k1", "v1");
    ob_set_s((PA_ObjectRef)o, L"k2", "v2");
    ob_set_a((PA_ObjectRef)o, L"k3", L"v3");
    CUTF16String v4 = widen("v4");
    ob_set_a((PA_ObjectRef)o, L"k4", &v4);
    PA_Picture pic = PA_CreatePicture(NULL, 0);
    // ob_set_p consumes `pic` (shallow set, deep-copying set, then clear).
    ob_set_p((PA_ObjectRef)o, L"k5", pic);
    bool ok = check(o->props.size() == 5, "five properties set");
    release_object(o);
    return no_leaks() && ok;
}

/* HARDENING: on error PA_GetObjectProperty leaves its result uninitialised. */
bool hardening_ob_get_error_result_untouched() {
    Obj *o = new_object(false);
    put_text(o, "a", "x");
    put_real(o, "n", 1.0);
    g_fail_key = "a";
    CUTF16String s;
    CUTF8String s8;
    bool ra = ob_get_a((PA_ObjectRef)o, L"a", &s);
    bool rs = ob_get_s((PA_ObjectRef)o, L"a", &s8);
    g_fail_key = "n";
    double d = ob_get_n((PA_ObjectRef)o, L"n");
    g_fail_key.clear();
    release_object(o);
    bool ok = check(!ra && !rs && s.empty() && s8.empty() && d == 0.0, "failed reads yield nothing");
    return no_leaks() && ok;
}

/* HARDENING: ob_stringify cleared the 1217 result without checking the error. */
bool hardening_ob_stringify_error_untouched() {
    Obj *o = new_object(false);
    put_text(o, "a", "x");
    g_fail_stringify = true;
    CUTF8String json;
    ob_stringify((PA_ObjectRef)o, &json);
    g_fail_stringify = false;
    release_object(o);
    bool ok = check(json.empty(), "failed stringify yields nothing");
    return no_leaks() && ok;
}

/* ---------- controls (pass on both trees) ---------- */

/* VOICES LIST: PA_SetStringInArray takes ownership, nothing is disposed after it. */
bool control_voices_list_new_array() {
    PA_Variable arr = undefined_var();
    call_voices_list(&arr);
    std::vector<std::string> got = array_strings(arr);
    bool ok = check(arr.fType == eVK_ArrayUnicode && got == std::vector<std::string>({ "", "com.example.synthetic.alpha", "com.example.synthetic.beta" }), "voices returned");
    PA_ClearVariable(&arr);
    return no_leaks() && ok;
}

bool control_voices_list_overwrites_existing_array() {
    PA_Variable arr = PA_CreateVariable(eVK_ArrayUnicode);
    PA_ResizeArray(&arr, 4);
    for(PA_long32 i = 1; i <= 4; i++) {
        char s[8];
        snprintf(s, sizeof(s), "old%d", (int)i);
        PA_Unistring u = U(s);
        PA_SetStringInArray(arr, i, &u);
    }
    call_voices_list(&arr);
    std::vector<std::string> got = array_strings(arr);
    bool ok = check(got == std::vector<std::string>({ "", "com.example.synthetic.alpha", "com.example.synthetic.beta" }), "old elements replaced");
    PA_ClearVariable(&arr);
    return no_leaks() && ok;
}

bool control_say_real_options() {
    Obj *o = new_object(false);
    put_real(o, "rate", 200.0);
    put_real(o, "volume", 0.5);
    call_say("hello", o);
    bool ok = check(g_say.called && g_say.rate == 200.0f && g_say.volume == 0.5f && !g_say.has_voice, "real options honoured");
    ok &= check(g_objects.count(o) && o->rc == 1, "caller's options object untouched");
    release_object(o);
    return no_leaks() && ok;
}

bool control_say_no_options() {
    call_say("hello", NULL);
    bool ok = check(g_say.called && g_say.rate == 180.0f && g_say.volume == 0.0f && !g_say.has_voice, "defaults");
    return no_leaks() && ok;
}

bool control_ob_set_get_round_trip() {
    Obj *o = new_object(false);
    ob_set_n((PA_ObjectRef)o, L"n", 2.5);
    ob_set_b((PA_ObjectRef)o, L"b", true);
    ob_set_i((PA_ObjectRef)o, L"i", 7);
    ob_set_0((PA_ObjectRef)o, L"z");
    ob_set_o((PA_ObjectRef)o, L"o", PA_CreateObject());
    ob_set_c((PA_ObjectRef)o, L"c", PA_CreateCollection());
    bool ok = check(o->props.size() == 6, "six properties set");
    ok &= check(ob_get_n((PA_ObjectRef)o, L"n") == 2.5 && ob_get_b((PA_ObjectRef)o, L"b"), "real and boolean read back");
    ok &= check(ob_is_defined((PA_ObjectRef)o, L"n") && !ob_is_defined((PA_ObjectRef)o, L"x"), "ob_is_defined");
    release_object(o);
    return no_leaks() && ok;
}

bool control_ob_stringify() {
    Obj *o = new_object(false);
    put_text(o, "a", "x");
    CUTF8String json;
    ob_stringify((PA_ObjectRef)o, &json);
    release_object(o);
    bool ok = check(json == (const uint8_t *)"{}", "stringified");
    return no_leaks() && ok;
}

struct Test { const char *name; bool (*fn)(); };
const Test kTests[] = {
    { "defect_live_say_voice_option_not_cleared", defect_live_say_voice_option_not_cleared },
    { "defect_live_say_rate_wrong_kind_not_cleared", defect_live_say_rate_wrong_kind_not_cleared },
    { "hardening_live_say_voice_error_untouched", hardening_live_say_voice_error_untouched },
    { "defect_dormant_ob_get_a_result_not_cleared", defect_dormant_ob_get_a_result_not_cleared },
    { "defect_dormant_ob_get_d_wrong_kind_not_cleared", defect_dormant_ob_get_d_wrong_kind_not_cleared },
    { "defect_dormant_ob_get_b_wrong_kind_not_cleared", defect_dormant_ob_get_b_wrong_kind_not_cleared },
    { "defect_dormant_ob_get_o_c_reference_not_released", defect_dormant_ob_get_o_c_reference_not_released },
    { "defect_dormant_ob_set_empty_owner_leaks", defect_dormant_ob_set_empty_owner_leaks },
    { "hardening_ob_get_error_result_untouched", hardening_ob_get_error_result_untouched },
    { "hardening_ob_stringify_error_untouched", hardening_ob_stringify_error_untouched },
    { "control_voices_list_new_array", control_voices_list_new_array },
    { "control_voices_list_overwrites_existing_array", control_voices_list_overwrites_existing_array },
    { "control_say_real_options", control_say_real_options },
    { "control_say_no_options", control_say_no_options },
    { "control_ob_set_get_round_trip", control_ob_set_get_round_trip },
    { "control_ob_stringify", control_ob_stringify },
};

} // namespace

int main(int argc, char **argv) {
    @autoreleasepool {
        gCall4D = engine;
        install_cocoa_stubs();
        if(argc == 2 && !strcmp(argv[1], "--list")) {
            for(const Test &t : kTests) printf("%s\n", t.name);
            return 0;
        }
        if(argc != 2) { fprintf(stderr, "usage: %s --list | <test>\n", argv[0]); return 2; }
        for(const Test &t : kTests) {
            if(!strcmp(argv[1], t.name)) {
                bool ok = t.fn();
                printf("%s: %s\n", t.name, ok ? "PASS" : "FAIL");
                return ok ? 0 : 1;
            }
        }
        fprintf(stderr, "unknown test %s\n", argv[1]);
        return 2;
    }
}
