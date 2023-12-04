## Pre-requisites to run python script
* Python3
* ```python3 -m pip install pandas```
* ```python3 -m pip install tabulate```

## Script to run rocDecode

```
python3 run_rocDecode_tests.py --help
```


usage:

```
usage: run_rocDecodeSamples.py [--rocDecode_directory ROCDECODE_DIRECTORY] 
                               [--gpu_device_id GPU_DEVICE_ID]
                               [--files_directory FILES_DIRECTORY]
                               [--sample_mode SAMPLE_MODE]
                               [--num_threads NUM_THREADS]

optional arguments:
  -h, --help            show this help message and exit
  --rocDecode_directory ROCDECODE_DIRECTORY
                        The rocDecode Samples Directory - required
  --gpu_device_id GPU_DEVICE_ID
                        The GPU device ID that will be used to run the test on it - optional (default:0 [range:0 - N] N = total number of available GPUs on a machine)
  --files_directory FILES_DIRECTORY
                        The path to a dirctory containing one or more supported files for decoding (e.g., mp4, mov, etc.) - required
  --sample_mode SAMPLE_MODE
                        The sample to run - optional (default:0 [range:0-1] 0: videoDecode, 1: videoDecodePerf)
  --num_threads NUM_THREADS
                        The number of threads for only for perf sample (sample_mode = 1) - optional (default:4)
```