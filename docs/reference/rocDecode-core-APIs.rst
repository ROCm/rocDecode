.. meta::
  :description: Using rocDecode
  :keywords: parse video, parse, decode, video decoder, video decoding, rocDecode, core APIs, AMD, ROCm

********************************************************************
The rocDecode core APIs
********************************************************************

The rocDecode core APIs are intended for users who find that the utility classes aren't sufficient for their implementations. The :doc:`Using the rocDecode videodecode sample <../how-to/using-rocDecode-videodecode-sample>` provides an introduction to using the utility classes.

The rocDecode core APIs are exposed in header files in the |apifolder|_ folder of the `rocDecode GitHub repository <https://github.com/ROCm/rocDecode>`_. 

:doc:`The rocDecode parser API <./rocDecode-parser>` is exposed in |rocparser|_. It contains functions that create and destroy the parser, and that parse the bitstream.

:doc:`The hardware decoder API <./rocDecode-hw-decoder>` is exposed in |rocdecode|_. It contains functions that create, control, and destroy the decoder, and decode the parsed frames on the GPU.

:doc:`The software decoder API <./rocDecode-sw-decoder>` is exposed in |rocdecodehost|_. It contains the same functionality as ``rocdecode.h``, but the all the operations are run on the host rather than the GPU.

:doc:`The bitstream reader API <../how-to/using-rocDecode-bitstream>` is exposed in |bitstreamreader|_. It contains a simplified set of APIs that provide a way to use and test the decoder without relying on FFMpeg.

.. |apifolder| replace:: ``api/rocdecode``
.. _apifolder: https://github.com/ROCm/rocDecode/tree/develop/api/rocdecode

.. |rocparser| replace:: ``api/rocdecode/rocparser.h``
.. _rocparser: https://github.com/ROCm/rocDecode/tree/develop/api/rocdecode/rocparser.h

.. |rocdecode| replace:: ``api/rocDecode/rocdecode.h``
.. _rocdecode: https://github.com/ROCm/rocDecode/tree/develop/api/rocdecode/rocdecode.h

.. |rocdecodehost| replace:: ``api/rocDecode/rocdecode_host.h``
.. _rocdecodehost: https://github.com/ROCm/rocDecode/tree/develop/api/rocdecode/rocdecode_host.h

.. |bitstreamreader| replace:: ``api/rocDecode/roc_bitstream_reader.h``
.. _bitstreamreader: https://github.com/ROCm/rocDecode/tree/develop/api/rocdecode/roc_bitstream_reader.h

.. |utilsfolder| replace:: ``utils`` folder
.. _utilsfolder: https://github.com/ROCm/rocDecode/tree/develop/utils
