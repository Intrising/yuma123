/*
 * Copyright (c) 2009, Andy Bierman
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*********************************************************************\

MODULE NAME:    b64.c

AUTHOR:         Bob Trower 08/04/01

PROJECT:        Crypt Data Packaging

COPYRIGHT:      Copyright (c) Trantor Standard Systems Inc., 2001

NOTE:           This source code may be used as you wish, subject to
                the MIT license.  See the LICENCE section below.

DESCRIPTION:
                This little utility implements the Base64
                Content-Transfer-Encoding standard described in
                RFC1113 (http://www.faqs.org/rfcs/rfc1113.html).

                This is the coding scheme used by MIME to allow
                binary data to be transferred by SMTP mail.

                Groups of 3 bytes from a binary stream are coded as
                groups of 4 bytes in a text stream.

                The input stream is 'padded' with zeros to create
                an input that is an even multiple of 3.

                A special character ('=') is used to denote padding so
                that the stream can be decoded back to its exact size.

                Encoded output is formatted in lines which should
                be a maximum of 72 characters to conform to the
                specification.  This program defaults to 72 characters,
                but will allow more or less through the use of a
                switch.  The program enforces a minimum line size
                of 4 characters.

                Example encoding:

                The stream 'ABCD' is 32 bits long.  It is mapped as
                follows:

                ABCD

                 A (65)     B (66)     C (67)     D (68)   (None) (None)
                01000001   01000010   01000011   01000100

                16 (Q)  20 (U)  9 (J)   3 (D)    17 (R) 0 (A)  NA (=) NA (=)
                010000  010100  001001  000011   010001 000000 000000 000000


                QUJDRA==

                Decoding is the process in reverse.  A 'decode' lookup
                table has been created to avoid string scans.

DESIGN GOALS:   Specifically:
                Code is a stand-alone utility to perform base64 
                encoding/decoding. It should be genuinely useful 
                when the need arises and it meets a need that is 
                likely to occur for some users.  
                Code acts as sample code to show the author's 
                design and coding style.  

                Generally: 
                This program is designed to survive:
                Everything you need is in a single source file.
                It compiles cleanly using a vanilla ANSI C compiler.
                It does its job correctly with a minimum of fuss.  
                The code is not overly clever, not overly simplistic 
                and not overly verbose. 
                Access is 'cut and paste' from a web page.  
                Terms of use are reasonable.  

VALIDATION:     Non-trivial code is never without errors.  This
                file likely has some problems, since it has only
                been tested by the author.  It is expected with most
                source code that there is a period of 'burn-in' when
                problems are identified and corrected.  That being
                said, it is possible to have 'reasonably correct'
                code by following a regime of unit test that covers
                the most likely cases and regression testing prior
                to release.  This has been done with this code and
                it has a good probability of performing as expected.

                Unit Test Cases:

                case 0:empty file:
                    CASE0.DAT  ->  ->
                    (Zero length target file created
                    on both encode and decode.)

                case 1:One input character:
                    CASE1.DAT A -> QQ== -> A

                case 2:Two input characters:
                    CASE2.DAT AB -> QUJD -> AB

                case 3:Three input characters:
                    CASE3.DAT ABC -> QUJD -> ABC

                case 4:Four input characters:
                    case4.dat ABCD -> QUJDRA== -> ABCD

                case 5:All chars from 0 to ff, linesize set to 50:

                    AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIj
                    JCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZH
                    SElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWpr
                    bG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6P
                    kJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKz
                    tLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX
                    2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7
                    /P3+/w==

                case 6:Mime Block from e-mail:
                    (Data same as test case 5)

                case 7: Large files:
                    Tested 28 MB file in/out.

                case 8: Random Binary Integrity:
                    This binary program (b64.exe) was encoded to base64,
                    back to binary and then executed.

                case 9 Stress:
                    All files in a working directory encoded/decoded
                    and compared with file comparison utility to
                    ensure that multiple runs do not cause problems
                    such as exhausting file handles, tmp storage, etc.

                -------------

                Syntax, operation and failure:
                    All options/switches tested.  Performs as
                    expected.

                case 10:
                    No Args -- Shows Usage Screen
                    Return Code 1 (Invalid Syntax)
                case 11:
                    One Arg (invalid) -- Shows Usage Screen
                    Return Code 1 (Invalid Syntax)
                case 12:
                    One Arg Help (-?) -- Shows detailed Usage Screen.
                    Return Code 0 (Success -- help request is valid).
                case 13:
                    One Arg Help (-h) -- Shows detailed Usage Screen.
                    Return Code 0 (Success -- help request is valid).
                case 14:
                    One Arg (valid) -- Uses stdin/stdout (filter)
                    Return Code 0 (Sucess)
                case 15:
                    Two Args (invalid file) -- shows system error.
                    Return Code 2 (File Error)
                case 16:
                    Encode non-existent file -- shows system error.
                    Return Code 2 (File Error)
                case 17:
                    Out of disk space -- shows system error.
                    Return Code 3 (File I/O Error)

                -------------

                Compile/Regression test:
                    gcc compiled binary under Cygwin
                    Microsoft Visual Studio under Windows 2000
                    Microsoft Version 6.0 C under Windows 2000

DEPENDENCIES:   None

LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

                Permission is hereby granted, free of charge, to any person
                obtaining a copy of this software and associated
                documentation files (the "Software"), to deal in the
                Software without restriction, including without limitation
                the rights to use, copy, modify, merge, publish, distribute,
                sublicense, and/or sell copies of the Software, and to
                permit persons to whom the Software is furnished to do so,
                subject to the following conditions:

                The above copyright notice and this permission notice shall
                be included in all copies or substantial portions of the
                Software.

                THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
                KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
                WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
                PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
                OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
                OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
                OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
                SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

VERSION HISTORY:
                Bob Trower 08/04/01 -- Create Version 0.00.00B

                Andy Bierman 10/25/08 - Adapted for use in YANG tools

\******************************************************************* */

