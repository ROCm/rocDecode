# Video Decode Sample
This sample illustrates the FFMPEG demuxer to get the individual frames which are then decoded using rocDecode API and optionally color-converted using custom HIP kernels on AMD hardware. This sample converts decoded YUV output to one of the RGB or BGR formats(24bit, 32bit, 464bit)

## Build and run the sample:
```
mkdir build
cd build
cmake ..
make -j
./videodecodergb -i <input video file - required> -o <optional; output path to save decoded YUV frames> -d <GPU device ID, 0 for the first device, 1 for the second device, etc> -of <optional: output format bgr, bgra, bgr48, bgr64 etc>
```