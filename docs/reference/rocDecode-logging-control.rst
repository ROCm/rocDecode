.. meta::
  :description: Using rocDecode
  :keywords: parse video, parse, decode, video decoder, video decoding, rocDecode, core APIs, AMD, ROCm

********************************************************************
rocDecode logging control
********************************************************************

rocDecode core components can generate various logs during the decode session. The logs can be critical status reports, error reports, warnings, etc. 
Each log has a significance level associated to it. The level can be from critical (0) to debug info related (4). Lower value has greater importance. 
An internal logging level threshold can be set to control the verbosity of logging output from each rocDecode component. If a log's level value is less 
than or equal to the logging level threshold, the log is output. Otherwise, the log is not output.

The logging level threshold can be set by the environment variable ROCDEC_LOG_LEVEL, or by calling the SetLogLevel method of the logger class. The default 
logging level is 0 (critical log only).

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
