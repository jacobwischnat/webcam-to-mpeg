emcc \
-s INITIAL_MEMORY=268435456 \
-s TOTAL_MEMORY=512mb \
-s EXPORT_ES6=1 \
-s ALLOW_MEMORY_GROWTH=1 \
-s DECLARE_ASM_MODULE_EXPORTS=1 \
-s "EXPORTED_FUNCTIONS=['_init_context', '_add_frame', '_retrieve_video', '_retrieve_video_size', '_finalise']" \
-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
-I "/Users/jacob/Code/FFmpeg/" \
"/Users/jacob/Code/FFmpeg/libavcodec/libavcodec.a" \
"/Users/jacob/Code/FFmpeg/libavformat/libavformat.a" \
"/Users/jacob/Code/FFmpeg/libavutil/libavutil.a" \
"/Users/jacob/Code/FFmpeg/libswresample/libswresample.a" \
"/Users/jacob/Code/FFmpeg/libswscale/libswscale.a" \
src/ffmpeg.c \
-o wasm/dist/ffmpeg.js