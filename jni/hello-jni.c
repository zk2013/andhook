#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <string.h>
#include <jni.h>
#include <fcntl.h>

#ifndef LOG_TAG
# define LOG_TAG "andhook"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>    /* C99 */
typedef uint8_t             u1;
typedef uint16_t            u2;
typedef uint32_t            u4;
typedef uint64_t            u8;
typedef int8_t              s1;
typedef int16_t             s2;
typedef int32_t             s4;
typedef int64_t             s8;
#else
typedef unsigned char       u1;
typedef unsigned short      u2;
typedef unsigned int        u4;
typedef unsigned long long  u8;
typedef signed char         s1;
typedef signed short        s2;
typedef signed int          s4;
typedef signed long long    s8;
#endif

#ifndef __CPP__
#ifndef __bool_true_false_are_defined
typedef enum { false=0, true=!false } bool;
#define __bool_true_false_are_defined 1
#endif
#endif

#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

typedef union JValue {
    u1      z;
    s1      b;
    u2      c;
    s2      s;
    s4      i;
    s8      j;
    float   f;
    double  d;
    void*   l;
} JValue;

#define ALLOC_DEFAULT  0x00
#define ALLOC_DONT_TRACK 0x02

struct StringObject;
struct ArrayObject;

typedef struct DexProto {
    u4* dexFile;     /* file the idx refers to */
    u4 protoIdx;                /* index into proto_ids table of dexFile */
} DexProto;

typedef void (*DalvikBridgeFunc)(const u4* args, void* pResult,
    const void* method, void* self);

struct Field {
   void*    clazz;          /* class in which the field is declared */
    const char*     name;
    const char*     signature;      /* e.g. "I", "[C", "Landroid/os/Debug;" */
    u4              accessFlags;
};

struct Method;
struct ClassObject;

struct Object {
    /* ptr to class object */
    struct ClassObject*    clazz;

    /*
     * A word containing either a "thin" lock or a "fat" monitor.  See
     * the comments in Sync.c for a description of its layout.
     */
    u4              lock;
};

struct InitiatingLoaderList {
    /* a list of initiating loader Objects; grown and initialized on demand */
    void**  initiatingLoaders;
    /* count of loaders in the above list */
    int       initiatingLoaderCount;
};

enum PrimitiveType {
    PRIM_NOT        = 0,       /* value is a reference type, not a primitive type */
    PRIM_VOID       = 1,
    PRIM_BOOLEAN    = 2,
    PRIM_BYTE       = 3,
    PRIM_SHORT      = 4,
    PRIM_CHAR       = 5,
    PRIM_INT        = 6,
    PRIM_LONG       = 7,
    PRIM_FLOAT      = 8,
    PRIM_DOUBLE     = 9,
} typedef PrimitiveType;

enum ClassStatus {
    CLASS_ERROR         = -1,

    CLASS_NOTREADY      = 0,
    CLASS_IDX           = 1,    /* loaded, DEX idx in super or ifaces */
    CLASS_LOADED        = 2,    /* DEX idx values resolved */
    CLASS_RESOLVED      = 3,    /* part of linking */
    CLASS_VERIFYING     = 4,    /* in the process of being verified */
    CLASS_VERIFIED      = 5,    /* logically part of linking; done pre-init */
    CLASS_INITIALIZING  = 6,    /* class init in progress */
    CLASS_INITIALIZED   = 7,    /* ready to go */
} typedef ClassStatus;

struct ClassObject {
    struct Object o; // emulate C++ inheritance, Collin
	
    /* leave space for instance data; we could access fields directly if we
       freeze the definition of java/lang/Class */
    u4              instanceData[4];

    /* UTF-8 descriptor for the class; from constant pool, or on heap
       if generated ("[C") */
    const char*     descriptor;
    char*           descriptorAlloc;

    /* access flags; low 16 bits are defined by VM spec */
    u4              accessFlags;

    /* VM-unique class serial number, nonzero, set very early */
    u4              serialNumber;

    /* DexFile from which we came; needed to resolve constant pool entries */
    /* (will be NULL for VM-generated, e.g. arrays and primitive classes) */
    void*         pDvmDex;

    /* state of class initialization */
    ClassStatus     status;

    /* if class verify fails, we must return same error on subsequent tries */
    struct ClassObject*    verifyErrorClass;

    /* threadId, used to check for recursive <clinit> invocation */
    u4              initThreadId;

    /*
     * Total object size; used when allocating storage on gc heap.  (For
     * interfaces and abstract classes this will be zero.)
     */
    size_t          objectSize;

    /* arrays only: class object for base element, for instanceof/checkcast
       (for String[][][], this will be String) */
    struct ClassObject*    elementClass;

    /* arrays only: number of dimensions, e.g. int[][] is 2 */
    int             arrayDim;
 	PrimitiveType   primitiveType;

