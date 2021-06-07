#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavcodec/packet.h>
#include <libavcodec/codec.h>
#include <libavutil/dict.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    AVCodec* codec;
    AVPacket* packet;
    AVFrame* inputFrame;
    AVStream* videoStream;
    AVIOContext* avioContext;
    AVCodecContext* codecCtx;
    AVOutputFormat* outputFormat;
    AVFormatContext* formatContext;
    struct SwsContext* swsCtx;
    int size;
    unsigned long long bytesWritten;
    unsigned long long outputSize;
    uint8_t* outputBuffer;
    unsigned long frameCounter;
} ffmpeg_context;

ffmpeg_context ctx;

void hexdump(void *ptr, int buflen) {
  unsigned char *buf = (unsigned char*)ptr;
  int i, j;
  for (i=0; i<buflen; i+=16) {
    printf("%06x: ", i);
    for (j=0; j<16; j++)
      if (i+j < buflen)
        printf("%02x ", buf[i+j]);
      else
        printf("   ");
    printf(" ");
    for (j=0; j<16; j++)
      if (i+j < buflen)
        printf("%c", isprint(buf[i+j]) ? buf[i+j] : '.');
    printf("\n");
  }
}

void encode(int skip);

void init_context(unsigned int width, unsigned int height);
void add_frame(uint8_t* rgbaData);
uint8_t* retrieve_video();
unsigned long retrieve_video_size();

int read_packet(void *opaque, uint8_t *buffer, int bufferSize) {
    printf("read_packet\n");
    return bufferSize;
}

int64_t seek (void *opaque, int64_t offset, int whence) {
    printf("seek: %lu\n", offset);
    ffmpeg_context* ctx = (ffmpeg_context*) opaque;
    switch(whence){
        case SEEK_SET:
            return ctx->outputBuffer + offset;
            break;
        case SEEK_CUR:
            ctx->outputBuffer += offset;
            break;
        case SEEK_END:
            // bd->ptr = (bd->buf + bd->size) + offset;
            ctx->outputBuffer = (ctx->outputBuffer + ctx->outputSize) + offset;
            return ctx->outputBuffer;
            break;
        case AVSEEK_SIZE:
            return ctx->outputSize;
            break;
        default:
           return -1;
    }
    return 1;
}

int write_packet(void *opaque, uint8_t *buffer, int bufferSize) {
    printf("bufferSize: %d\n", bufferSize);
    // printf("ctx.outputBuffer: %p\n", ctx.outputBuffer);
    unsigned long index = ctx.bytesWritten;
    ctx.bytesWritten += bufferSize;
    // printf("ctx->bytesWritten: %d\n", ctx.bytesWritten);
    // printf("pointer before realloc: %p\n", ctx.outputBuffer);
    // ctx.outputBuffer = av_realloc(ctx.outputBuffer, ctx.bytesWritten);
    // printf("pointer after realloc: %p\n", ctx.outputBuffer);
    // printf("---->\n");
    // ctx.outputSize = ctx.bytesWritten;
    // printf("---->> %lu %p\n", index, &ctx.outputBuffer[index]);
    memcpy(&ctx.outputBuffer[index], buffer, bufferSize);
    // printf('~~~~~~>\n');

    return bufferSize;
}

