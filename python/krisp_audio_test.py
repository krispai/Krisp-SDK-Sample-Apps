import os
import sys
import time
import krisp_audio
from audio_utils import TestUtils

console_log_char_count = 50

class KrispAudioTest:
    def __init__(self, model_path, audio_stream_info, frame_dur, suppression_level):
        self.model_path = model_path
        self.audio_stream_info = audio_stream_info
        self.frame_dur = frame_dur
        self.nc_instance = None
        self.suppression_level = suppression_level
        krisp_audio.globalInit("")
        version_info = krisp_audio.getVersion()
        print(f"Krisp Audio version: {version_info.major}.{version_info.minor}.{version_info.patch}.{version_info.build}")
        print("=" * console_log_char_count)

    def __enter__(self):
        self.nc_instance = self._create_nc_session()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.nc_instance = None
        krisp_audio.globalDestroy()

    def _int_to_sample_rate(self, sample_rate):
        rates = {
            8000: krisp_audio.SamplingRate.Sr8000Hz,
            16000: krisp_audio.SamplingRate.Sr16000Hz,
            24000: krisp_audio.SamplingRate.Sr24000Hz,
            32000: krisp_audio.SamplingRate.Sr32000Hz,
            44100: krisp_audio.SamplingRate.Sr44100Hz,
            48000: krisp_audio.SamplingRate.Sr48000Hz
        }
        if sample_rate not in rates:
            raise ValueError("Unsupported sample rate")
        return rates[sample_rate]

    def _int_to_frame_dur(self, frame_dur):
        durations = {
            10: krisp_audio.FrameDuration.Fd10ms,
            15: krisp_audio.FrameDuration.Fd15ms,
            20: krisp_audio.FrameDuration.Fd20ms,
            30: krisp_audio.FrameDuration.Fd30ms,
            32: krisp_audio.FrameDuration.Fd32ms
        }
        if frame_dur not in durations:
            raise ValueError("Unsupported frame duration")
        return durations[frame_dur]

    def _create_nc_session(self):
        model_info = krisp_audio.ModelInfo()
        model_info.path = self.model_path

        nc_cfg = krisp_audio.NcSessionConfig()
        nc_cfg.inputSampleRate = self._int_to_sample_rate(self.audio_stream_info['sample_rate'])
        nc_cfg.inputFrameDuration = self._int_to_frame_dur(self.frame_dur)
        nc_cfg.outputSampleRate = nc_cfg.inputSampleRate
        nc_cfg.modelInfo = model_info

        if self.audio_stream_info['sample_type'] == 'FLOAT':
            return krisp_audio.NcFloat.create(nc_cfg)
        elif self.audio_stream_info['sample_type'] == 'PCM_16':
            return krisp_audio.NcInt16.create(nc_cfg)
        else:
            raise ValueError(f"Unsupported sample type {self.audio_stream_info['sample_type']}")

    def process_stream(self):
        input_audio_stream = self.audio_stream_info['audio_stream']
        total_frames = len(input_audio_stream)
        processed_audio_stream = []

        for i, frame in enumerate(input_audio_stream):
            processed_frame = self.nc_instance.process(frame, self.suppression_level)
            processed_audio_stream.append(processed_frame)                               

            progress = (i + 1) / total_frames * 100
            sys.stdout.write(f"\rProcessing frame {i + 1}/{total_frames} ({progress:.2f}%)")
            sys.stdout.flush()            

        sys.stdout.write("\n")
        return processed_audio_stream

def main():
    print("=" * console_log_char_count)
    print("Krisp Python SDK Sample App")
    print("=" * console_log_char_count)

    # Parse command line arguments and read input audio into buffer
    args = TestUtils.parse_arguments()
    audio_stream_info = TestUtils.wav_to_audio_stream(args.input_audio_path, args.frame_dur)
    print('Input Audio info')
    print(f'Sample rate: {audio_stream_info["sample_rate"]}')
    print(f'Sample type: {audio_stream_info["sample_type"]}')
    print("=" * console_log_char_count)

    # Initialize Krisp Audio SDK and process audio stream
    with KrispAudioTest(args.kef_path, audio_stream_info, args.frame_dur, args.suppression_level) as krisp:
        processed_audio_stream = krisp.process_stream()

    # Write processed audio to a new wav file
    file_name_without_extension = os.path.splitext(os.path.basename(args.input_audio_path))[0]
    output_audio_path = f"{file_name_without_extension}_nvc_{args.frame_dur}ms_{audio_stream_info['sample_rate']}hz.wav"
    TestUtils.audio_stream_to_wav(output_audio_path, processed_audio_stream, 
                                audio_stream_info['sample_rate'], audio_stream_info['sample_type'])
    print(f'Processed audio written to {output_audio_path}')
    print("=" * console_log_char_count)
    print("Exiting Application")
    print("=" * console_log_char_count)

if __name__ == "__main__":
    main()
