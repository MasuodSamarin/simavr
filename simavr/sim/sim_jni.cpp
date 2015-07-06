#include <jni.h>
#include <stdint.h>
#include <string>
#include <string.h>
#include <deque>

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

#define NUMBER_OF_PORTS 5

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

JNIEXPORT void JNICALL
Java_org_starlo_boardmicro_NativeInterface_engineInit(JNIEnv* env, jobject obj, jstring target)
{
	writePort = env->GetMethodID(env->GetObjectClass(obj), "writePort", "(IB)V");
	writeSPI = env->GetMethodID(env->GetObjectClass(obj), "writeSPI", "(Ljava/lang/String;)V");
	engineInit(env->GetStringUTFChars(target, NULL));
}

JNIEXPORT jint JNICALL
Java_org_starlo_boardmicro_NativeInterface_fetchN(JNIEnv* env, jobject obj, jint n)
{
	int32_t state = fetchN(n);
	refreshUI(env, obj);
	return state;
}

struct spiWrite
{
	uint8_t ports[5];
	uint8_t spi;
};

std::deque<spiWrite> spiDeque;
void refreshUI(JNIEnv* env, jobject obj)
{
/*
[
{
  "ports": {
    "bstate": "0",
    "cstate": "0",
    "dstate": "0",
    "estate": "0",
    "fstate": "0"
  },
  "spi":"0"
}
]
*/
	char buffer[32];
	memset(buffer, '\0', 32);
	std::string spiString;
	const char* portNames[NUMBER_OF_PORTS] = {"bState", "cState", "dState", "eState", "fState"};
	spiString.append("[\n");
	while(spiDeque.size() > 0)
	{
		spiString.append("{\n\"ports\":{\n");
		spiWrite call = spiDeque.front();
		int i = NUMBER_OF_PORTS-1;
		while(i--)
		{
			sprintf(buffer, "\"%s\": \"%i\",\n", portNames[i], call.ports[i]);
			spiString.append(buffer);
			memset(buffer, '\0', 32);
		}
		sprintf(buffer, "\"%s\": \"%i\"\n", portNames[NUMBER_OF_PORTS-1], call.ports[NUMBER_OF_PORTS-1]);
		spiString.append(buffer);
		memset(buffer, '\0', 32);
		spiString.append("},\n");
		sprintf(buffer, "\"spi\": \"%i\"\n", call.spi);
		spiString.append(buffer);
		if(spiDeque.size() != 1)
		{
			spiString.append("},\n");
		}
		else
		{
			spiString.append("}\n");
		}
		spiDeque.pop_front();
	}
	spiString.append("]\n");
	jstring spi = env->NewStringUTF(spiString.c_str());
	env->CallVoidMethod(obj, writeSPI, spi);
	env->DeleteLocalRef(spi);

	env->CallVoidMethod(obj, writePort, 0, bState);
	env->CallVoidMethod(obj, writePort, 1, cState);
	env->CallVoidMethod(obj, writePort, 2, dState);
	env->CallVoidMethod(obj, writePort, 3, eState);
	env->CallVoidMethod(obj, writePort, 4, fState);
}

void jniWriteSPI(uint8_t value)
{
	spiWrite call;
	call.ports[0] = bState;
	call.ports[1] = cState;
	call.ports[2] = dState;
	call.ports[3] = eState;
	call.ports[4] = fState;
	call.spi = value;
	spiDeque.push_back(call);
}

}
