#include <jni.h>
#include <stdint.h>

extern "C"
{

void loadPartialProgram(uint8_t* binary);
void engineInit(const char* m);
int32_t fetchN(int32_t n);
void refreshUI(JNIEnv* env, jobject obj);

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv* env;
	if(vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
	{
		return -1;
	}

	return JNI_VERSION_1_6;
}

uint8_t bState = 0x0;
uint8_t cState = 0x0;
uint8_t dState = 0x0;
uint8_t eState = 0x0;
uint8_t fState = 0x0;
jmethodID writePort = NULL;
jmethodID writeSPI = NULL;

JNIEXPORT void JNICALL
Java_org_starlo_boardmicro_NativeInterface_loadPartialProgram(JNIEnv* env, jobject, jstring hex)
{
	loadPartialProgram((uint8_t*)env->GetStringUTFChars(hex, NULL));
}

JNIEnv* gEnv = NULL;
jobject gObj;
JNIEXPORT void JNICALL
Java_org_starlo_boardmicro_NativeInterface_engineInit(JNIEnv* env, jobject obj, jstring target)
{
	gEnv = env;
	gObj = obj;
	writePort = env->GetMethodID(env->GetObjectClass(obj), "writePort", "(IB)V");
	writeSPI = env->GetMethodID(env->GetObjectClass(obj), "writeSPI", "(I)V");
	engineInit(env->GetStringUTFChars(target, NULL));
}

JNIEXPORT jint JNICALL
Java_org_starlo_boardmicro_NativeInterface_fetchN(JNIEnv* env, jobject obj, jint n)
{
	int32_t state = fetchN(n);
	refreshUI(env, obj);
	return state;
}

void refreshUI(JNIEnv* env, jobject obj)
{
	env->CallVoidMethod(obj, writePort, 0, bState);
	env->CallVoidMethod(obj, writePort, 1, cState);
	env->CallVoidMethod(obj, writePort, 2, dState);
	env->CallVoidMethod(obj, writePort, 3, eState);
	env->CallVoidMethod(obj, writePort, 4, fState);
}

void jniWriteSPI(uint8_t value)
{
	refreshUI(gEnv, gObj);
	gEnv->CallVoidMethod(gObj, writeSPI, value);
}

}
