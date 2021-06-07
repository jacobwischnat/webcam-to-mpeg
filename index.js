import FFMPEG from './wasm/dist/ffmpeg.js';

const video = document.querySelector('video');
const canvas = document.querySelector('canvas');
const context = canvas.getContext('2d');

console.dir(FFMPEG);

const init = async (width, height) => {
    const module = await FFMPEG();

    // const ptr = module._malloc(10 * 1024 * 1024 * 1024);
    const pointer = module._malloc(10 * 1024 * 1024 * 1024);

    // const pointer = new DataView(module.HEAPU8.buffer).getUint32(ptr);

    // console.log({'module.HEAPU8': module.HEAPU8});

    const init_context = module.cwrap('init_context', 'void', ['int', 'int']);
    const add_frame = module.cwrap('add_frame', 'void', ['number']);
    const retrieve_video = module.cwrap('retrieve_video', 'number', []);
    const retrieve_video_size = module.cwrap('retrieve_video_size', 'unsigned long', []);
    const finalise = module.cwrap('finalise', 'void', []);

    canvas.width = width;
    canvas.height = height;

    init_context(width, height);
    console.log('pointer = 0x', pointer.toString(16));

    const finished = () => {
        finalise();

        const size = retrieve_video_size();
        console.log(`Video size is: ${size}`);
        // const videoBuffer = module._malloc(4);
        // const videoPointer = module._malloc(size);
        const videoPointer = retrieve_video();
        console.log('videoPointer', videoPointer, videoPointer.toString(16));

        const buffer = module.HEAPU8.subarray(videoPointer, videoPointer + size);
        // console.log({buffer});

        const blob = new Blob([buffer], {type : 'video/mpeg2'});

        // const vid = document.createElement('video');

        // const url = URL.createObjectURL(blob);
        // vid.onloadedmetadata = () => vid.play();
        // vid.src = url;

        // document.body.appendChild(vid);

        const objectURL = URL.createObjectURL( blob );

        const link = document.createElement('a');

        link.href = objectURL;
        link.href = URL.createObjectURL( blob );
        link.download = 'video.mp4';
        link.click();
    }

    const startTime = Date.now() / 1000;
    const render = () => {
        if (((Date.now() / 1000) - startTime) > 10) {
            finished();

            return console.warn('STOPPING RECORDING...');
        }

        context.drawImage(video, 0, 0);

        const imageData = context.getImageData(0, 0, width, height);
        // console.log('const imageBuffer = module._malloc(imageData.data.length);');
        const imageBuffer = module._malloc(imageData.data.length);
        module.HEAPU8.set(imageData.data, imageBuffer);

        // console.log(module.HEAPU8.slice(imageBuffer, imageBuffer + imageData.data.length));

        // console.log('pointer = 0x', imageBuffer.toString(16));

        add_frame(imageBuffer);

        // console.log('module._free(imageBuffer);');
        // module._free(imageBuffer);

        window.requestAnimationFrame(() => render());
    }

    window.requestAnimationFrame(() => render());
}

video.addEventListener('loadedmetadata', () => {
    video.play();
    // init(video.videoWidth, video.videoHeight);
    // init(128, 128);
}, {once: true});

navigator.mediaDevices.getUserMedia({video: true, audio: true}).then(async stream => {
    video.srcObject = stream;

    let haveData = false;
    const mr = new MediaRecorder(stream, {video: true, audio: true});
    mr.ondataavailable = ({data}) => {
        haveData = true;
        console.warn('Data Available');
        console.log(data);

        video.onloadedmetadata = () => video.play();
        video.srcObject = null;
        video.loop = true;
        video.volume = 1;
        video.src = URL.createObjectURL(data);
    }
    mr.onstart = () => console.warn('Started');
    mr.onpause = () => console.warn('Paused');
    mr.onstop = () => console.warn('Stopped');
    console.log(mr);
    console.log('starting');
    mr.start();
    setTimeout(() => {
        console.log('stopping');
        mr.stop();
    }, 5000);
});