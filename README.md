![Krisp Logo](Krisp.png)

# Krisp Sample Apps

## Build Dependencies
The reference samples require **libsndfile** library to read and write WAV files. It also needs KRISP archive libraries should also be provided during the build to enable noise cancellation.cancel the noise.

The following environement variables are mandatory. The name of each parameter is self- explanatory.
* LIBSNDFILE_INC
* LIBSNDFILE_LIB
* KRISP_INC
* KRISP_LIB

The one optional parameter is MKL_LIB that points to the directory that contains Intel MKL libraries.  If the paraemeter is not set the CMake will search for the library in the default location used by Intel MKL installer. Please consider that Intel MKL libraries are required only on x86/amd64 based systems. Accelerator library is required on Apple ARM based platforms instead.

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
* how to initialise Krisp SDK and how to free memory resources if you don't need to use Krisp anymore 
* how to load a single model
* how to define the size of the frame to prepare the SDK for the processing of the frame sequence
* how to process the sound frame by frame using Krisp

### Usage
```sample-nc -i <PCM16 wav file> -o <output WAV file path> -w <path to AI model weight file>```

### Test input for the app
```test/input/sample-nc-test.wav```

