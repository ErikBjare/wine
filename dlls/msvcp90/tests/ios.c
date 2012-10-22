/*
 * Copyright 2010 Piotr Caban for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>
#include <locale.h>
#include <sys/stat.h>

#include <windef.h>
#include <winbase.h>
#include <share.h>
#include "wine/test.h"

static void* (__cdecl *p_set_invalid_parameter_handler)(void*);
static void  (__cdecl *p_free)(void*);

#undef __thiscall
#ifdef __i386__
#define __thiscall __stdcall
#else
#define __thiscall __cdecl
#endif

typedef unsigned char MSVCP_bool;
typedef SIZE_T MSVCP_size_t;
typedef SSIZE_T streamoff;
typedef SSIZE_T streamsize;

typedef void (*vtable_ptr)(void);

/* class mutex */
typedef struct {
        void *mutex;
} mutex;

/* class locale */
typedef struct
{
    struct _locale__Locimp *ptr;
} locale;

/* class locale::facet */
typedef struct {
    const vtable_ptr *vtable;
    MSVCP_size_t refs;
} locale_facet;

/* class codecvt_base */
typedef struct {
    locale_facet facet;
} codecvt_base;

/* class codecvt<char> */
typedef struct {
    codecvt_base base;
} codecvt_char;

typedef enum {
    FMTFLAG_skipws      = 0x0001,
    FMTFLAG_unitbuf     = 0x0002,
    FMTFLAG_uppercase   = 0x0004,
    FMTFLAG_showbase    = 0x0008,
    FMTFLAG_showpoint   = 0x0010,
    FMTFLAG_showpos     = 0x0020,
    FMTFLAG_left        = 0x0040,
    FMTFLAG_right       = 0x0080,
    FMTFLAG_internal    = 0x0100,
    FMTFLAG_dec         = 0x0200,
    FMTFLAG_oct         = 0x0400,
    FMTFLAG_hex         = 0x0800,
    FMTFLAG_scientific  = 0x1000,
    FMTFLAG_fixed       = 0x2000,
    FMTFLAG_hexfloat    = 0x3000,
    FMTFLAG_boolalpha   = 0x4000,
    FMTFLAG_stdio       = 0x8000,
    FMTFLAG_adjustfield = FMTFLAG_left|FMTFLAG_right|FMTFLAG_internal,
    FMTFLAG_basefield   = FMTFLAG_dec|FMTFLAG_oct|FMTFLAG_hex,
    FMTFLAG_floatfield  = FMTFLAG_scientific|FMTFLAG_fixed,
    FMTFLAG_mask        = 0xffff
} IOSB_fmtflags;

typedef enum {
    OPENMODE_in         = 0x01,
    OPENMODE_out        = 0x02,
    OPENMODE_ate        = 0x04,
    OPENMODE_app        = 0x08,
    OPENMODE_trunc      = 0x10,
    OPENMODE__Nocreate  = 0x40,
    OPENMODE__Noreplace = 0x80,
    OPENMODE_binary     = 0x20,
    OPENMODE_mask       = 0xff
} IOSB_openmode;

typedef enum {
    SEEKDIR_beg  = 0x0,
    SEEKDIR_cur  = 0x1,
    SEEKDIR_end  = 0x2,
    SEEKDIR_mask = 0x3
} IOSB_seekdir;

typedef enum {
    IOSTATE_goodbit   = 0x00,
    IOSTATE_eofbit    = 0x01,
    IOSTATE_failbit   = 0x02,
    IOSTATE_badbit    = 0x04,
    IOSTATE__Hardfail = 0x10,
    IOSTATE_mask      = 0x17
} IOSB_iostate;

typedef struct _iosarray {
    struct _iosarray *next;
    int index;
    int long_val;
    void *ptr_val;
} IOS_BASE_iosarray;

typedef enum {
    EVENT_erase_event,
    EVENT_imbue_event,
    EVENT_copyfmt_event
} IOS_BASE_event;

struct _ios_base;
typedef void (CDECL *IOS_BASE_event_callback)(IOS_BASE_event, struct _ios_base*, int);
typedef struct _fnarray {
    struct _fnarray *next;
    int index;
    IOS_BASE_event_callback event_handler;
} IOS_BASE_fnarray;

/* class ios_base */
typedef struct _ios_base {
    const vtable_ptr *vtable;
    MSVCP_size_t stdstr;
    IOSB_iostate state;
    IOSB_iostate except;
    IOSB_fmtflags fmtfl;
    streamsize prec;
    streamsize wide;
    IOS_BASE_iosarray *arr;
    IOS_BASE_fnarray *calls;
    locale *loc;
} ios_base;

/* class basic_streambuf<char> */
typedef struct {
    const vtable_ptr *vtable;
    mutex lock;
    char *rbuf;
    char *wbuf;
    char **prbuf;
    char **pwbuf;
    char *rpos;
    char *wpos;
    char **prpos;
    char **pwpos;
    int rsize;
    int wsize;
    int *prsize;
    int *pwsize;
    locale *loc;
} basic_streambuf_char;

/* class basic_streambuf<wchar> */
typedef struct {
    const vtable_ptr *vtable;
    mutex lock;
    wchar_t *rbuf;
    wchar_t *wbuf;
    wchar_t **prbuf;
    wchar_t **pwbuf;
    wchar_t *rpos;
    wchar_t *wpos;
    wchar_t **prpos;
    wchar_t **pwpos;
    int rsize;
    int wsize;
    int *prsize;
    int *pwsize;
    locale *loc;
} basic_streambuf_wchar;

typedef struct {
    basic_streambuf_char base;
    char *seekhigh;
    int state;
    char allocator; /* empty struct */
} basic_stringbuf_char;

typedef struct {
    basic_streambuf_wchar base;
    wchar_t *seekhigh;
    int state;
    char allocator; /* empty struct */
} basic_stringbuf_wchar;

typedef struct {
    ios_base base;
    basic_streambuf_char *strbuf;
    struct _basic_ostream_char *stream;
    char fillch;
} basic_ios_char;

typedef struct {
    ios_base base;
    basic_streambuf_wchar *strbuf;
    struct _basic_ostream_wchar *stream;
    wchar_t fillch;
} basic_ios_wchar;

