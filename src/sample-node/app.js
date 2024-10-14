const fs = require('fs');
const addon = require('./nodejs-module.node');
const WaveFile = require('wavefile').WaveFile;

const { Command } = require('commander');
const program = new Command();

program
    .name('node-js-krisp')
    .description('Krisp NodeJS audio processing sample')
    .requiredOption('-o --output-wav <path>', 'Path to the processed WAV file')
    .requiredOption('-i --input-wav <path>', 'Path to the input WAV file')
    .requiredOption('-m --model <model>', 'Path to the model file');

program.parse(process.argv)

const options = program.opts();

const inputWavPath = options.inputWav;
const outputWavPath = options.outputWav;
const modelPath = options.model;

console.log('Input WAV:', inputWavPath);
console.log('Output WAV:', outputWavPath);
console.log('Model:', modelPath);


function readFileSync(filePath) {
    try {
        const buffer = fs.readFileSync(filePath);
        return buffer;
    }
    catch (err) {
        if (err.code === 'ENOENT') {
            throw new Error(`Error: file at "${filePath}" does not exist.`)
        }
        else if (err.code === 'EACCES') {
            throw new Error(`Error: can't access the "${filePath}".`);
        }
        else {
            throw new Error(`Error: reading "${filePath}" file.`);
        }
    }
}

function readWavFileSync(filePath) {
    const buffer = readFileSync(filePath);
    let wav;
    try {
        wav = new WaveFile(buffer);
    }
    catch (err) {
        throw new Error(`${err}\nError decoding ${filePath} WAV file`);
    }
    if (wav.fmt.numChannels !== 1) {
        throw new Error(`Unsupported number of channels: ${wav.fmt.numChannels}. Only mono is supported.`);
    }
    const sampleRate = wav.fmt.sampleRate;
    const WAV_PCM_TYPE = 1;
    const WAV_FLOAT_TYPE = 3;
    if (wav.fmt.audioFormat == WAV_FLOAT_TYPE && wav.fmt.bitsPerSample == 32) {
        const sampleSize = 4;
        const audioData = wav.data.samples;
        return [audioData, sampleRate, sampleSize, "FLOAT32"];
    }
    else if (wav.fmt.audioFormat == WAV_PCM_TYPE && wav.fmt.bitsPerSample == 16) {
        const sampleSize = 2;
        const audioData = wav.data.samples;
        return [audioData, sampleRate, sampleSize, "PCM16"];
    }
    else {
        throw new Error(`Unsupported WAV format: ${wav.fmt.audioFormat} and depth: ${wav.fmt.bitsPerSample}\nonly PCM16 and FLOAT32 are supported`);
    }
}

function getFrameSize(sampleRate) {
    const FRAME_DURATION_MS = 20;
    return Math.floor((FRAME_DURATION_MS / 1000) * sampleRate);
}

function processAudio(inputWavPath, outputWavPath) {
    const [audioData, sampleRate, sampleSize, sampleType] = readWavFileSync(inputWavPath);
    const frameSizeInSamples = getFrameSize(sampleRate);
    const frameSizeInBytes = frameSizeInSamples * sampleSize;
    const numberOfFrames = audioData.length / frameSizeInBytes;
    const processedAudio = Buffer.alloc(numberOfFrames * frameSizeInBytes);

    const KrispAudioProcessor = addon.KrispAudioProcessor;
    const audioProcessor = new KrispAudioProcessor();

    audioProcessor.loadModel(modelPath);
    audioProcessor.setSampleRate(sampleRate);

    let krispProcessFrame;
    if (sampleType === "PCM16") {
        krispProcessFrame = audioProcessor.processFramesPcm16.bind(audioProcessor);
    }
    else if (sampleType === "FLOAT32") {
        krispProcessFrame = audioProcessor.processFramesFloat32.bind(audioProcessor);
    }
    else {
        throw new Error("this will not happen");
    }

    for (let i = 0; i < numberOfFrames; i++) {
        const start = i * frameSizeInBytes;
        const end = start + frameSizeInBytes;
        const frame = audioData.subarray(start, end);
        const processedFrame = processedAudio.slice(start, end);
        krispProcessFrame(frame, processedFrame);
    }
    const processedWav = new WaveFile();
    if (sampleType === "PCM16") {
        const typedSamples = new Int16Array(processedAudio.buffer, processedAudio.byteOffset, processedAudio.length / 2);
        processedWav.fromScratch(1, sampleRate, '16', typedSamples);
    } else if (sampleType === "FLOAT32") {
        const typedSamples = new Float32Array(processedAudio.buffer, processedAudio.byteOffset, processedAudio.length / 4);
        processedWav.fromScratch(1, sampleRate, '32f', typedSamples);
    } else {
        throw new Error("Unsupported sample type");
    }
    fs.writeFileSync(outputWavPath, processedWav.toBuffer());
}

try {
    processAudio(inputWavPath, outputWavPath);
}
catch (err) {
    console.log(err);
    console.log(`${err}`);
    process.exit(1);
}
