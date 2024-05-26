#!/usr/bin/env python3

import soundfile as sf
import numpy as np
import audio_processor


class AudioProcessorWrapper:
    def __init__(self, sample_rate, channels):
        self.processor = audio_processor.AudioProcessor(sample_rate, channels)
        self.sample_rate = sample_rate
        self.__channels = channels

    def store_audio_chunk(self, audio_chunk):
        self.processor.store_audio_chunk(audio_chunk)

    def get_processed_frames(self):
        frame_size_ms = 10
        samples_per_frame = (self.sample_rate * frame_size_ms) // 1000
        samples_count = self.processor.get_samples_count()
        frame_count = samples_count // (samples_per_frame * self.__channels)
        output_shape = (frame_count, samples_per_frame, self.__channels)
        output_frames = np.zeros(output_shape, dtype=np.float32)
        num_of_frames = self.processor.get_processed_frames(output_frames)
        return output_frames[:(num_of_frames * samples_per_frame * self.__channels)]


def simulate_audio_stream(file_path, chunk_size_ms):
    with sf.SoundFile(file_path) as inputFile:
        audio_data = inputFile.read()
        sample_rate = inputFile.samplerate
        subtype = inputFile.subtype
        channels = inputFile.channels
        channels = audio_data.shape[1] if audio_data.ndim > 1 else 1
        ap_wrapper = AudioProcessorWrapper(sample_rate, channels)
        num_samples = audio_data.shape[0]
        chunk_size_samples = (chunk_size_ms * sample_rate) // 1000
        all_frames = []
        for start in range(0, num_samples, chunk_size_samples):
            end = min(start + chunk_size_samples, num_samples)
            audio_chunk = audio_data[start:end]
            ap_wrapper.store_audio_chunk(audio_chunk)
            frames = ap_wrapper.get_processed_frames()
            print(len(frames))
            assert(0)
            all_frames.extend(frames)
        if all_frames:
            all_frames = np.concatenate(all_frames, axis=0)
        with sf.SoundFile('new_file.wav', 'w', samplerate=sample_rate, channels=channels, subtype="FLOAT") as myfile:
            myfile.write(all_frames)

def _entry():
    print("HI")
    input_wav = "/Users/atatalyan/dev/gitrepos/Krisp-SDK-Sample-Apps/test/nc32f-32k.wav"
    simulate_audio_stream(input_wav, 20)

if __name__ == "__main__":
    _entry()