typedef struct _basic_ostream_char {
    const int *vbtable;
    /* virtual inheritance
     * basic_ios_char basic_ios;
     */
} basic_ostream_char;

typedef struct _basic_ostream_wchar {
    const int *vbtable;
    /* virtual inheritance
     * basic_ios_wchar basic_ios;
     */
} basic_ostream_wchar;

typedef struct {
    const int *vbtable;
    streamsize count;
    /* virtual inheritance
     * basic_ios_char basic_ios;
     */
} basic_istream_char;

typedef struct {
    const int *vbtable;
    streamsize count;
    /* virtual inheritance
     * basic_ios_wchar basic_ios;
     */
} basic_istream_wchar;

typedef struct {
    basic_istream_char base1;
    basic_ostream_char base2;
    /* virtual inheritance
     * basic_ios_char basic_ios;
     */
} basic_iostream_char;

typedef struct {
    basic_istream_wchar base1;
    basic_ostream_wchar base2;
    /* virtual inheritance
     * basic_ios_wchar basic_ios;
     */
} basic_iostream_wchar;

typedef struct {
    basic_ostream_char base;
    basic_stringbuf_char strbuf;
    /* virtual inheritance
     * basic_ios_char basic_ios;
     */
} basic_ostringstream_char;

typedef struct {
    basic_ostream_wchar base;
    basic_stringbuf_wchar strbuf;
    /* virtual inheritance
     * basic_ios_wchar basic_ios;
     */
} basic_ostringstream_wchar;

typedef struct {
    basic_istream_char base;
    basic_stringbuf_char strbuf;
    /* virtual inheritance
     * basic_ios_char basic_ios;
     */
} basic_istringstream_char;

typedef struct {
    basic_istream_wchar base;
    basic_stringbuf_wchar strbuf;
    /* virtual inheritance
     * basic_ios_wchar basic_ios;
     */
} basic_istringstream_wchar;

typedef struct {
    basic_iostream_char base;
    basic_stringbuf_char strbuf;
    /* virtual inheritance */
    basic_ios_char basic_ios; /* here to reserve correct stack size */
} basic_stringstream_char;

typedef struct {
    basic_iostream_wchar base;
    basic_stringbuf_wchar strbuf;
    /* virtual inheritance */
    basic_ios_wchar basic_ios; /* here to reserve correct stack size */
} basic_stringstream_wchar;

/* basic_string<char, char_traits<char>, allocator<char>> */
#define BUF_SIZE_CHAR 16
typedef struct _basic_string_char
{
    void *allocator;
    union {
        char buf[BUF_SIZE_CHAR];
        char *ptr;
    } data;
    size_t size;
    size_t res;
} basic_string_char;

#define BUF_SIZE_WCHAR 8
typedef struct
{
    void *allocator;
    union {
        wchar_t buf[BUF_SIZE_WCHAR];
        wchar_t *ptr;
    } data;
    MSVCP_size_t size;
    MSVCP_size_t res;
} basic_string_wchar;

/* stringstream */
static basic_stringstream_char* (*__thiscall p_basic_stringstream_char_ctor)(basic_stringstream_char*, MSVCP_bool);
static basic_stringstream_char* (*__thiscall p_basic_stringstream_char_ctor_str)(basic_stringstream_char*, const basic_string_char*, int, MSVCP_bool);
static basic_string_char* (*__thiscall p_basic_stringstream_char_str_get)(const basic_stringstream_char*, basic_string_char*);
static void (*__thiscall p_basic_stringstream_char_vbase_dtor)(basic_stringstream_char*);

static basic_stringstream_wchar* (*__thiscall p_basic_stringstream_wchar_ctor)(basic_stringstream_wchar*, MSVCP_bool);
static basic_stringstream_wchar* (*__thiscall p_basic_stringstream_wchar_ctor_str)(basic_stringstream_wchar*, const basic_string_wchar*, int, MSVCP_bool);
static basic_string_wchar* (*__thiscall p_basic_stringstream_wchar_str_get)(const basic_stringstream_wchar*, basic_string_wchar*);
static void (*__thiscall p_basic_stringstream_wchar_vbase_dtor)(basic_stringstream_wchar*);

/* istream */
static basic_istream_char* (*__thiscall p_basic_istream_char_read_uint64)(basic_istream_char*, unsigned __int64*);
static basic_istream_char* (*__thiscall p_basic_istream_char_read_double)(basic_istream_char*, double*);
static int (*__thiscall p_basic_istream_char_get)(basic_istream_char*);

static basic_istream_wchar* (*__thiscall p_basic_istream_wchar_read_uint64)(basic_istream_wchar*, unsigned __int64*);
static basic_istream_wchar* (*__thiscall p_basic_istream_wchar_read_double)(basic_istream_wchar*, double *);
static int (*__thiscall p_basic_istream_wchar_get)(basic_istream_wchar*);

/* ostream */
static basic_ostream_char* (*__thiscall p_basic_ostream_char_print_double)(basic_ostream_char*, double);

static basic_ostream_wchar* (*__thiscall p_basic_ostream_wchar_print_double)(basic_ostream_wchar*, double);

/* basic_ios */
static locale*  (*__thiscall p_basic_ios_char_imbue)(basic_ios_char*, locale*, const locale*);

static locale*  (*__thiscall p_basic_ios_wchar_imbue)(basic_ios_wchar*, locale*, const locale*);

/* ios_base */
static IOSB_iostate  (*__thiscall p_ios_base_rdstate)(const ios_base*);
static IOSB_fmtflags (*__thiscall p_ios_base_setf_mask)(ios_base*, IOSB_fmtflags, IOSB_fmtflags);
static streamsize    (*__thiscall p_ios_base_precision_set)(ios_base*, streamsize);

/* locale */
static locale*  (*__thiscall p_locale_ctor_cstr)(locale*, const char*, int /* FIXME: category */);
static void     (*__thiscall p_locale_dtor)(locale *this);

