#include <jni.h>
#include <stdint.h>

extern "C"
{

void loadPartialProgram(uint8_t* binary);
void engineInit(const char* m);
int32_t fetchN(int32_t n);

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv* env;
	if(vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
	{
		return -1;
	}

	return JNI_VERSION_1_6;
}

jmethodID writePort = NULL;

JNIEXPORT void JNICALL
Java_org_starlo_boardmicro_NativeInterface_loadPartialProgram(JNIEnv* env, jobject, jstring hex)
{
	loadPartialProgram((uint8_t*)env->GetStringUTFChars(hex, NULL));
}

JNIEXPORT void JNICALL
Java_org_starlo_boardmicro_NativeInterface_engineInit(JNIEnv* env, jobject obj, jstring target)
{
	writePort = env->GetMethodID(env->GetObjectClass(obj), "writePort", "(IB)V");
	engineInit(env->GetStringUTFChars(target, NULL));
}

JNIEXPORT jint JNICALL
Java_org_starlo_boardmicro_NativeInterface_fetchN(JNIEnv* env, jobject obj, jint n)
{
	env->CallVoidMethod(obj, writePort, 0, 0xFF);
	return fetchN(n);
}

}
