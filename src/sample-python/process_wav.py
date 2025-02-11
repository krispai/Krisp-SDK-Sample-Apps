#!/usr/bin/env python3

import argparse
import soundfile as sf
import numpy as np
import krisp_module


class AudioProcessorWrapper:
    def __init__(self, sample_rate, sample_type, model_path):
        krisp_module.loadKrispModel(model_path, "aliasForModel")
        if sample_type == 'FLOAT':
            self.__data_type = np.float32
            self.processor = krisp_module.KrispAudioProcessorPcmFloat(sample_rate, "aliasForModel")
        elif sample_type == 'PCM_16':
            self.__data_type = np.int16
            self.processor = krisp_module.KrispAudioProcessorPcm16(sample_rate, "aliasForModel")
        else:
            assert(0)

        self.sample_rate = sample_rate
        self.__channels = 1

    def store_audio_chunk(self, audio_chunk):
        self.processor.store_audio_chunk(audio_chunk)

    def get_processed_frames(self):
        frame_size_ms = 10
        samples_per_frame = (self.sample_rate * frame_size_ms) // 1000
        samples_count = self.processor.get_samples_count()
        frame_count = samples_count // (samples_per_frame * self.__channels)
        output_shape = (frame_count, samples_per_frame, self.__channels)
        output_frames = np.zeros(output_shape, dtype=self.__data_type)
        num_of_frames = self.processor.get_processed_frames(output_frames)
        return output_frames[:(num_of_frames * samples_per_frame * self.__channels)]


def simulate_audio_stream(file_path, output_file_path, chunk_size_ms, model_path):
    with sf.SoundFile(file_path) as inputFile:
        sample_rate = inputFile.samplerate
        sample_type = inputFile.subtype
        if sample_type == 'PCM_16':
            data_type = 'int16'
        elif sample_type == 'FLOAT':
            data_type = 'float32'
        else:
            raise ValueError(f"Unsporrted WAV data type: {sample_type}")
        audio_data = inputFile.read(dtype=data_type)
        subtype = inputFile.subtype
        channels = inputFile.channels
        if channels != 1:
            raise Exception("Only MONO WAV files are supported")
        channels = audio_data.shape[1] if audio_data.ndim > 1 else 1
        ap_wrapper = AudioProcessorWrapper(sample_rate, sample_type, model_path)
        num_samples = audio_data.shape[0]
        chunk_size_samples = (chunk_size_ms * sample_rate) // 1000
        all_frames = []
        for start in range(0, num_samples, chunk_size_samples):
            end = min(start + chunk_size_samples, num_samples)
            audio_chunk = audio_data[start:end]
            ap_wrapper.store_audio_chunk(audio_chunk)
            frames = ap_wrapper.get_processed_frames()
            all_frames.extend(frames)
        if all_frames:
            all_frames = np.concatenate(all_frames, axis=0)
        with sf.SoundFile(output_file_path, 'w', samplerate=sample_rate, channels=channels, subtype=sample_type) as myfile:
            myfile.write(all_frames)

def get_command_line_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(description="Process FLOAT WAV files with Krisp Audio SDK.")
    parser.add_argument("-i", "--input", type=str, required=True, help="Input WAV file path.")
    parser.add_argument("-o", "--output", type=str, required=True, help="Output WAV file path.")
    parser.add_argument("-m", "--model", type=str, required=True, help="Path to the AI model.")
    return parser.parse_args()

def _entry():
    args = get_command_line_arguments()
    simulate_audio_stream(args.input, args.output, 20, args.model)

if __name__ == "__main__":
    _entry()
