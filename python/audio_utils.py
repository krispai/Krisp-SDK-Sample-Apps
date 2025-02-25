import argparse
import soundfile as sf

class TestUtils:
    @staticmethod
    def parse_arguments():
        parser = argparse.ArgumentParser(description="Sample usage of krisp_audio")
        parser.add_argument('-cfg', '--kef_path', type=str, required=True, help='Path to the .kef file')
        parser.add_argument('-i', '--input_audio_path', type=str, required=True, help='Path to the input .wav audio file')
        parser.add_argument('-d', '--frame_dur', type=int, default=10, help='Optional: Frame duration. Supported: 10, 15, 20, 30, 32. Default: 10')
        parser.add_argument('-nsl', '--suppression_level', type=int, default=100, help='Optional: Noise suppression level in the range [0, 100], used to controll the noise canceling aggressiveness. Default: 100, i.e. full noise canceling')
        return parser.parse_args()

    @staticmethod
    def wav_to_audio_stream(wav_path, frame_dur):
        with sf.SoundFile(wav_path) as inputFile:
            sample_rate = inputFile.samplerate
            sample_type = inputFile.subtype
            if sample_type == 'PCM_16':
                data_type = 'int16'
            elif sample_type == 'FLOAT':
                data_type = 'float32'
            else:
                raise ValueError(f"Unsupported WAV data type: {sample_type}")

            audio_data = inputFile.read(dtype=data_type)
            if inputFile.channels > 1:
                raise ValueError(f"Supports only Mono audio, provided audio channels: {inputFile.channels}")

            frame_size = int(sample_rate * frame_dur / 1000)
            audio_stream = [audio_data[i:i + frame_size] for i in range(0, len(audio_data) - frame_size + 1, frame_size)]
            return {
                'sample_rate': sample_rate,
                'sample_type': sample_type,
                'audio_stream': audio_stream
            }

    @staticmethod
    def audio_stream_to_wav(wav_path, audio_stream, sample_rate, sample_type):
        concat_stream = [sample for frame in audio_stream for sample in frame]
        with sf.SoundFile(wav_path, mode='w', samplerate=sample_rate, channels=1, subtype=sample_type) as wav_file:
            wav_file.write(concat_stream) 