/* basic_string */
static basic_string_char* (__thiscall *p_basic_string_char_ctor_cstr)(basic_string_char*, const char*);
static const char* (__thiscall *p_basic_string_char_cstr)(basic_string_char*);
static void (__thiscall *p_basic_string_char_dtor)(basic_string_char*);

static basic_string_wchar* (__thiscall *p_basic_string_wchar_ctor_cstr)(basic_string_wchar*, const wchar_t*);
static const wchar_t* (__thiscall *p_basic_string_wchar_cstr)(basic_string_wchar*);
static void (__thiscall *p_basic_string_wchar_dtor)(basic_string_wchar*);

static int invalid_parameter = 0;
static void __cdecl test_invalid_parameter_handler(const wchar_t *expression,
        const wchar_t *function, const wchar_t *file,
        unsigned line, uintptr_t arg)
{
    ok(expression == NULL, "expression is not NULL\n");
    ok(function == NULL, "function is not NULL\n");
    ok(file == NULL, "file is not NULL\n");
    ok(line == 0, "line = %u\n", line);
    ok(arg == 0, "arg = %lx\n", (UINT_PTR)arg);
    invalid_parameter++;
}

/* Emulate a __thiscall */
#ifdef __i386__

#include "pshpack1.h"
struct thiscall_thunk
{
    BYTE pop_eax;    /* popl  %eax (ret addr) */
    BYTE pop_edx;    /* popl  %edx (func) */
    BYTE pop_ecx;    /* popl  %ecx (this) */
    BYTE push_eax;   /* pushl %eax */
    WORD jmp_edx;    /* jmp  *%edx */
};
#include "poppack.h"

static void * (WINAPI *call_thiscall_func1)( void *func, void *this );
static void * (WINAPI *call_thiscall_func2)( void *func, void *this, const void *a );
static void * (WINAPI *call_thiscall_func3)( void *func, void *this, const void *a, const void *b );
static void * (WINAPI *call_thiscall_func4)( void *func, void *this, const void *a, const void *b,
        const void *c );
static void * (WINAPI *call_thiscall_func5)( void *func, void *this, const void *a, const void *b,
        const void *c, const void *d );

/* to silence compiler error about converting double to pointer */
static void * (WINAPI *call_thiscall_func2_ptr_dbl)( void *func, void *this, double a );

struct thiscall_thunk_retptr *thunk_retptr;

static void init_thiscall_thunk(void)
{
    struct thiscall_thunk *thunk = VirtualAlloc( NULL, sizeof(*thunk),
                                                 MEM_COMMIT, PAGE_EXECUTE_READWRITE );
    thunk->pop_eax  = 0x58;   /* popl  %eax */
    thunk->pop_edx  = 0x5a;   /* popl  %edx */
    thunk->pop_ecx  = 0x59;   /* popl  %ecx */
    thunk->push_eax = 0x50;   /* pushl %eax */
    thunk->jmp_edx  = 0xe2ff; /* jmp  *%edx */
    call_thiscall_func1 = (void *)thunk;
    call_thiscall_func2 = (void *)thunk;
    call_thiscall_func3 = (void *)thunk;
    call_thiscall_func4 = (void *)thunk;
    call_thiscall_func5 = (void *)thunk;

    call_thiscall_func2_ptr_dbl = (void *)thunk;
}

#define call_func1(func,_this) call_thiscall_func1(func,_this)
#define call_func2(func,_this,a) call_thiscall_func2(func,_this,(const void*)(a))
#define call_func3(func,_this,a,b) call_thiscall_func3(func,_this,(const void*)(a),(const void*)(b))
#define call_func4(func,_this,a,b,c) call_thiscall_func4(func,_this,(const void*)(a),(const void*)(b), \
        (const void*)(c))
#define call_func5(func,_this,a,b,c,d) call_thiscall_func5(func,_this,(const void*)(a),(const void*)(b), \
        (const void*)(c), (const void *)(d))

#define call_func2_ptr_dbl(func,_this,a) call_thiscall_func2_ptr_dbl(func,_this,a)

#else

#define init_thiscall_thunk()
#define call_func1(func,_this) func(_this)
#define call_func2(func,_this,a) func(_this,a)
#define call_func3(func,_this,a,b) func(_this,a,b)
#define call_func4(func,_this,a,b,c) func(_this,a,b,c)
#define call_func5(func,_this,a,b,c,d) func(_this,a,b,c,d)

#define call_func2_ptr_dbl  call_func2

#endif /* __i386__ */