    /* superclass, or NULL if this is java.lang.Object */
    struct ClassObject*    super;

    /* defining class loader, or NULL for the "bootstrap" system loader */
    struct Object*         classLoader;
	
	struct InitiatingLoaderList initiatingLoaderList;

    /* array of interfaces this class implements directly */
    int             interfaceCount;
    struct ClassObject**   interfaces;

    /* static, private, and <init> methods */
    int             directMethodCount;
    struct Method*         directMethods;

    /* virtual methods defined in this class; invoked through vtable */
    int             virtualMethodCount;
    struct Method*         virtualMethods;

    /*
     * Virtual method table (vtable), for use by "invoke-virtual".  The
     * vtable from the superclass is copied in, and virtual methods from
     * our class either replace those from the super or are appended.
     */
    int             vtableCount;
    struct Method**        vtable;

};
	
typedef struct Method {
	struct ClassObject *clazz;
	u4 a; // accessflags
	
	u2             methodIndex;
	
	u2              registersSize;  /* ins + locals */
    u2              outsSize;
    u2              insSize;

    /* method name, e.g. "<init>" or "eatLunch" */
    const char*     name;

    /*
     * Method prototype descriptor string (return and argument types).
     *
     * TODO: This currently must specify the DexFile as well as the proto_ids
     * index, because generated Proxy classes don't have a DexFile.  We can
     * remove the DexFile* and reduce the size of this struct if we generate
     * a DEX for proxies.
     */
    DexProto        prototype;

    /* short-form method descriptor string */
    const char*     shorty;

    /*
     * The remaining items are not used for abstract or native methods.
     * (JNI is currently hijacking "insns" as a function pointer, set
     * after the first call.  For internal-native this stays null.)
     */

    /* the actual code */
    u2*       insns;
	
	 /* cached JNI argument and return-type hints */
    int             jniArgInfo;

    /*
     * Native method ptr; could be actual function or a JNI bridge.  We
     * don't currently discriminate between DalvikBridgeFunc and
     * DalvikNativeFunc; the former takes an argument superset (i.e. two
     * extra args) which will be ignored.  If necessary we can use
     * insns==NULL to detect JNI bridge vs. internal native.
     */
    DalvikBridgeFunc  nativeFunc;

#ifdef WITH_PROFILER
    bool            inProfile;
#endif
#ifdef WITH_DEBUGGER
    short           debugBreakpointCount;
#endif

  bool fastJni;

    /*
     * JNI: true if this method has no reference arguments. This lets the JNI
     * bridge avoid scanning the shorty for direct pointers that need to be
     * converted to local references.
     *
     * TODO: replace this with a list of indexes of the reference arguments.
     */
    bool noRef;

	
} Method;

typedef void (*DalvikNativeFunc)(const u4* args, jvalue* pResult);

typedef struct DalvikNativeMethod_t {
    const char* name;
    const char* signature;
    DalvikNativeFunc  fnPtr;
} DalvikNativeMethod;

typedef void* (*dvmCreateStringFromCstr_func)(const char* utf8Str, int len, int allocFlags);
typedef void* (*dvmGetSystemClassLoader_func)(void);
typedef void* (*dvmThreadSelf_func)(void);

typedef void* (*dvmIsClassInitialized_func)(void*);
typedef void* (*dvmInitClass_func)(void*);
typedef void* (*dvmFindVirtualMethodHierByDescriptor_func)(void*,const char*, const char*);
typedef void* (*dvmFindDirectMethodByDescriptor_func)(void*,const char*, const char*);
typedef void* (*dvmIsStaticMethod_func)(void*);
typedef void* (*dvmAllocObject_func)(void*, unsigned int);
typedef void* (*dvmCallMethodV_func)(void*,void*,void*,void*,va_list);
typedef void* (*dvmCallMethodA_func)(void*,void*,void*,bool,void*,jvalue*);
typedef void* (*dvmAddToReferenceTable_func)(void*,void*);
typedef void (*dvmDumpAllClasses_func)(int);
typedef void* (*dvmFindLoadedClass_func)(const char*);

typedef void (*dvmUseJNIBridge_func)(void*, void*);

typedef void* (*dvmDecodeIndirectRef_func)(void*,void*);

typedef void* (*dvmGetCurrentJNIMethod_func)();

typedef void (*dvmLinearSetReadWrite_func)(void*,void*);

typedef void* (*dvmFindInstanceField_func)(void*,const char*,const char*);

typedef void* (*dvmSetNativeFunc_func)(void*,void*, void*);
typedef void (*dvmCallJNIMethod_func)(const u4*, void*, void*, void*);

typedef void (*dvmHashTableLock_func)(void*);
typedef void (*dvmHashTableUnlock_func)(void*);
typedef void (*dvmHashForeach_func)(void*,void*,void*);

