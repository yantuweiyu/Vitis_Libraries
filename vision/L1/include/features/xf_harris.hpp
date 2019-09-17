/***************************************************************************
Copyright (c) 2016, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CxFSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ***************************************************************************/

#ifndef _XF_HARRIS_HPP_
#define _XF_HARRIS_HPP_

#ifndef __cplusplus
#error C++ is needed to use this file!
#endif

#include "hls_stream.h"
#include "common/xf_common.h"
#include "common/xf_utility.h"

#include "imgproc/xf_box_filter.hpp"
#include "imgproc/xf_sobel.hpp"
#include "xf_harris_utils.hpp"
#include "xf_max_suppression.hpp"
#include "xf_pack_corners.hpp"

#ifndef __SYNTHESIS__
#include <iostream>
#endif

namespace xf {
namespace cv {

/************************************************************************
 * xFCornerHarrisDetector : CornerHarris function to find corners in the image
 ************************************************************************/
template <int FILTERSIZE,
          int BLOCKWIDTH,
          int SRC_T,
          int ROWS,
          int COLS,
          int CHANNELINFO,
          int IN_DEPTH,
          int NPC,
          int IN_WW,
          int OUT_WW,
          int TC,
          int GRAD_WW,
          int DET_WW,
          bool USE_URAM>
void xFCornerHarrisDetector(xf::cv::Mat<SRC_T, ROWS, COLS, NPC>& _src_mat,
                            xf::cv::Mat<SRC_T, ROWS, COLS, NPC>& _dst_mat,
                            uint16_t img_height,
                            uint16_t img_width,
                            uint16_t _nms_radius,
                            uint16_t _threshold,
                            uint16_t k) {
    int scale;
    if (FILTERSIZE == XF_FILTER_3X3)
        scale = 6;
    else if (FILTERSIZE == XF_FILTER_5X5)
        scale = 12;
    else if (FILTERSIZE == XF_FILTER_7X7)
        scale = 1;

    xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> gradx_2(img_height, img_width);
    xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> grady_2(img_height, img_width);
    xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> gradxy(img_height, img_width);
    xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> gradx2g(img_height, img_width);
    xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> grady2g(img_height, img_width);
    xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> gradxyg(img_height, img_width);
    xf::cv::Mat<XF_32SC1, ROWS, COLS, NPC> score(img_height, img_width);
    xf::cv::Mat<XF_32SC1, ROWS, COLS, NPC> thresh(img_height, img_width);

    #pragma HLS STREAM variable=gradx_2.data depth=2
    #pragma HLS STREAM variable=grady_2.data depth=2
    #pragma HLS STREAM variable=gradxy.data depth=2
    #pragma HLS STREAM variable=grady2g.data depth=2
    #pragma HLS STREAM variable=gradx2g.data depth=2
    #pragma HLS STREAM variable=gradxyg.data depth=2
    #pragma HLS STREAM variable=score.data depth=2
    #pragma HLS STREAM variable=thresh.data depth=2

    #pragma HLS DATAFLOW

    if (FILTERSIZE == XF_FILTER_7X7) {
        xf::cv::Mat<XF_32SC1, ROWS, COLS, NPC> gradx_mat(img_height, img_width);
        xf::cv::Mat<XF_32SC1, ROWS, COLS, NPC> grady_mat(img_height, img_width);
        xf::cv::Mat<XF_32SC1, ROWS, COLS, NPC> gradx1_mat(img_height, img_width);
        xf::cv::Mat<XF_32SC1, ROWS, COLS, NPC> gradx2_mat(img_height, img_width);
        xf::cv::Mat<XF_32SC1, ROWS, COLS, NPC> grady1_mat(img_height, img_width);
        xf::cv::Mat<XF_32SC1, ROWS, COLS, NPC> grady2_mat(img_height, img_width);

        #pragma HLS STREAM variable=gradx_mat.data depth=2
        #pragma HLS STREAM variable=grady_mat.data depth=2
        #pragma HLS STREAM variable=gradx1_mat.data depth=2
        #pragma HLS STREAM variable=gradx2_mat.data depth=2
        #pragma HLS STREAM variable=grady1_mat.data depth=2
        #pragma HLS STREAM variable=grady2_mat.data depth=2

        Sobel<XF_BORDER_CONSTANT, FILTERSIZE, SRC_T, XF_32SC1, ROWS, COLS, NPC, USE_URAM>(_src_mat, gradx_mat,
                                                                                          grady_mat);

        xFDuplicate<XF_32SC1, ROWS, COLS, XF_32SP, NPC, DET_WW, TC>(gradx_mat, gradx1_mat, gradx2_mat, img_height,
                                                                    img_width);

        xFDuplicate<XF_32SC1, ROWS, COLS, XF_32SP, NPC, DET_WW, TC>(grady_mat, grady1_mat, grady2_mat, img_height,
                                                                    img_width);

        xFSquare<XF_32SC1, XF_16SC1, ROWS, COLS, XF_32SP, XF_16SP, NPC, DET_WW, GRAD_WW, TC>(
            gradx1_mat, gradx_2, scale, FILTERSIZE, img_height, img_width);

        xFSquare<XF_32SC1, XF_16SC1, ROWS, COLS, XF_32SP, XF_16SP, NPC, DET_WW, GRAD_WW, TC>(
            grady1_mat, grady_2, scale, FILTERSIZE, img_height, img_width);

        xFMultiply<XF_32SC1, XF_16SC1, ROWS, COLS, XF_32SP, XF_16SP, NPC, DET_WW, GRAD_WW, TC>(
            gradx2_mat, grady2_mat, gradxy, scale, FILTERSIZE, img_height, img_width);
    } else {
        xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> gradx_mat(img_height, img_width);
        xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> grady_mat(img_height, img_width);
        xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> gradx1_mat(img_height, img_width);
        xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> grady1_mat(img_height, img_width);
        xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> gradx2_mat(img_height, img_width);
        xf::cv::Mat<XF_16SC1, ROWS, COLS, NPC> grady2_mat(img_height, img_width);

        #pragma HLS STREAM variable=gradx_mat.data depth=2
        #pragma HLS STREAM variable=grady_mat.data depth=2
        #pragma HLS STREAM variable=gradx1_mat.data depth=2
        #pragma HLS STREAM variable=gradx2_mat.data depth=2
        #pragma HLS STREAM variable=grady1_mat.data depth=2
        #pragma HLS STREAM variable=grady2_mat.data depth=2

        Sobel<XF_BORDER_CONSTANT, FILTERSIZE, SRC_T, XF_16SC1, ROWS, COLS, NPC, USE_URAM>(_src_mat, gradx_mat,
                                                                                          grady_mat);

        xFDuplicate<XF_16SC1, ROWS, COLS, XF_16SP, NPC, GRAD_WW, TC>(gradx_mat, gradx1_mat, gradx2_mat, img_height,
                                                                     img_width);

        xFDuplicate<XF_16SC1, ROWS, COLS, XF_16SP, NPC, GRAD_WW, TC>(grady_mat, grady1_mat, grady2_mat, img_height,
                                                                     img_width);

        xFSquare<XF_16SC1, XF_16SC1, ROWS, COLS, XF_16SP, XF_16SP, NPC, GRAD_WW, GRAD_WW, TC>(
            gradx1_mat, gradx_2, scale, FILTERSIZE, img_height, img_width);

        xFSquare<XF_16SC1, XF_16SC1, ROWS, COLS, XF_16SP, XF_16SP, NPC, GRAD_WW, GRAD_WW, TC>(
            grady1_mat, grady_2, scale, FILTERSIZE, img_height, img_width);

        xFMultiply<XF_16SC1, XF_16SC1, ROWS, COLS, XF_16SP, XF_16SP, NPC, GRAD_WW, GRAD_WW, TC>(
            gradx2_mat, grady2_mat, gradxy, scale, FILTERSIZE, img_height, img_width);
    }

    boxFilter<XF_BORDER_CONSTANT, BLOCKWIDTH, XF_16SC1, ROWS, COLS, NPC, USE_URAM>(gradx_2, gradx2g);
    boxFilter<XF_BORDER_CONSTANT, BLOCKWIDTH, XF_16SC1, ROWS, COLS, NPC, USE_URAM>(grady_2, grady2g);
    boxFilter<XF_BORDER_CONSTANT, BLOCKWIDTH, XF_16SC1, ROWS, COLS, NPC, USE_URAM>(gradxy, gradxyg);

    xFComputeScore<XF_16SC1, XF_32SC1, ROWS, COLS, XF_16SP, XF_32SP, NPC, GRAD_WW, DET_WW, TC>(
        gradx2g, grady2g, gradxyg, score, img_height, img_width, k, FILTERSIZE);

    xFThreshold<XF_32SC1, ROWS, COLS, XF_32SP, NPC, DET_WW, TC>(score, thresh, _threshold, img_height, img_width);

    xFMaxSuppression<XF_32SC1, SRC_T, ROWS, COLS, XF_32SP, XF_8UP, NPC, DET_WW, IN_WW>(thresh, _dst_mat, _nms_radius,
                                                                                       img_height, img_width);
}

#pragma SDS data access_pattern("src.data":SEQUENTIAL)
#pragma SDS data access_pattern("dst.data":SEQUENTIAL)

#pragma SDS data copy ("src.data"[0:"src.size"])
#pragma SDS data copy ("dst.data"[0:"dst.size"])
//#pragma SDS data mem_attribute("src.data":NON_CACHEABLE|PHYSICAL_CONTIGUOUS)
//#pragma SDS data mem_attribute("dst.data":NON_CACHEABLE|PHYSICAL_CONTIGUOUS)

template <int FILTERSIZE,
          int BLOCKWIDTH,
          int NMSRADIUS,
          int SRC_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          bool USE_URAM = false>
void cornerHarris(xf::cv::Mat<SRC_T, ROWS, COLS, NPC>& src,
                  xf::cv::Mat<SRC_T, ROWS, COLS, NPC>& dst,
                  uint16_t threshold,
                  uint16_t k) {
    #pragma HLS inline off

    uint16_t img_height = src.rows;
    uint16_t img_width = src.cols; //>> XF_BITSHIFT(NPC);

    /*assert(((FILTERSIZE == XF_FILTER_3X3) || (FILTERSIZE == XF_FILTER_5X5) ||
                    (FILTERSIZE == XF_FILTER_7X7)) && "filter width must be 3, 5 or 7");

    assert(((BLOCKWIDTH == XF_FILTER_3X3) || (BLOCKWIDTH == XF_FILTER_5X5) ||
                    (BLOCKWIDTH == XF_FILTER_7X7)) && "block width must be 3, 5 or 7");

    assert(((NMSRADIUS == XF_NMS_RADIUS_1) || (NMSRADIUS == XF_NMS_RADIUS_2))
                    && "radius size must be 1, 2");

    assert(((img_height <= ROWS ) && (src.cols <= COLS)) && "ROWS and COLS should be greater than input image");

    assert(((NPC == XF_NPPC1) || (NPC == XF_NPPC8)) &&
                    " NPC must be 0 or 3 ");*/

    if (NPC == XF_NPPC8) {
        xFCornerHarrisDetector<FILTERSIZE, BLOCKWIDTH, SRC_T, ROWS, COLS, XF_CHANNELS(SRC_T, NPC), XF_DEPTH(SRC_T, NPC),
                               NPC, XF_WORDWIDTH(SRC_T, NPC), XF_32UW, (COLS >> XF_BITSHIFT(NPC)), XF_128UW, XF_256UW,
                               USE_URAM>(src, dst, img_height, img_width, NMSRADIUS, threshold, k);
    } else if (NPC == XF_NPPC1) {
        xFCornerHarrisDetector<FILTERSIZE, BLOCKWIDTH, SRC_T, ROWS, COLS, XF_CHANNELS(SRC_T, NPC), XF_DEPTH(SRC_T, NPC),
                               NPC, XF_WORDWIDTH(SRC_T, NPC), XF_32UW, (COLS >> XF_BITSHIFT(NPC)), XF_16UW, XF_32UW,
                               USE_URAM>(src, dst, img_height, img_width, NMSRADIUS, threshold, k);
    }
}
} // namespace cv
} // namespace xf

#endif // _XF_HARRIS_HPP_
