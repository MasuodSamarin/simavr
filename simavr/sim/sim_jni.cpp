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

JNIEXPORT void JNICALL
Java_org_starlo_boardmicro_NativeInterface_loadPartialProgram(JNIEnv *, jobject, jstring)
{
	//loadPartialProgram("");
}

JNIEXPORT void JNICALL
Java_org_starlo_boardmicro_NativeInterface_engineInit(JNIEnv *, jobject, jstring)
{
	engineInit("");
}

JNIEXPORT jint JNICALL
Java_org_starlo_boardmicro_NativeInterface_fetchN(JNIEnv *, jobject, jint n)
{
	return fetchN(n);
}

}
