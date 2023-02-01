Build Instructions.

The reference samples require libsndfile library to read and write WAV files.
It also needs KRISP archive libraries should also be provided during the build to enable noise cancellation.cancel the noise.

The list of optional and mandatory environement variables.

The following environement variables are mandatory. The name of each parameter is self- explanatory.
LIBSNDFILE_INC
LIBSNDFILE_LIB
KRISP_INC
KRISP_LIB

The one optional parameter is MKL_LIB that points to the directory that contains Intel MKL libraries.  If the paraemeter is not set the CMake will search for the library in the default location used by Intel MKL installer. Please consider that Intel MKL libraries are required only on x86/amd64 based systems. Accelerator library is required on Apple ARM based platforms instead.

How to run the build

On Mac/Linux run
make

On Windows run
make vs
This will produce a Visual Studio Solution project in the vsbuild folder.
