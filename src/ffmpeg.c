#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avconfig.h>

#include <stdio.h>
#include <stdlib.h>

void encode(AVCodecContext* codecCtx, AVFrame* inputFrame, AVPacket* packet, FILE* fp) {
        if (inputFrame) printf("Send frame %3"PRId64"\n", inputFrame->pts);

        int response = avcodec_send_frame(codecCtx, inputFrame);
        printf("avcodec_send_frame response=%d %s\n", response, av_err2str(response));

        while (response >= 0) {
            response = avcodec_receive_packet(codecCtx, packet);
            printf("avcodec_receive_packet response=%d %s\n", response, av_err2str(response));

            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                printf("skipping...\n");
                continue;
            } else if (response < 0) {
                printf("other error\n");
                return;
            };

            printf("Write packet %3"PRId64" (size=%5d)\n", packet->pts, packet->size);
            fwrite(packet->data, 1, packet->size, fp);

            av_packet_unref(packet);

            // response = av_interleaved_write_frame(outputFormatCtx, packet);

            // printf("av_interleaved_write_frame response=%d %s\n", response, av_err2str(response));

            break;
        }
}

int main(int argc, char** argv) {
    FILE* fp = fopen("outfile.mp2", "wb+");
    AVFormatContext* formatContext = NULL;
    // int res = avformat_alloc_output_context2(&formatContext, NULL, NULL, "outfile.mp2");
    // printf("avformat_alloc_output_context2 %d\n", res);

    AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);

    // AVStream* stream = avformat_new_stream(formatContext, codec);

    int isEncoder = av_codec_is_encoder(codec);
    printf("av_codec_is_encoder %d\n", isEncoder);

    // AVCodecContext* codecCtx;

    // AVStream* stream = avformat_new_stream(formatContext, codec);
    // stream->time_base = (AVRational){ 1, 25 };

    // codecCtx = stream->codec;
    // codecCtx->bit_rate = 64000;
    // codecCtx->sample_rate = 44100;
    // codecCtx->channels = 2;
    // codecCtx->time_base.den = 1;
    // codecCtx->time_base.num = 1;
    codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    codecCtx->width = 128;
    codecCtx->height = 128;
    codecCtx->time_base = (AVRational){1, 25};
    codecCtx->framerate = (AVRational){25, 1};
    // codecCtx->gop_size = 10;
    // codecCtx->max_b_frames = 1;
    // av_opt_set(codecCtx->priv_data, "preset", "slow", 0);

    AVDictionary* opt = NULL;
	av_dict_set(&opt, "preset", "slow", 0);
    av_dict_set(&opt, "no-mbtree", "", 0);
    av_dict_set(&opt, "rc-lookahead", "0", 0);

    if (avcodec_open2(codecCtx, codec, &opt) < 0) {
        printf("Error when opening codec\n");

        return 1;
    }

    int size = codecCtx->width * codecCtx->height;
    // uint8_t* pictureBuffer = malloc((size * 3) / 2);

    AVFrame* inputFrame = av_frame_alloc();

    inputFrame->format = codecCtx->pix_fmt;
    inputFrame->width = codecCtx->width;
    inputFrame->height = codecCtx->height;
    inputFrame->nb_samples = 0;
    inputFrame->channel_layout = 0;

    int response = av_frame_get_buffer(inputFrame, 1);
    printf("av_frame_get_buffer response=%d %s\n", response, av_err2str(response));

    AVPacket* packet = av_packet_alloc();
    // size; //av_image_get_buffer_size(codecCtx->pix_fmt, codecCtx->width, codecCtx->height, 32);

    for (int i = 0; i < (10 * 25); i += 1) {
        fflush(stdout);

        av_init_packet(packet);
        packet->data = NULL;
        packet->size = size;

        // response = av_frame_make_writable(inputFrame);
        // printf("av_frame_make_writable response=%d %s\n", response, av_err2str(response));

        // int writable = av_frame_is_writable(inputFrame);
        // printf("av_frame_is_writable response=%d\n", writable);

        int x,y;
        for (y = 0; y < codecCtx->height; y++) {
            for (x = 0; x < codecCtx->width; x++) {
                inputFrame->data[0][y * inputFrame->linesize[0] + x] = x + y + i * 3;
            }
        }

        for (y = 0; y < codecCtx->height/2; y++) {
            for (x = 0; x < codecCtx->width/2; x++) {
                inputFrame->data[1][y * inputFrame->linesize[1] + x] = 128 + y + i * 2;
                inputFrame->data[2][y * inputFrame->linesize[2] + x] = 64 + x + i * 5;
            }
        }

        inputFrame->pts = i;

        // int output = 0;
        // int result = avcodec_encode_video2(codecCtx, packet, inputFrame, &output);

        // printf("result %d output %d\n", result, output);

        printf("----> encode\n");
        encode(codecCtx, inputFrame, packet, fp);

        // if (output) {
        //     packet->stream_index = stream->index;
        //     printf("---->\n");
        //     // av_interleaved_write_frame(formatContext, packet);
        //     av_write_frame(formatContext, packet);
        //     printf("====>\n");
        //     av_packet_unref(packet);
        // }

    }
    // encode(codecCtx, NULL, packet, fp);

    printf("√\n");

    return 0;
}