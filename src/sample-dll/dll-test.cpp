#include <krisp-audio-sdk.hpp>
#include <krisp-audio-sdk-nc.hpp>
#include <krisp-audio-sdk-nc-stats.hpp>


int main() {
	krispAudioGlobalInit(nullptr);
	krispAudioGlobalDestroy();
	return 0;
}
