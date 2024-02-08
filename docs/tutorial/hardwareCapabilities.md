# rocDecode hardware capabilities

Table 1 shows the codec support and capabilities of the VCN for each GPU architecture supported by rocDecode.

| GPU Architecture                   | VCN Generation | Number of VCNs | H.265/HEVC | Max width, Max height - H.265 | H.264/AVC | Max width, Max height - H.264 |
| :--------------------------------- | :------------- | :------------- | :--------- | :---------------------------- | :-------- | :---------------------------- |
| gfx908 - MI1xx                     | VCN 2.5.0      | 2              | Yes        | 4096, 2176                    | No        | 4096, 2160                    |
| gfx90a - MI2xx                     | VCN 2.6.0      | 2              | Yes        | 4096, 2176                    | No        | 4096, 2160                    |
| gfx940, gfx942 - MI3xx             | VCN 3.0        | 3              | Yes        | 7680, 4320                    | No        | 4096, 2176                    |
| gfx941 - MI3xx                     | VCN 3.0        | 4              | Yes        | 7680, 4320                    | No        | 4096, 2176                    |
| gfx1030, gfx1031, gfx1032 - Navi2x | VCN 3.x        | 2              | Yes        | 7680, 4320                    | No        | 4096, 2176                    |
| gfx1100, gfx1102 - Navi3x          | VCN 4.0        | 2              | Yes        | 7680, 4320                    | No        | 4096, 2176                    |
| gfx1101 - Navi3x                   | VCN 4.0        | 1              | Yes        | 7680, 4320                    | No        | 4096, 2176                    |

Table 1: Hardware video decoder capabilities