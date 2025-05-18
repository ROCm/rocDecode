# Video Decode Validation Sample

This sample combines performance testing capabilities from `videoDecodePerf` with output validation features from `videoDecode`. It allows you to measure the performance of the video decoder while also validating the decoded output against reference data or saving the output for further analysis.

## Features

- Performance measurement of video decoding (FPS, decoding time)
- Optional output of decoded frames to file (using thread 0 when multiple threads are used)
- Support for multiple threads for performance testing
- Support for different output memory types
- Device selection for multi-GPU systems
- MD5 hash generation and validation
- Asynchronous frame output (default) with synchronous option

## Build Instructions

Navigate to the sample directory and run:

```shell
mkdir build && cd build
cmake ..
make -j
```

## Usage

```shell
./videodecodevalidation -i <input_file> [options]
```

### Options

- `-i <file_path>` - Input file path (required)
- `-o <file_path>` - Output file path to dump frames (optional)
  - When using multiple threads, only thread 0 will write output
- `-t <number>` - Number of threads (>= 1) (optional; default: 1)
- `-d <device_id>` - GPU device ID (optional; default: 0)
- `-z` - Force zero latency (decoded frames flushed immediately) (optional)
- `-disp_delay <number>` - Specify frames to delay for display (optional)
- `-m <memory_type>` - Memory type for decoded output (optional; default: 3)
  - 0: Decoded output in internal interop memory
  - 1: Decoded output copied to separate device memory
  - 2: Decoded output copied to host memory
  - 3: Decoded output not available (decode only)
- `-f <number>` - Number of frames to decode (optional; default: whole file)
- `-md5` - Generate MD5 hash of decoded output (optional)
- `-md5_check <file>` - Check decoded output against reference MD5 (optional)
- `-sync_output` - Use synchronous frame output (default: asynchronous) (optional)

## Example

To decode and validate the first 100 frames of a video with performance metrics:

```shell
./videodecodevalidation -i /path/to/video.mp4 -f 100 -md5
```

To decode with multiple threads for performance testing and save the output:

```shell
./videodecodevalidation -i /path/to/video.mp4 -t 4 -o /path/to/output_frames -m 2
```

To decode and compare output against a reference MD5 hash:

```shell
./videodecodevalidation -i /path/to/video.mp4 -md5_check /path/to/reference.md5
```

## Notes

- For best performance results, use `-m 3` (decode only) when measurements are the primary goal.
- For output validation, use `-t 5` along with the `-o` option.
- When using multiple threads, only thread 0 will handle file output to maximize decoder throughput.
- The default asynchronous frame output is designed to improve VCN utilization. Use `-sync_output` if you need synchronous operation.
- MD5 hash generation works with all threads but validation uses the results from thread 0.