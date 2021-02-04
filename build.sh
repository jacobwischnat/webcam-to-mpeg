emcc \
-I/usr/local/Cellar/ffmpeg/4.3.1_9/include \
src/ffmpeg.c \
libavcodec.a \
libavutil.a \
libswresample.a \
-o wasm/dist/ffmpeg.js