#include <stdio.h>
#include <stdlib.h>

#include "status.h"
#include "b64.h"


/*
** Translation Table as described in RFC1113
*/
static const char cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
** Translation Table to decode (created by author)
*/
static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

/*
** encodeblock
**
** encode 3 8-bit binary bytes as 4 '6-bit' characters
*/
static void encodeblock( unsigned char in[3], 
                         unsigned char out[4], 
                         unsigned int len )
{
    out[0] = cb64[ in[0] >> 2 ];
    out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
    out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
    out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}


/*
** decodeblock
**
** decode 4 '6-bit' characters into 3 8-bit binary bytes
*/
static void decodeblock( unsigned char in[4], unsigned char out[3] )
{   
    out[0] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
    out[1] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
    out[2] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
}


/*************** E X T E R N A L    F U N C T I O N S  *************/


/*
 * b64_encode
 *
 * base64 encode a stream adding padding and line breaks as per spec.
 *
 * adapted from the b64 function for 2 FILE parameters
 * using internal buffers instead
 *
 * INPUTS:
 *   inbuff == pointer to buffer of binary chars
 *   inbufflen == number of binary chars in inbuff
 *   outbuff == pointer to the output buffer to use
 *              MAY BE NULL; just get encoded length in that case
 *   outbufflen == max number of chars to write to outbuff
 *   linesize == the output line length to use
 *   retlen == address of return length
 *   
 * OUTPUTS:
 *   *retlen == number of chars written to outbuff,
 *             or would have been written if it was non-NULL
 *
 * RETURNS:
 *   status:
 *     NO_ERR if all OK
 *     ERR_BUFF_OVFL if outbuff not big enough
 */
status_t
    b64_encode (const unsigned char *inbuff,
                unsigned int inbufflen,
                unsigned char *outbuff, 
                unsigned int outbufflen,
                unsigned int linesize,
                unsigned int *retlen)

