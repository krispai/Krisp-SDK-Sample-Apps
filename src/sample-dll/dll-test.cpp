#include <iostream>
#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-nc.hpp>
#include <krisp-audio-sdk-nc-stats.hpp>


int dllFunction();


int main() {
	std::cout << "Hello World" << std::endl;
	krispAudioGlobalInit(nullptr);
	krispAudioGlobalDestroy();
	dllFunction();
	return 0;
}