#define SETNOFAIL(x,y) x = (void*)GetProcAddress(msvcp,y)
#define SET(x,y) do { SETNOFAIL(x,y); ok(x != NULL, "Export '%s' not found\n", y); } while(0)
static BOOL init(void)
{
    HMODULE msvcr = LoadLibraryA("msvcr90.dll");
    HMODULE msvcp = LoadLibraryA("msvcp90.dll");
    if(!msvcr || !msvcp) {
        win_skip("msvcp90.dll or msvcrt90.dll not installed\n");
        return FALSE;
    }

    p_set_invalid_parameter_handler = (void*)GetProcAddress(msvcr, "_set_invalid_parameter_handler");
    p_free = (void*)GetProcAddress(msvcr, "free");
    if(!p_set_invalid_parameter_handler || !p_free) {
        win_skip("Error setting tests environment\n");
        return FALSE;
    }

    p_set_invalid_parameter_handler(test_invalid_parameter_handler);

    if(sizeof(void*) == 8) { /* 64-bit initialization */
        SET(p_basic_stringstream_char_ctor,
            "??_F?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAXXZ");
        SET(p_basic_stringstream_char_ctor_str,
            "??0?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@H@Z");
        SET(p_basic_stringstream_char_str_get,
            "?str@?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@XZ");
        SET(p_basic_stringstream_char_vbase_dtor,
            "??_D?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAAXXZ");

        SET(p_basic_stringstream_wchar_ctor,
            "??_F?$basic_stringstream@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAXXZ");
        SET(p_basic_stringstream_wchar_ctor_str,
            "??0?$basic_stringstream@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAA@AEBV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@1@H@Z");
        SET(p_basic_stringstream_wchar_str_get,
            "?str@?$basic_stringstream@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBA?AV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@2@XZ");
        SET(p_basic_stringstream_wchar_vbase_dtor,
            "??_D?$basic_stringstream@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAAXXZ");

        SET(p_basic_istream_char_read_uint64,
            "??5?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@AEA_K@Z");
        SET(p_basic_istream_char_read_double,
            "??5?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@AEAN@Z");
        SET(p_basic_istream_char_get,
            "?get@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAHXZ");

        SET(p_basic_istream_wchar_read_uint64,
            "??5?$basic_istream@_WU?$char_traits@_W@std@@@std@@QEAAAEAV01@AEA_K@Z");
        SET(p_basic_istream_wchar_read_double,
            "??5?$basic_istream@_WU?$char_traits@_W@std@@@std@@QEAAAEAV01@AEAN@Z");
        SET(p_basic_istream_wchar_get,
            "?get@?$basic_istream@_WU?$char_traits@_W@std@@@std@@QEAAGXZ");

        SET(p_basic_ostream_char_print_double,
            "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@N@Z");

        SET(p_basic_ostream_wchar_print_double,
            "??6?$basic_ostream@_WU?$char_traits@_W@std@@@std@@QEAAAEAV01@N@Z");

        SET(p_ios_base_rdstate,
            "?rdstate@ios_base@std@@QEBAHXZ");
        SET(p_ios_base_setf_mask,
            "?setf@ios_base@std@@QEAAHHH@Z");
        SET(p_ios_base_precision_set,
            "?precision@ios_base@std@@QEAA_J_J@Z");

        SET(p_basic_ios_char_imbue,
            "?imbue@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAA?AVlocale@2@AEBV32@@Z");

        SET(p_basic_ios_wchar_imbue,
            "?imbue@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QEAA?AVlocale@2@AEBV32@@Z");

        SET(p_locale_ctor_cstr,
            "??0locale@std@@QEAA@PEBDH@Z");
        SET(p_locale_dtor,
            "??1locale@std@@QEAA@XZ");

        SET(p_basic_string_char_ctor_cstr,
                "??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@PEBD@Z");
        SET(p_basic_string_char_cstr,
                "?c_str@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEBAPEBDXZ");
        SET(p_basic_string_char_dtor,
                "??1?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QEAA@XZ");

        SET(p_basic_string_wchar_ctor_cstr,
                "??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAA@PEB_W@Z");
        SET(p_basic_string_wchar_cstr,
                "?c_str@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEBAPEB_WXZ");
        SET(p_basic_string_wchar_dtor,
                "??1?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QEAA@XZ");
    } else {
        SET(p_basic_stringstream_char_ctor,
            "??_F?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEXXZ");
        SET(p_basic_stringstream_char_ctor_str,
            "??0?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@H@Z");
        SET(p_basic_stringstream_char_str_get,
            "?str@?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBE?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@2@XZ");
        SET(p_basic_stringstream_char_vbase_dtor,
            "??_D?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAEXXZ");

        SET(p_basic_stringstream_wchar_ctor,
            "??_F?$basic_stringstream@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEXXZ");
        SET(p_basic_stringstream_wchar_ctor_str,
            "??0?$basic_stringstream@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@ABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@1@H@Z");
        SET(p_basic_stringstream_wchar_str_get,
            "?str@?$basic_stringstream@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBE?AV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@2@XZ");
        SET(p_basic_stringstream_wchar_vbase_dtor,
            "??_D?$basic_stringstream@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAEXXZ");

        SET(p_basic_istream_char_read_uint64,
            "??5?$basic_istream@DU?$char_traits@D@std@@@std@@QAEAAV01@AA_K@Z");
        SET(p_basic_istream_char_read_double,
            "??5?$basic_istream@DU?$char_traits@D@std@@@std@@QAEAAV01@AAN@Z");
        SET(p_basic_istream_char_get,
            "?get@?$basic_istream@DU?$char_traits@D@std@@@std@@QAEHXZ");

        SET(p_basic_istream_wchar_read_uint64,
            "??5?$basic_istream@_WU?$char_traits@_W@std@@@std@@QAEAAV01@AA_K@Z");
        SET(p_basic_istream_wchar_read_double,
            "??5?$basic_istream@_WU?$char_traits@_W@std@@@std@@QAEAAV01@AAN@Z");
        SET(p_basic_istream_wchar_get,
            "?get@?$basic_istream@_WU?$char_traits@_W@std@@@std@@QAEGXZ");

        SET(p_basic_ostream_char_print_double,
            "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QAEAAV01@N@Z");

        SET(p_basic_ostream_wchar_print_double,
            "??6?$basic_ostream@_WU?$char_traits@_W@std@@@std@@QAEAAV01@N@Z");

        SET(p_ios_base_rdstate,
            "?rdstate@ios_base@std@@QBEHXZ");
        SET(p_ios_base_setf_mask,
            "?setf@ios_base@std@@QAEHHH@Z");
        SET(p_ios_base_precision_set,
            "?precision@ios_base@std@@QAEHH@Z");

        SET(p_basic_ios_char_imbue,
            "?imbue@?$basic_ios@DU?$char_traits@D@std@@@std@@QAE?AVlocale@2@ABV32@@Z");

        SET(p_basic_ios_wchar_imbue,
            "?imbue@?$basic_ios@_WU?$char_traits@_W@std@@@std@@QAE?AVlocale@2@ABV32@@Z");

        SET(p_locale_ctor_cstr,
            "??0locale@std@@QAE@PBDH@Z");
        SET(p_locale_dtor,
            "??1locale@std@@QAE@XZ");

        SET(p_basic_string_char_ctor_cstr,
                "??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@PBD@Z");
        SET(p_basic_string_char_cstr,
                "?c_str@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QBEPBDXZ");
        SET(p_basic_string_char_dtor,
                "??1?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@XZ");

        SET(p_basic_string_wchar_ctor_cstr,
                "??0?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@PB_W@Z");
        SET(p_basic_string_wchar_cstr,
                "?c_str@?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QBEPB_WXZ");
        SET(p_basic_string_wchar_dtor,
                "??1?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@QAE@XZ");
    }

    init_thiscall_thunk();
    return TRUE;
}

