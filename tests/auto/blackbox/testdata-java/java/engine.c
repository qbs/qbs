#include <jni.h>

// javac 1.8 is required to generate native headers
#ifdef JNI_VERSION_1_8
#include "Car_InternalCombustionEngine.h"
#endif

JNIEXPORT void JNICALL Java_Car_00024InternalCombustionEngine_run(JNIEnv *env, jobject obj) {
    printf("Native code performing complex internal combustion process (%p, %p)!\n", env, obj);
}
