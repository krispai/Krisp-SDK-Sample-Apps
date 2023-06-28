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
* Krisp archive libraries for the noise canceling
* Intel MKL libraries installed only on the x86/amd64 based system - bundled with Krisp Audio SDK starting from v7.0
* Libresample - bundled with Krisp Audio SDK starting from v7.0

The Accelerator library is used instead of Intel MKL on Apple ARM-based platforms which is available in the OS out of the box.

The following environment variables are mandatory. The name of each parameter is self-explanatory.
* LIBSNDFILE_INC
* LIBSNDFILE_LIB
* KRISP_INC
* KRISP_LIB

The MKL_LIB parameter is mandatory on the x86/amd64-based system. The parameter should point to the MKL libraries folder which is packaged with Krisp Audio SDK for x86/amd64-based systems starting from v7.0.

KRISP_INC and KRISP_LIB should point to the Krisp SDK header directory and library directory.

The LIBRESAMPLE_LIB parameter is mandatory and required by Krisp Audio SDK. The parameter should point to the folder containing libresample library which is bundled with Krisp Audio SDK starting from v7.0.

LIBSNDFILE_INC and LIBSNDFILE_LIB directories is required by the sample-nc app for the purpose of reading and writing WAV PCM-based audio files. It is not the SDK requirement.


## Build Process

How to run the build

### On Mac/Linux run
```make```

### On Windows run
```make vs```

This will produce a Visual Studio Solution project in the **vsbuild** folder.

## Build Output
All apps will be stored inside the **bin** folder in the root directory

# Apps
## sample-nc
The noise cancelling app that applies Krisp NC technology on the given PCM16 wav file using given Krisp Weight file model. The app with its codebase demonstrates 
* how to initialize Krisp SDK and how to free memory resources if you don't need to use Krisp anymore 
* how to load a single model
* how to define the size of the frame to prepare the SDK for the processing of the frame sequence
* how to process the sound frame-by-frame using Krisp
* how to process either PCM16 or FLOAT based audio file
* how to use either regular Krisp NC API or NC API with Call Stats
* how to to get the Call Stats for the whole processed file and for the each frame, the feature is enabled with '-s' option

### Usage
```sample-nc -i <PCM16 wav file> -o <output WAV file path> -w <path to AI model weight file> -s```

### Test input for the sample-nc app
[test/input/sample-nc-test.wav](test/input/sample-nc-test.wav)