/* convert a dll name A->W without depending on the current codepage */
static wchar_t *AtoW( wchar_t *nameW, const char *nameA, unsigned int len )
{
    unsigned int i;

    for (i = 0; i < len; i++) nameW[i] = nameA[i];
    nameW[i] = 0;
    return nameW;
}

static void test_num_get_get_uint64(void)
{
    unsigned short testus, nextus;
    basic_stringstream_wchar wss;
    basic_stringstream_char ss;
    basic_string_wchar wstr;
    basic_string_char str;
    IOSB_iostate state;
    locale lcl, retlcl;
    wchar_t wide[64];
    ULONGLONG val;
    int i, next;

    /* makes tables narrower */
    const IOSB_iostate IOSTATE_faileof = IOSTATE_failbit|IOSTATE_eofbit;

    struct _test_num_get {
        const char    *str;
        const char    *lcl;
        IOSB_fmtflags fmtfl;
        IOSB_iostate  state;
        ULONGLONG     val;
        int           next;
    } tests[] = {
        /* simple cases */
        { "0",        NULL, FMTFLAG_dec, IOSTATE_eofbit,  0,       EOF },
        { "1234567",  NULL, FMTFLAG_dec, IOSTATE_eofbit,  1234567, EOF },
        { "+1234567", NULL, FMTFLAG_dec, IOSTATE_eofbit,  1234567, EOF },
        { "-1234567", NULL, FMTFLAG_dec, IOSTATE_eofbit, -1234567, EOF },
        { "",         NULL, FMTFLAG_dec, IOSTATE_faileof, 42,      EOF },

        /* different bases */
        /* (with and without zero are both tested, since 0 can signal a prefix check) */
        { "0x1000",   NULL, FMTFLAG_hex, IOSTATE_eofbit, 4096, EOF }, /* lowercase x */
        { "0X1000",   NULL, FMTFLAG_hex, IOSTATE_eofbit, 4096, EOF }, /* uppercase X */
        { "010",      NULL, FMTFLAG_hex, IOSTATE_eofbit, 16,   EOF },
        { "010",      NULL, FMTFLAG_dec, IOSTATE_eofbit, 10,   EOF },
        { "010",      NULL, FMTFLAG_oct, IOSTATE_eofbit, 8,    EOF },
        { "10",       NULL, FMTFLAG_hex, IOSTATE_eofbit, 16,   EOF },
        { "10",       NULL, FMTFLAG_dec, IOSTATE_eofbit, 10,   EOF },
        { "10",       NULL, FMTFLAG_oct, IOSTATE_eofbit, 8,    EOF },
        { "10",       NULL, 0,           IOSTATE_eofbit, 10,   EOF }, /* discover dec */
        { "010",      NULL, 0,           IOSTATE_eofbit, 8,    EOF }, /* discover oct */
        { "0xD",      NULL, 0,           IOSTATE_eofbit, 13,   EOF }, /* discover hex (upper) */
        { "0xd",      NULL, 0,           IOSTATE_eofbit, 13,   EOF }, /* discover hex (lower) */

        /* test grouping - default/"C" has no grouping, named English/German locales do */
        { "0.", NULL,       FMTFLAG_dec, IOSTATE_goodbit, 0,  '.' },
        { "0,", NULL,       FMTFLAG_dec, IOSTATE_goodbit, 0,  ',' },
        { "0,", "English",  FMTFLAG_dec, IOSTATE_faileof, 42, EOF }, /* trailing group with , */
        { "0.", "German",   FMTFLAG_dec, IOSTATE_faileof, 42, EOF }, /* trailing group with . */
        { "0,", "German",   FMTFLAG_dec, IOSTATE_goodbit, 0,  ',' },
        { ",0", "English",  FMTFLAG_dec, IOSTATE_failbit, 42, EOF }, /* empty group at start */

        { "1,234,567",   NULL,      FMTFLAG_dec, IOSTATE_goodbit, 1,        ',' }, /* no grouping */
        { "1,234,567",   "English", FMTFLAG_dec, IOSTATE_eofbit,  1234567,  EOF }, /* grouping with , */
        { "1.234.567",   "German",  FMTFLAG_dec, IOSTATE_eofbit,  1234567,  EOF }, /* grouping with . */
        { "1,,234",      NULL,      FMTFLAG_dec, IOSTATE_goodbit, 1,        ',' }, /* empty group */
        { "1,,234",      "English", FMTFLAG_dec, IOSTATE_failbit, 42,       EOF }, /* empty group */
        { "0x1,000,000", "English", FMTFLAG_hex, IOSTATE_eofbit,  16777216, EOF }, /* yeah, hex can group */
        { "1,23,34",     "English", FMTFLAG_dec, IOSTATE_faileof, 42,       EOF }, /* invalid size group */
        { "0,123",       "English", FMTFLAG_dec, IOSTATE_eofbit,  123,      EOF }, /* 0 solo in group */

        { "18446744073709551615", NULL, FMTFLAG_dec, IOSTATE_eofbit,  ~0, EOF }, /* max value */
        { "99999999999999999999", NULL, FMTFLAG_dec, IOSTATE_faileof, 42, EOF }, /* invalid value */

        /* test invalid formats */
        { "0000x10", NULL, FMTFLAG_hex, IOSTATE_goodbit, 0,  'x' },
        { "x10",     NULL, FMTFLAG_hex, IOSTATE_failbit, 42, EOF },
        { "0xx10",   NULL, FMTFLAG_hex, IOSTATE_failbit, 42, EOF },
    };

    for(i=0; i<sizeof(tests)/sizeof(tests[0]); i++) {
        /* char version */
        call_func2(p_basic_string_char_ctor_cstr, &str, tests[i].str);
        call_func4(p_basic_stringstream_char_ctor_str, &ss, &str, OPENMODE_out|OPENMODE_in, TRUE);

        if(tests[i].lcl) {
            call_func3(p_locale_ctor_cstr, &lcl, tests[i].lcl, 0x3f /* FIXME: support categories */);
            call_func3(p_basic_ios_char_imbue, &ss.basic_ios, &retlcl, &lcl);
        }

        val = 42;
        call_func3(p_ios_base_setf_mask, &ss.basic_ios.base, tests[i].fmtfl, FMTFLAG_basefield);
        call_func2(p_basic_istream_char_read_uint64, &ss.base.base1, &val);
        state = (IOSB_iostate)call_func1(p_ios_base_rdstate, &ss.basic_ios.base);
        next  = (int)call_func1(p_basic_istream_char_get, &ss.base.base1);

        if(strlen(tests[i].str) == 0) {
            /* a later patch to istream<>::_Ipfx will handle empty string */
            todo_wine ok(tests[i].state == state, "wrong state, expected = %x found = %x\n", tests[i].state, state);
        } else
            ok(tests[i].state == state, "wrong state, expected = %x found = %x\n", tests[i].state, state);
        ok(tests[i].val   == val,   "wrong val, expected = %llu found %llu\n", (unsigned long long)tests[i].val, (unsigned long long)val);
        ok(tests[i].next  == next,  "wrong next, expected = %c (%i) found = %c (%i)\n", tests[i].next, tests[i].next, next, next);

        if(tests[i].lcl)
            call_func1(p_locale_dtor, &lcl);

        call_func1(p_basic_stringstream_char_vbase_dtor, &ss);
        call_func1(p_basic_string_char_dtor, &str);

        /* wchar_t version */
        AtoW(wide, tests[i].str, strlen(tests[i].str));
        call_func2(p_basic_string_wchar_ctor_cstr, &wstr, wide);
        call_func4(p_basic_stringstream_wchar_ctor_str, &wss, &wstr, OPENMODE_out|OPENMODE_in, TRUE);

        if(tests[i].lcl) {
            call_func3(p_locale_ctor_cstr, &lcl, tests[i].lcl, 0x3f /* FIXME: support categories */);
            call_func3(p_basic_ios_wchar_imbue, &wss.basic_ios, &retlcl, &lcl);
        }

        val = 42;
        call_func3(p_ios_base_setf_mask, &wss.basic_ios.base, tests[i].fmtfl, FMTFLAG_basefield);
        call_func2(p_basic_istream_wchar_read_uint64, &wss.base.base1, &val);
        state = (IOSB_iostate)call_func1(p_ios_base_rdstate, &wss.basic_ios.base);
        nextus = (unsigned short)(int)call_func1(p_basic_istream_wchar_get, &wss.base.base1);

        if(strlen(tests[i].str) == 0) {
            /* a later patch to istream<>::_Ipfx will handle empty string */
            todo_wine ok(tests[i].state == state, "wrong state, expected = %x found = %x\n", tests[i].state, state);
        } else
            ok(tests[i].state == state, "wrong state, expected = %x found = %x\n", tests[i].state, state);
        ok(tests[i].val == val, "wrong val, expected = %llu found %llu\n", (unsigned long long)tests[i].val, (unsigned long long)val);
        testus = tests[i].next == EOF ? WEOF : (unsigned short)tests[i].next;
        ok(testus == nextus, "wrong next, expected = %c (%i) found = %c (%i)\n", testus, testus, nextus, nextus);

        if(tests[i].lcl)
            call_func1(p_locale_dtor, &lcl);

        call_func1(p_basic_stringstream_wchar_vbase_dtor, &wss);
        call_func1(p_basic_string_wchar_dtor, &wstr);
    }
}