{
    unsigned char in[3], out[4];
    unsigned int i, len, inx, outx, blocksout;

#ifdef DEBUG
    if (!inbuff || !retlen) {
        return ERR_INTERNAL_PTR;
    }
    if (outbufflen<4 || linesize < 3) {
        return ERR_INTERNAL_VAL;
    }
#endif

    *retlen = 0;

    if (inbufflen == 0) {
        return NO_ERR;
    }

    blocksout = 0;
    inx = 0;
    outx = 0;
    outbufflen--;    /* save 1 spot for the NULL-terminator */

    while (inx < inbufflen) {
        len = 0;

        for (i = 0; i < 3; i++) {
            if (inx < inbufflen) {
                in[i] = inbuff[inx++];
                len++;
            } else {
                in[i] = (unsigned char)0;
            }
        }

        if (len != 0) {
            encodeblock(in, out, len);
            for (i = 0; i < 4; i++) {
                if (outx < outbufflen) {
                    if (outbuff) {
                        outbuff[outx++] = out[i];
                    } else {
                        outx++;
                    }
                } else {
                    return ERR_BUFF_OVFL;
                }
            }
            blocksout++;
        }

        if ((blocksout >= (linesize/4)) || (inx==inbufflen)) {
            if (blocksout > 0) {
                if (outx+1 < outbufflen) {
                    if (outbuff) {
                        outbuff[outx++] = '\r';
                        outbuff[outx++] = '\n';
                    } else {
                        outx += 2;
                    }
                } else {
                    return ERR_BUFF_OVFL;
                }
            }
            blocksout = 0;
        }
    }

    if (outbuff) {
        outbuff[outx] = 0;
    }

    *retlen = outx;
    return NO_ERR;

}  /* b64_encode */


/*
 * b64_decode
 *
 * decode a base64 encoded stream discarding padding, line breaks and noise
 *
 * INPUTS:
 *   inbuff == pointer to buffer of base64 chars
 *   inbufflen == number of chars in inbuff
 *   outbuff == pointer to the output buffer to use
 *              MAY BE NULL; just get encoded length in that case
 *   outbufflen == max number of chars to write to outbuff
 *   linesize == the output line length to use
 *   retlen == address of return length
 *   
 * OUTPUTS:
 *   *retlen == number of chars written to outbuff
 *
 * RETURNS:
 *   status:
 *     NO_ERR if all OK
 *     ERR_BUFF_OVFL if outbuff not big enough
 */

status_t
    b64_decode (const unsigned char *inbuff, 
                unsigned int inbufflen,
                unsigned char *outbuff,
                unsigned int outbufflen,
                unsigned int *retlen)
{
    unsigned char in[4], out[3], v;
    unsigned int i, len, inx, outx;

#ifdef DEBUG
    if (!inbuff || !retlen) {
        return ERR_INTERNAL_PTR;
    }
    if (!inbufflen || !outbufflen) {
        return ERR_INTERNAL_VAL;
    }
#endif

    inx = 0;
    outx = 0;

    while (inx < inbufflen) {
        for (len = 0, i = 0; i < 4 && inx < inbufflen; i++) {
            v = 0;
            while (inx < inbufflen && v == 0) {
                v = inbuff[inx++];
                v = (unsigned char) ((v < 43 || v > 122) ? 0 : cd64[ v - 43 ]);
                if (v) {
                    v = (unsigned char) ((v == '$') ? 0 : v - 61);
                }
            }

            if (inx < inbufflen) {
                len++;
                if (v) {
                    in[i] = (unsigned char)(v - 1);
                }
            } else {
                in[i] = 0;
            }
        }

        if (len) {
            decodeblock(in, out);
            for (i = 0; i < len - 1; i++ ) {
                if (outx < outbufflen) {
                    if (outbuff) {
                        outbuff[outx++] = out[i];
                    } else {
                        outx++;
                    }
                } else {
                    return ERR_BUFF_OVFL;
                }
            }
        }
    }

    *retlen = outx;
    return NO_ERR;

}  /* b64_decode */


/* END b64.c */

