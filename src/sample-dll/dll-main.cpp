#include <iostream>

#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-nc.hpp>
#include <krisp-audio-sdk-nc-stats.hpp>


int dllFunction() {
	std::cout << "Hello World" << std::endl;
	krispAudioGlobalInit(nullptr);
	krispAudioSetModel(nullptr, nullptr);
	krispAudioNcCreateSessionInt16(KrispAudioSamplingRate(0),
                          KrispAudioSamplingRate(0),
                          KrispAudioFrameDuration(10),
						  nullptr);
	krispAudioNcCleanAmbientNoiseInt16(nullptr, nullptr, 0, nullptr, 0);
	krispAudioNcCloseSession(nullptr);
	krispAudioRemoveModel(nullptr);
	krispAudioGlobalDestroy();
	return 0;
}