static void test_num_get_get_double(void)
{
    unsigned short testus, nextus;
    basic_stringstream_wchar wss;
    basic_stringstream_char ss;
    basic_string_wchar wstr;
    basic_string_char str;
    IOSB_iostate state;
    locale lcl, retlcl;
    wchar_t wide[64];
    int i, next;
    double val;

    /* makes tables narrower */
    const IOSB_iostate IOSTATE_faileof = IOSTATE_failbit|IOSTATE_eofbit;

    struct _test_num_get {
        const char    *str;
        const char    *lcl;
        IOSB_iostate  state;
        double        val;
        int           next;
    } tests[] = {
        /* simple cases */
        { "0",     NULL, IOSTATE_eofbit,  0.0,  EOF },
        { "10",    NULL, IOSTATE_eofbit,  10.0, EOF },
        { "+10",   NULL, IOSTATE_eofbit,  10.0, EOF },
        { "-10",   NULL, IOSTATE_eofbit, -10.0, EOF },
        { "+010",  NULL, IOSTATE_eofbit,  10.0, EOF }, /* leading zero */

        /* test grouping - default/"C" has no grouping, named English/German locales do */
        { "1,000", NULL,         IOSTATE_goodbit,  1.0,      ',' }, /* with comma */
        { "1,000", "English",    IOSTATE_eofbit,   1000.0,   EOF },
        { "1,000", "German",     IOSTATE_eofbit,   1.0,      EOF },

        { "1.000", NULL,         IOSTATE_eofbit,   1.0,      EOF }, /* with period */
        { "1.000", "English",    IOSTATE_eofbit,   1.0,      EOF },
        { "1.000", "German",     IOSTATE_eofbit,   1000.0,   EOF },

        { "1,234.",  NULL,       IOSTATE_goodbit,  1.0,      ',' },
        { "1,234.",  "English",  IOSTATE_eofbit,   1234.0,   EOF }, /* trailing decimal */
        { "1,234.",  "German",   IOSTATE_goodbit,  1.234,    '.' },
        { "1,234.5", "English",  IOSTATE_eofbit,   1234.5,   EOF }, /* group + decimal */
        { "1,234.5", "German",   IOSTATE_goodbit,  1.234,    '.' },

        { "1,234,567,890", NULL,      IOSTATE_goodbit, 1.0,          ',' }, /* more groups */
        { "1,234,567,890", "English", IOSTATE_eofbit,  1234567890.0, EOF },
        { "1,234,567,890", "German",  IOSTATE_goodbit, 1.234,        ',' },
        { "1.234.567.890", "German",  IOSTATE_eofbit,  1234567890.0, EOF },

        /* extra digits and stuff */
        { "00000.123456", NULL,  IOSTATE_eofbit,  0.123456, EOF },
        { "0.1234560000", NULL,  IOSTATE_eofbit,  0.123456, EOF },
        { "100aaaa",      NULL,  IOSTATE_goodbit, 100.0,    'a' },

        /* exponent */
        { "10e10",       NULL,      IOSTATE_eofbit,    10e10,      EOF }, /* lowercase e */
        { "10E10",       NULL,      IOSTATE_eofbit,    10E10,      EOF }, /* uppercase E */
        { "10e+10",      NULL,      IOSTATE_eofbit,    10e10,      EOF }, /* sign */
        { "10e-10",      NULL,      IOSTATE_eofbit,    10e-10,     EOF },
        { "10.e10",      NULL,      IOSTATE_eofbit,    10e10,      EOF }, /* trailing decimal before exponent */
        { "-10.e-10",    NULL,      IOSTATE_eofbit,   -10e-10,     EOF },
        { "-12.345e-10", NULL,      IOSTATE_eofbit,   -12.345e-10, EOF },
        { "1,234e10",    NULL,      IOSTATE_goodbit,   1.0,        ',' },
        { "1,234e10",    "English", IOSTATE_eofbit,    1234.0e10,  EOF },
        { "1,234e10",    "German",  IOSTATE_eofbit,    1.234e10,   EOF },
        { "1.0e999",     NULL,      IOSTATE_faileof,   42.0,       EOF }, /* too big   */
        { "1.0e-999",    NULL,      IOSTATE_faileof,   42.0,       EOF }, /* too small */

        /* bad form */
        { "1,000,", NULL,       IOSTATE_goodbit, 1.0,   ',' }, /* trailing group */
        { "1,000,", "English",  IOSTATE_faileof, 42.0,  EOF },
        { "1.000.", "German",   IOSTATE_faileof, 42.0,  EOF },

        { "1,,000", NULL,       IOSTATE_goodbit, 1.0,   ',' }, /* empty group */
        { "1,,000", "English",  IOSTATE_failbit, 42.0,  EOF },
        { "1..000", "German",   IOSTATE_failbit, 42.0,  EOF },

        { "1.0,00", "English",  IOSTATE_goodbit, 1.0,   ',' },
        { "1.0,00", "German",   IOSTATE_faileof, 42.0,  EOF },

        { "1.0ee10", NULL,      IOSTATE_failbit, 42.0,  EOF }, /* dup exp */
        { "1.0e1.0", NULL,      IOSTATE_goodbit, 10.0,  '.' }, /* decimal in exponent */
        { "1.0e1,0", NULL,      IOSTATE_goodbit, 10.0,  ',' }, /* group in exponent */
    };

    for(i=0; i<sizeof(tests)/sizeof(tests[0]); i++) {
        /* char version */
        call_func2(p_basic_string_char_ctor_cstr, &str, tests[i].str);
        call_func4(p_basic_stringstream_char_ctor_str, &ss, &str, OPENMODE_out|OPENMODE_in, TRUE);

        if(tests[i].lcl) {
            call_func3(p_locale_ctor_cstr, &lcl, tests[i].lcl, 0x3f /* FIXME: support categories */);
            call_func3(p_basic_ios_char_imbue, &ss.basic_ios, &retlcl, &lcl);
        }

        val = 42.0;
        call_func2(p_basic_istream_char_read_double, &ss.base.base1, &val);
        state = (IOSB_iostate)call_func1(p_ios_base_rdstate, &ss.basic_ios.base);
        next  = (int)call_func1(p_basic_istream_char_get, &ss.base.base1);

        if(strlen(tests[i].str) == 0) {
            /* a later patch to istream<>::_Ipfx will handle empty string */
            todo_wine ok(tests[i].state == state, "wrong state, expected = %x found = %x\n", tests[i].state, state);
        } else
            ok(tests[i].state == state, "wrong state, expected = %x found = %x\n", tests[i].state, state);
        ok(tests[i].val   == val,   "wrong val, expected = %g found %g\n", tests[i].val, val);
        ok(tests[i].next  == next,  "wrong next, expected = %c (%i) found = %c (%i)\n", tests[i].next, tests[i].next, next, next);

        if(tests[i].lcl)
            call_func1(p_locale_dtor, &lcl);

        call_func1(p_basic_stringstream_char_vbase_dtor, &ss);
        call_func1(p_basic_string_char_dtor, &str);

        /* wchar_t version */
        AtoW(wide, tests[i].str, strlen(tests[i].str));
        call_func2(p_basic_string_wchar_ctor_cstr, &wstr, wide);
        call_func4(p_basic_stringstream_wchar_ctor_str, &wss, &wstr, OPENMODE_out|OPENMODE_in, TRUE);

        if(tests[i].lcl) {
            call_func3(p_locale_ctor_cstr, &lcl, tests[i].lcl, 0x3f /* FIXME: support categories */);
            call_func3(p_basic_ios_wchar_imbue, &wss.basic_ios, &retlcl, &lcl);
        }

        val = 42.0;
        call_func2(p_basic_istream_wchar_read_double, &wss.base.base1, &val);
        state = (IOSB_iostate)call_func1(p_ios_base_rdstate, &wss.basic_ios.base);
        nextus = (unsigned short)(int)call_func1(p_basic_istream_wchar_get, &wss.base.base1);

        if(strlen(tests[i].str) == 0) {
            /* a later patch to istream<>::_Ipfx will handle empty string */
            todo_wine ok(tests[i].state == state, "wrong state, expected = %x found = %x\n", tests[i].state, state);
        } else
            ok(tests[i].state == state, "wrong state, expected = %x found = %x\n", tests[i].state, state);
        ok(tests[i].val == val, "wrong val, expected = %g found %g\n", tests[i].val, val);
        testus = tests[i].next == EOF ? WEOF : (unsigned short)tests[i].next;
        ok(testus == nextus, "wrong next, expected = %c (%i) found = %c (%i)\n", testus, testus, nextus, nextus);

        if(tests[i].lcl)
            call_func1(p_locale_dtor, &lcl);

        call_func1(p_basic_stringstream_wchar_vbase_dtor, &wss);
        call_func1(p_basic_string_wchar_dtor, &wstr);
    }
}