typedef void (*dvmDumpClass_func)(void*,void*);

typedef int (*dvmInstanceof_func)(void*,void*);

struct dexstuff_t
{	
	void *dvm_hand;
	
	dvmCreateStringFromCstr_func dvmStringFromCStr_fnPtr;
	dvmGetSystemClassLoader_func dvmGetSystemClassLoader_fnPtr;
	dvmThreadSelf_func dvmThreadSelf_fnPtr;

	dvmIsClassInitialized_func dvmIsClassInitialized_fnPtr;
	dvmInitClass_func dvmInitClass_fnPtr;
	dvmFindVirtualMethodHierByDescriptor_func dvmFindVirtualMethodHierByDescriptor_fnPtr;
	dvmFindDirectMethodByDescriptor_func dvmFindDirectMethodByDescriptor_fnPtr;
	dvmIsStaticMethod_func dvmIsStaticMethod_fnPtr;
	dvmAllocObject_func dvmAllocObject_fnPtr;
	dvmCallMethodV_func dvmCallMethodV_fnPtr;
	dvmCallMethodA_func dvmCallMethodA_fnPtr;
	dvmAddToReferenceTable_func dvmAddToReferenceTable_fnPtr;
	dvmDecodeIndirectRef_func dvmDecodeIndirectRef_fnPtr;
	dvmUseJNIBridge_func dvmUseJNIBridge_fnPtr;
	dvmFindInstanceField_func dvmFindInstanceField_fnPtr;
	dvmFindLoadedClass_func dvmFindLoadedClass_fnPtr;
	dvmDumpAllClasses_func dvmDumpAllClasses_fnPtr;
	
	dvmGetCurrentJNIMethod_func dvmGetCurrentJNIMethod_fnPtr;
	dvmLinearSetReadWrite_func dvmLinearSetReadWrite_fnPtr;
	
	dvmSetNativeFunc_func dvmSetNativeFunc_fnPtr;
	dvmCallJNIMethod_func dvmCallJNIMethod_fnPtr;
	
	dvmHashTableLock_func dvmHashTableLock_fnPtr;
	dvmHashTableUnlock_func dvmHashTableUnlock_fnPtr;
	dvmHashForeach_func dvmHashForeach_fnPtr;
	
	dvmDumpClass_func dvmDumpClass_fnPtr;
	dvmInstanceof_func dvmInstanceof_fnPtr;
	
	DalvikNativeMethod *dvm_dalvik_system_DexFile;
	DalvikNativeMethod *dvm_java_lang_Class;
		
	void *gDvm; // dvm globals !
	
	int done;
};

void dexstuff_resolv_dvm(struct dexstuff_t *d);
int dexstuff_loaddex(struct dexstuff_t *d, char *path);
void* dexstuff_defineclass(struct dexstuff_t *d, char *name, int cookie);
void* getSelf(struct dexstuff_t *d);
void dalvik_dump_class(struct dexstuff_t *dex, char *clname);

static void* mydlsym(void *hand, const char *name)
{
	void* ret = dlsym(hand, name);
	return ret;
}


