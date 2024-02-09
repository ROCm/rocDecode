## Preparing the decoded frame for further processing
The decoded frames can be used for further postprocessing using the `rocDecGetVideoFrame()` API call. The successful completion of `rocDecGetVideoFrame()` indicates that the decoding process is completed and the device memory pointer is inter-opped into ROCm HIP address space for further processing of the decoded frame in device memory. The caller will get all the necessary information on the output surface like YUV format, dimensions, pitch, etc. from this call. In the high-level `RocVideoDecoder` class, we provide 4 different surface type modes for the mapped surface as specified in OutputSurfaceMemoryType as explained below.

    typedef enum OutputSurfaceMemoryType_enum {
        OUT_SURFACE_MEM_DEV_INTERNAL = 0,      /**<  Internal interopped decoded surface memory **/
        OUT_SURFACE_MEM_DEV_COPIED = 1,        /**<  decoded output will be copied to a separate device memory **/
        OUT_SURFACE_MEM_HOST_COPIED = 2        /**<  decoded output will be copied to a separate host memory **/
        OUT_SURFACE_MEM_NOT_MAPPED = 3         /**<  decoded output is not available (interop won't be used): useful for decode only performance app*/
    } OutputSurfaceMemoryType;

If the mapped surface type is `OUT_SURFACE_MEM_DEV_INTERNAL`, the direct pointer to the decoded surface is given to the user. The user is supposed to trigger `rocDecUnMapVideoFrame()` using the `ReleaseFrame()` call of the RocVideoDecoder class. If the requested surface type `OUT_SURFACE_MEM_DEV_COPIED` or `OUT_SURFACE_MEM_HOST_COPIED`, the internal decoded frame will be copied to another buffer either in device memory or host memory. After that, it is immediately unmapped for re-use by the RocVideoDecoder class.
In all the cases, the user needs to call `rocDecUnMapVideoFrame()` to indicate that the frame is ready to be used for decoding again.
Please refer to the RocVideoDecoder class and samples for detailed use of these APIs.
