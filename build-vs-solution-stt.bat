rmdir /S /Q vs-solution
mkdir vs-solution
cmake -B vs-solution -S cmake ^
 -D KRISP_SDK_PATH=%KRISP_SDK_PATH% ^
 -D LIBSNDFILE_INC=%LIBSNDFILE_INC% ^
 -D LIBSNDFILE_LIB=%LIBSNDFILE_LIB% ^
 -D STT=1