static void test_num_put_put_double(void)
{
    basic_stringstream_wchar wss;
    basic_stringstream_char ss;
    basic_string_wchar pwstr;
    basic_string_char pstr;
    locale lcl, retlcl;
    const wchar_t *wstr;
    const char *str;
    wchar_t wide[64];
    int i;

    struct _test_num_get {
        double        val;
        const char    *lcl;
        streamsize    prec;  /* set to -1 for default */
        IOSB_fmtflags fmtfl; /* FMTFLAG_scientific, FMTFLAG_fixed */
        const char    *str;
    } tests[] = {
        { 0.0, NULL, -1, 0, "0" },

        /* simple cases */
        { 0.123, NULL, -1, 0, "0.123" },
        { 0.123, NULL,  6, 0, "0.123" },
        { 0.123, NULL,  0, 0, "0.123" },

        /* fixed format */
        { 0.123, NULL, -1, FMTFLAG_fixed, "0.123000" },
        { 0.123, NULL,  6, FMTFLAG_fixed, "0.123000" },
        { 0.123, NULL,  0, FMTFLAG_fixed, "0" },

        /* scientific format */
        { 123456.789, NULL,    -1, FMTFLAG_scientific, "1.234568e+005"    },
        { 123456.789, NULL,     0, FMTFLAG_scientific, "1.234568e+005"    },
        { 123456.789, NULL,     9, FMTFLAG_scientific, "1.234567890e+005" },
        { 123456.789, "German", 9, FMTFLAG_scientific, "1,234567890e+005" },

        /* different locales */
        { 0.123, "C",       -1, 0, "0.123" },
        { 0.123, "English", -1, 0, "0.123" },
        { 0.123, "German",  -1, 0, "0,123" },

        { 123456.789, "C",       -1, 0, "123457"  },
        { 123456.789, "English", -1, 0, "123,457" },
        { 123456.789, "German",  -1, 0, "123.457" },

        /* signs and exponents */
        {  1.0e-9, NULL, -1, 0, "1e-009"  },
        {  1.0e-9, NULL,  9, 0, "1e-009"  },
        { -1.0e9,  NULL, -1, 0, "-1e+009" },
        { -1.0e9,  NULL,  9, 0, "-1e+009" },

        {  1.0e-9, NULL, 0, FMTFLAG_fixed, "0"                  },
        {  1.0e-9, NULL, 6, FMTFLAG_fixed, "0.000000"           },
        {  1.0e-9, NULL, 9, FMTFLAG_fixed, "0.000000001"        },
        { -1.0e9,  NULL, 0, FMTFLAG_fixed, "-1000000000"        },
        { -1.0e9,  NULL, 6, FMTFLAG_fixed, "-1000000000.000000" },

        { -1.23456789e9,  NULL, 0, 0,             "-1.23457e+009"      },
        { -1.23456789e9,  NULL, 0, FMTFLAG_fixed, "-1234567890"        },
        { -1.23456789e9,  NULL, 6, FMTFLAG_fixed, "-1234567890.000000" },
        { -1.23456789e-9, NULL, 6, FMTFLAG_fixed, "-0.000000"          },
        { -1.23456789e-9, NULL, 9, FMTFLAG_fixed, "-0.000000001"       }
    };

    for(i=0; i<sizeof(tests)/sizeof(tests[0]); i++) {
        /* char version */
        call_func2(p_basic_stringstream_char_ctor, &ss, TRUE);

        if(tests[i].lcl) {
            call_func3(p_locale_ctor_cstr, &lcl, tests[i].lcl, 0x3f /* FIXME: support categories */);
            call_func3(p_basic_ios_char_imbue, &ss.basic_ios, &retlcl, &lcl);
        }

        /* set format and precision only if specified, so we can try defaults */
        if(tests[i].fmtfl)
            call_func3(p_ios_base_setf_mask, &ss.basic_ios.base, tests[i].fmtfl, FMTFLAG_floatfield);
        if(tests[i].prec != -1)
            call_func2(p_ios_base_precision_set, &ss.basic_ios.base, tests[i].prec);
        call_func2_ptr_dbl(p_basic_ostream_char_print_double, &ss.base.base2, tests[i].val);

        call_func2(p_basic_stringstream_char_str_get, &ss, &pstr);
        str = call_func1(p_basic_string_char_cstr, &pstr);

        ok(!strcmp(tests[i].str, str), "wrong output, expected = %s found = %s\n", tests[i].str, str);

        if(tests[i].lcl)
            call_func1(p_locale_dtor, &lcl);

        call_func1(p_basic_stringstream_char_vbase_dtor, &ss);

        /* wchar_t version */
        call_func2(p_basic_stringstream_wchar_ctor, &wss, TRUE);

        if(tests[i].lcl) {
            call_func3(p_locale_ctor_cstr, &lcl, tests[i].lcl, 0x3f /* FIXME: support categories */);
            call_func3(p_basic_ios_wchar_imbue, &wss.basic_ios, &retlcl, &lcl);
        }

        /* set format and precision only if specified, so we can try defaults */
        if(tests[i].fmtfl)
            call_func3(p_ios_base_setf_mask, &wss.basic_ios.base, tests[i].fmtfl, FMTFLAG_floatfield);
        if(tests[i].prec != -1)
            call_func2(p_ios_base_precision_set, &wss.basic_ios.base, tests[i].prec);
        call_func2_ptr_dbl(p_basic_ostream_wchar_print_double, &wss.base.base2, tests[i].val);

        call_func2(p_basic_stringstream_wchar_str_get, &wss, &pwstr);
        wstr = call_func1(p_basic_string_wchar_cstr, &pwstr);

        AtoW(wide, tests[i].str, strlen(tests[i].str));
        ok(!lstrcmpW(wide, wstr), "wrong output, expected = %s found = %s\n", tests[i].str, str);

        if(tests[i].lcl)
            call_func1(p_locale_dtor, &lcl);

        call_func1(p_basic_stringstream_wchar_vbase_dtor, &wss);
    }
}

START_TEST(ios)
{
    if(!init())
        return;

    test_num_get_get_uint64();
    test_num_get_get_double();
    test_num_put_put_double();

    ok(!invalid_parameter, "invalid_parameter_handler was invoked too many times\n");
}