void dexstuff_resolv_dvm(struct dexstuff_t *d)
{
	d->dvm_hand = dlopen("libdvm.so", RTLD_NOW);
	
	if (d->dvm_hand) {
		d->dvm_dalvik_system_DexFile = (DalvikNativeMethod*) mydlsym(d->dvm_hand, "dvm_dalvik_system_DexFile");
		d->dvm_java_lang_Class = (DalvikNativeMethod*) mydlsym(d->dvm_hand, "dvm_java_lang_Class");
		
		d->dvmThreadSelf_fnPtr = mydlsym(d->dvm_hand, "_Z13dvmThreadSelfv");
		if (!d->dvmThreadSelf_fnPtr)
			d->dvmThreadSelf_fnPtr = mydlsym(d->dvm_hand, "dvmThreadSelf");
		
		d->dvmStringFromCStr_fnPtr = mydlsym(d->dvm_hand, "_Z32dvmCreateStringFromCstrAndLengthPKcj");
		d->dvmGetSystemClassLoader_fnPtr = mydlsym(d->dvm_hand, "_Z23dvmGetSystemClassLoaderv");
		d->dvmIsClassInitialized_fnPtr = mydlsym(d->dvm_hand, "_Z21dvmIsClassInitializedPK11ClassObject");
		d->dvmInitClass_fnPtr = mydlsym(d->dvm_hand, "dvmInitClass");
		
		d->dvmFindVirtualMethodHierByDescriptor_fnPtr = mydlsym(d->dvm_hand, "_Z36dvmFindVirtualMethodHierByDescriptorPK11ClassObjectPKcS3_");
		if (!d->dvmFindVirtualMethodHierByDescriptor_fnPtr)
			d->dvmFindVirtualMethodHierByDescriptor_fnPtr = mydlsym(d->dvm_hand, "dvmFindVirtualMethodHierByDescriptor");
			
		d->dvmFindDirectMethodByDescriptor_fnPtr = mydlsym(d->dvm_hand, "_Z31dvmFindDirectMethodByDescriptorPK11ClassObjectPKcS3_");
		if (!d->dvmFindDirectMethodByDescriptor_fnPtr)
			d->dvmFindDirectMethodByDescriptor_fnPtr = mydlsym(d->dvm_hand, "dvmFindDirectMethodByDescriptor");
		
		d->dvmIsStaticMethod_fnPtr = mydlsym(d->dvm_hand, "_Z17dvmIsStaticMethodPK6Method");
		d->dvmAllocObject_fnPtr = mydlsym(d->dvm_hand, "dvmAllocObject");
		d->dvmCallMethodV_fnPtr = mydlsym(d->dvm_hand, "_Z14dvmCallMethodVP6ThreadPK6MethodP6ObjectbP6JValueSt9__va_list");
		d->dvmCallMethodA_fnPtr = mydlsym(d->dvm_hand, "_Z14dvmCallMethodAP6ThreadPK6MethodP6ObjectbP6JValuePK6jvalue");
		d->dvmAddToReferenceTable_fnPtr = mydlsym(d->dvm_hand, "_Z22dvmAddToReferenceTableP14ReferenceTableP6Object");
		
		d->dvmSetNativeFunc_fnPtr = mydlsym(d->dvm_hand, "_Z16dvmSetNativeFuncP6MethodPFvPKjP6JValuePKS_P6ThreadEPKt");
		d->dvmUseJNIBridge_fnPtr = mydlsym(d->dvm_hand, "_Z15dvmUseJNIBridgeP6MethodPv");
		if (!d->dvmUseJNIBridge_fnPtr)
			d->dvmUseJNIBridge_fnPtr = mydlsym(d->dvm_hand, "dvmUseJNIBridge");
		
		d->dvmDecodeIndirectRef_fnPtr =  mydlsym(d->dvm_hand, "_Z20dvmDecodeIndirectRefP6ThreadP8_jobject");
		
		d->dvmLinearSetReadWrite_fnPtr = mydlsym(d->dvm_hand, "_Z21dvmLinearSetReadWriteP6ObjectPv");
		
		d->dvmGetCurrentJNIMethod_fnPtr = mydlsym(d->dvm_hand, "_Z22dvmGetCurrentJNIMethodv");
		
		d->dvmFindInstanceField_fnPtr = mydlsym(d->dvm_hand, "_Z20dvmFindInstanceFieldPK11ClassObjectPKcS3_");
		
		//d->dvmCallJNIMethod_fnPtr = mydlsym(d->dvm_hand, "_Z21dvmCheckCallJNIMethodPKjP6JValuePK6MethodP6Thread");
		d->dvmCallJNIMethod_fnPtr = mydlsym(d->dvm_hand, "_Z16dvmCallJNIMethodPKjP6JValuePK6MethodP6Thread");
		
		d->dvmDumpAllClasses_fnPtr = mydlsym(d->dvm_hand, "_Z17dvmDumpAllClassesi");
		d->dvmDumpClass_fnPtr = mydlsym(d->dvm_hand, "_Z12dvmDumpClassPK11ClassObjecti");
		
		d->dvmFindLoadedClass_fnPtr = mydlsym(d->dvm_hand, "_Z18dvmFindLoadedClassPKc");
		if (!d->dvmFindLoadedClass_fnPtr)
			d->dvmFindLoadedClass_fnPtr = mydlsym(d->dvm_hand, "dvmFindLoadedClass");
		
		d->dvmHashTableLock_fnPtr = mydlsym(d->dvm_hand, "_Z16dvmHashTableLockP9HashTable");
		d->dvmHashTableUnlock_fnPtr = mydlsym(d->dvm_hand, "_Z18dvmHashTableUnlockP9HashTable");
		d->dvmHashForeach_fnPtr = mydlsym(d->dvm_hand, "_Z14dvmHashForeachP9HashTablePFiPvS1_ES1_");
	
		d->dvmInstanceof_fnPtr = mydlsym(d->dvm_hand, "_Z13dvmInstanceofPK11ClassObjectS1_");
		
		d->gDvm = mydlsym(d->dvm_hand, "gDvm");
		printf("libdvm.so load ok.\n");
	}
	else {
		printf("libdvm.so load fail.\n");
	}
}

//-------------- function -----------------
#ifdef _BUILD_MAIN
#include "nisa.pic.h"
int main(int argc, char** argv) {
	printf("%s",nisa);
	struct dexstuff_t xx;
	dexstuff_resolv_dvm(&xx);
	return 0;
}
#endif
