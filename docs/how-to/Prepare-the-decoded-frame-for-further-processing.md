## Preparing the decoded frame for further processing
The decoded frames can be used for further postprocessing using the rocDecGetVideoFrame() API call. The successful completion of rocDecGetVideoFrame() indicates that the decoding process is completed and the device memory pointer is inter-opped into the ROCm HIP address space to process the decoded frame in device memory further. The caller gets the necessary information on the output surface, like YUV format, dimensions, pitch, and others, from this call. In the high-level RocVideoDecoder class, we provide four different surface type modes for the mapped surface as specified in OutputSurfaceMemoryType, as explained below.

    typedef enum OutputSurfaceMemoryType_enum {
        OUT_SURFACE_MEM_DEV_INTERNAL = 0,      /**<  Internal interopped decoded surface memory **/
        OUT_SURFACE_MEM_DEV_COPIED = 1,        /**<  decoded output will be copied to a separate device memory **/
        OUT_SURFACE_MEM_HOST_COPIED = 2        /**<  decoded output will be copied to a separate host memory **/
        OUT_SURFACE_MEM_NOT_MAPPED = 3         /**<  decoded output is not available (interop won't be used): useful for decode only performance app*/
    } OutputSurfaceMemoryType;

If the mapped surface type is `OUT_SURFACE_MEM_DEV_INTERNAL`, the direct pointer to the decoded surface is given to the user. The user must call `ReleaseFrame()` of the RocVideoDecoder class. If the requested surface type is `OUT_SURFACE_MEM_DEV_COPIED` or `OUT_SURFACE_MEM_HOST_COPIED`, the internal decoded frame is copied to another buffer either in device memory or host memory. After that, it is immediately unmapped for re-use by the RocVideoDecoder class.

Refer to the RocVideoDecoder class and [samples](https://github.com/ROCm/rocDecode/tree/develop/samples) for details on how to use the APIs.