void init_context(unsigned int width, unsigned int height) {
    // ctx = (ffmpeg_context*) calloc(1, sizeof(ffmpeg_context));

    // printf("ctx = %p sizeof pointer %lu\n", ctx, sizeof(void*));

    av_register_all();

    ctx.outputFormat = av_guess_format("webm", 0, 0);

    ctx.frameCounter = 0;
    ctx.formatContext = avformat_alloc_context();
    ctx.bytesWritten = 0;
    ctx.outputSize = 704643072;
    ctx.outputBuffer = (uint8_t*) av_malloc(ctx.outputSize);

    ctx.avioContext = avio_alloc_context(
        ctx.outputBuffer,
        ctx.outputSize,
        1,
        &ctx,
        NULL,
        &write_packet,
        &seek
    );

    ctx.formatContext->pb = ctx.avioContext;
    ctx.formatContext->flags = AVFMT_FLAG_CUSTOM_IO;

    int response = avformat_alloc_output_context2(&ctx.formatContext, ctx.outputFormat, NULL, NULL);
    printf("avformat_alloc_output_context2 response=%d %s\n", response, av_err2str(response));

    AVDictionary* opt = NULL;
    av_dict_set(&opt, "preset", "slow", 0);
    av_dict_set(&opt, "crf", "5", 0);
    av_dict_set(&opt, "pass", "1", 0);


    ctx.codec = avcodec_find_encoder(AV_CODEC_ID_VP8);
    // ctx.codec = avcodec_find_encoder_by_name("libx264");
    printf("ctx.codec: %p\n", ctx.codec);
    ctx.codecCtx = avcodec_alloc_context3(ctx.codec);
    ctx.codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx.codecCtx->width = width;
    ctx.codecCtx->height = height;
    printf("w %d x h %d\n", ctx.codecCtx->width, ctx.codecCtx->height);

    ctx.codecCtx->time_base = (AVRational){1, 25};
    ctx.codecCtx->framerate = (AVRational){25, 1};
    ctx.codecCtx->gop_size = 10;
    ctx.codecCtx->max_b_frames = 1;

    if (avcodec_open2(ctx.codecCtx, ctx.codec, &opt) < 0) {
        printf("Error when opening codec\n");

        // return 1;
    }

    ctx.videoStream = avformat_new_stream(ctx.formatContext, NULL);

    ctx.formatContext->pb = ctx.avioContext;
    ctx.formatContext->flags |= AVFMT_FLAG_CUSTOM_IO;

    ctx.videoStream->codec->codec_tag = 0;
    ctx.videoStream->time_base = ctx.codecCtx->time_base;
    ctx.videoStream->id = ctx.formatContext->nb_streams - 1;


    response = avcodec_parameters_from_context(ctx.videoStream->codecpar, ctx.codecCtx);
    printf("avcodec_parameters_from_context response=%d %s\n", response, av_err2str(response));

    response = avformat_write_header(ctx.formatContext, &opt);
    printf("avformat_write_header response=%d %s\n", response, av_err2str(response));

    ctx.size = ctx.codecCtx->width * ctx.codecCtx->height;

    uint8_t* picture_buf = (uint8_t*) av_malloc((ctx.size * 3) / 2); /* size for YUV 420 */
    ctx.inputFrame = av_frame_alloc();
    ctx.inputFrame->format = ctx.codecCtx->pix_fmt;
    ctx.inputFrame->width = ctx.codecCtx->width;
    ctx.inputFrame->height = ctx.codecCtx->height;
    ctx.inputFrame->nb_samples = 0;
    ctx.inputFrame->channel_layout = 0;
    // ctx.inputFrame->data[0] = picture_buf;
    // ctx.inputFrame->data[1] = ctx.inputFrame->data[0] + ctx.size;
    // ctx.inputFrame->data[2] = ctx.inputFrame->data[1] + ctx.size / 4;
    // ctx.inputFrame->linesize[0] = ctx.codecCtx->width;
    // ctx.inputFrame->linesize[1] = ctx.codecCtx->width / 2;
    // ctx.inputFrame->linesize[2] = ctx.codecCtx->width / 2;

    printf("ctx->inputFrame = %p\n", ctx.inputFrame);

    // response = ctx.formatContext->oformat->write_header(ctx.formatContext);
    // response = avformat_write_header(ctx.formatContext, NULL);
    // printf("avformat_write_header response=%d %s\n", response, av_err2str(response));

    response = av_frame_get_buffer(ctx.inputFrame, 1);
    printf("av_frame_get_buffer response=%d %s\n", response, av_err2str(response));
    // if (response != 0) return response;

    ctx.packet = av_packet_alloc();

    ctx.swsCtx = sws_getContext(
        ctx.codecCtx->width,
        ctx.codecCtx->height,
        AV_PIX_FMT_RGBA,
        ctx.codecCtx->width,
        ctx.codecCtx->height,
        AV_PIX_FMT_YUV420P,
        0, 0, 0, 0
    );
    printf("sws_getContext swsCtx=%p\n", ctx.swsCtx);
}

void add_frame(uint8_t* rgbaData) {
    printf("ctx = %p rgbaData = %p\n", ctx, rgbaData);
    // printf("ctx->inputFrame = %p\n", ctx.inputFrame);`
    // hexdump(rgbaData, 4 * ctx.codecCtx->width);

    av_init_packet(ctx.packet);
    ctx.packet->data = NULL;
    ctx.packet->size = ctx.size;

    int response = av_frame_make_writable(ctx.inputFrame);
    printf("av_frame_make_writable response=%d %s\n", response, av_err2str(response));

    uint8_t* inputData[1] = { rgbaData };
    int inputLineSize[1] = { 4 * ctx.codecCtx->width };
    printf("inputData %p\n", inputData);
    printf("inputLineSize %p\n", inputLineSize);
    printf("ctx->codecCtx->height %d\n", ctx.codecCtx->height);
    printf("ctx->inputFrame->data %p\n", ctx.inputFrame->data);
    printf("ctx->inputFrame->linesize %d\n", ctx.inputFrame->linesize);

    sws_scale(
        ctx.swsCtx,
        inputData,
        inputLineSize,
        0,
        ctx.codecCtx->height,
        ctx.inputFrame->data,
        ctx.inputFrame->linesize
    );

    printf("sws_scale done..\n");

    ctx.inputFrame->pts = ctx.frameCounter++;

    printf("---> encode\n");
    encode(0);
}

