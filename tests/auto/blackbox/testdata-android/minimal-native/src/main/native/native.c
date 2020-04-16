#include <string.h>
#include <jni.h>

jstring
Java_minimalnative_MinimalNative_stringFromNative(JNIEnv* env, jobject thiz)
{
    (void)thiz;
    return (*env)->NewStringUTF(env, "This message comes from native code.");
}
