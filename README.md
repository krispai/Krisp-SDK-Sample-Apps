<div align="right">
<img src="./Krisp.png" height="75px" />
</div>

# Krisp Sample Apps
## Overview
The repository provides sample apps for all desktop platforms demonstrating Krisp SDK functionality.
The build system and the codebase is compatible with.
* Mac M1
* Mac Intel
* Windows(amd64)
* Linux(amd64)

## Build Dependencies
The reference samples require
* **libsndfile** library to read and write WAV files 
* Krisp archive libraries for noise cancelling
* Intel MKL libraries installed only on the x86/amd64 based system.

The Accelerator library is used instead of Intel MKL on Apple ARM based platforms which is available in the OS out of the box.

The following environment variables are mandatory. The name of each parameter is self-explanatory.
* LIBSNDFILE_INC
* LIBSNDFILE_LIB
* KRISP_INC
* KRISP_LIB

The optional MKL_LIB parameter is only considered on the x86/amd64 based system. If the parameter is set, then CMake will look for the required Intel MKL libraries in the specified folder. If the parameter is not set then CMake will search for the library in the default location used by the Intel MKL installer. 

## Build Process

How to run the build

### On Mac/Linux run
```make```

### On Windows run
```make vs```

This will produce a Visual Studio Solution project in the **vsbuild** folder.

## Build Output
All apps will be stored inside the **bin** folder in the root directory

## Apps
### sample-nc  
The noise cancelling app that applies Krisp NC technology on the given PCM16 wav file using given Krisp Weight file model. The app with its codebase demonstrates 
* how to initialize Krisp SDK and how to free memory resources if you don't need to use Krisp anymore 
* how to load a single model
* how to define the size of the frame to prepare the SDK for the processing of the frame sequence
* how to process the sound frame-by-frame using Krisp

### Usage
```sample-nc -i <PCM16 wav file> -o <output WAV file path> -w <path to AI model weight file>```

### Test input for the sample-nc app
[test/input/sample-nc-test.wav](test/input/sample-nc-test.wav)