unsigned long retrieve_video_size() {
    printf("retrieve_video_size\n");
    return ctx.bytesWritten;
}

uint8_t* retrieve_video() {
    // hexdump(ctx.outputBuffer, ctx.bytesWritten);
    printf("outputBuffer: %p | %lu\n", ctx.outputBuffer, ctx.outputBuffer);
    return ctx.outputBuffer;
    // outputBuffer = ctx.outputBuffer;
    // memcpy(outputBuffer, ctx.outputBuffer, ctx.bytesWritten);

    // hexdump(outputBuffer, 10);
}

void encode(int skip) {
        printf("encode: %p\n", ctx);
        int response = avcodec_send_frame(ctx.codecCtx, skip ? NULL : ctx.inputFrame);
        printf("avcodec_send_frame response=%d %s\n", response, av_err2str(response));

        while (response >= 0) {
            response = avcodec_receive_packet(ctx.codecCtx, ctx.packet);
            printf("avcodec_receive_packet response=%d %s\n", response, av_err2str(response));

            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                printf("skipping...\n");
                continue;
                // return;
            } else if (response < 0) {
                printf("other error\n");
                return;
            };

            // Allocate the buffer and write.
            int index = ctx.outputSize;
            ctx.outputSize += ctx.packet->size;
            // ctx.packet->stream_index = 1;
            // printf("Writing %d bytes to buffer index %d\n", ctx.packet->size, index);

            // if (ctx.outputBuffer) ctx.outputBuffer = realloc(ctx.outputBuffer, ctx.outputSize);
            // else ctx.outputBuffer = malloc(ctx.outputSize);

            // printf("Writing %d bytes to buffer\n", ctx.packet->size);

            // memcpy(&ctx.outputBuffer[index], ctx.packet->data, ctx.packet->size);
            av_write_frame(ctx.formatContext, ctx.packet);

            av_packet_unref(ctx.packet);

            break;
        }
}

void finalise() {
    printf("finalise\n");
    for (;;) {
        avcodec_send_frame(ctx.codecCtx, NULL);
        if (avcodec_receive_packet(ctx.codecCtx, ctx.packet) == 0) {
            av_interleaved_write_frame(ctx.formatContext, ctx.packet);
            av_packet_unref(ctx.packet);
        }
        else {
            break;
        }
    }

    av_write_trailer(ctx.formatContext);
}

// int main(int argc, char** argv) {
//     ffmpeg_context ctx;
//     printf("init %p\n", &ctx);
//     // init_context(&ctx, 128, 128);
//     printf("init_done %p\n", &ctx);
//     printf("w %d x h %d\n", ctx.codecCtx->width, ctx.codecCtx->height);

//     uint8_t* rgba32Data = (uint8_t*) av_calloc(1, 4 * ctx.codecCtx->width * ctx.codecCtx->height);
//     printf("rgbda32Data = %p\n", rgba32Data);

//     for (int i = 0; i < (1 * 25); i += 1) {
//         uint8_t *pos = rgba32Data;
//         int x, y;
//         for (y = 0; y < ctx.codecCtx->height; y++)
//         {
//             for (x = 0; x < ctx.codecCtx->width; x++)
//             {
//                 pos[0] = i / (float)25 * 255;
//                 pos[1] = 0;
//                 pos[2] = x / (float)(ctx.codecCtx->width) * 255;
//                 pos[3] = 255;
//                 pos += 4;
//             }
//         }

//         add_frame(&ctx, rgba32Data);
//     }

//     finalise(&ctx);

//     unsigned long size = retrieve_video_size(&ctx);
//     printf("size: %d\n", size);

//     hexdump(ctx.outputBuffer, ctx.outputSize);

//     // uint8_t* buffer;
//     // // retrieve_video(&ctx, buffer);

//     // hexdump(buffer, size);

//     FILE* fp = fopen("outfile.mp2", "wb");
//     fwrite(ctx.outputBuffer, 1, ctx.outputSize, fp);

//     fclose(fp);

//     return 0;
// }