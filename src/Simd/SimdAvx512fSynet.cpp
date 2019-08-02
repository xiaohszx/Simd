/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2019 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdMemory.h"
#include "Simd/SimdStore.h"
#include "Simd/SimdExtract.h"
#include "Simd/SimdSynet.h"
#include "Simd/SimdPow.h"
#include "Simd/SimdExp.h"
#include "Simd/SimdBase.h"
#include "Simd/SimdSse1.h"
#include "Simd/SimdAvx1.h"
#include "Simd/SimdAvx2.h"
#include "Simd/SimdAvx512f.h"
#include "Simd/SimdArray.h"

namespace Simd
{
#ifdef SIMD_AVX512F_ENABLE    
    namespace Avx512f
    {
        template <bool align, bool mask> SIMD_INLINE void SynetAddBias(const __m512 & bias, float * dst, __mmask16 tail = -1)
        {
            Store<align, mask>(dst, _mm512_add_ps((Load<align, mask>(dst, tail)), bias), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetAddBias(const float * bias, float * dst, __mmask16 tail = -1)
        {
            __m512 _bias = Load<align, mask>(bias, tail);
            __m512 _dst = Load<align, mask>(dst, tail);
            Store<align, mask>(dst, _mm512_add_ps(_dst, _bias), tail);
        }

        template <bool align> void SynetAddBiasNchw(const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(spatial) && Aligned(dst));

            size_t aligned = AlignLo(spatial, QF);
            size_t partial = AlignLo(spatial, F);
            __mmask16 tail = TailMask16(spatial - partial);
            for (size_t c = 0; c < channels; ++c)
            {
                size_t s = 0;
                __m512 _bias = _mm512_set1_ps(bias[c]);
                for (; s < aligned; s += QF)
                {
                    SynetAddBias<align, false>(_bias, dst + s + F * 0);
                    SynetAddBias<align, false>(_bias, dst + s + F * 1);
                    SynetAddBias<align, false>(_bias, dst + s + F * 2);
                    SynetAddBias<align, false>(_bias, dst + s + F * 3);
                }
                for (; s < partial; s += F)
                    SynetAddBias<align, false>(_bias, dst + s);
                if (s < spatial)
                    SynetAddBias<align, true>(_bias, dst + s, tail);
                dst += spatial;
            }
        }

        SIMD_INLINE void SynetAddBiasNchw(const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(spatial) && Aligned(dst))
                SynetAddBiasNchw<true>(bias, channels, spatial, dst);
            else
                SynetAddBiasNchw<false>(bias, channels, spatial, dst);
        }

        template <bool align> void SynetAddBiasNhwc(const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(channels) && Aligned(bias) && Aligned(dst));

            size_t aligned = AlignLo(channels, QF);
            size_t partial = AlignLo(channels, F);
            __mmask16 tail = TailMask16(channels - partial);
            for (size_t s = 0; s < spatial; ++s)
            {
                size_t c = 0;
                for (; c < aligned; c += QF)
                {
                    SynetAddBias<align, false>(bias + c + F * 0, dst + c + F * 0);
                    SynetAddBias<align, false>(bias + c + F * 1, dst + c + F * 1);
                    SynetAddBias<align, false>(bias + c + F * 2, dst + c + F * 2);
                    SynetAddBias<align, false>(bias + c + F * 3, dst + c + F * 3);
                }
                for (; c < partial; c += F)
                    SynetAddBias<align, false>(bias + c, dst + c);
                if (c < channels)
                    SynetAddBias<align, true>(bias + c, dst + c, tail);
                dst += channels;
            }
        }

        SIMD_INLINE void SynetAddBiasNhwc(const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(bias) && Aligned(channels) && Aligned(dst))
                SynetAddBiasNhwc<true>(bias, channels, spatial, dst);
            else
                SynetAddBiasNhwc<false>(bias, channels, spatial, dst);
        }

        template <bool align> void SynetAddBiasNchw16c(const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(dst));

            size_t spatial4 = AlignLo(spatial, 4);
            for (size_t c = 0; c < channels; c += F)
            {
                __m512 _bias = Load<false>(bias + c);
                size_t s = 0;
                for (; s < spatial4; s += 4, dst += 4 * F)
                {
                    SynetAddBias<align, false>(_bias, dst + 0 * F);
                    SynetAddBias<align, false>(_bias, dst + 1 * F);
                    SynetAddBias<align, false>(_bias, dst + 2 * F);
                    SynetAddBias<align, false>(_bias, dst + 3 * F);
                }
                for (; s < spatial; ++s, dst += F)
                    SynetAddBias<align, false>(_bias, dst);
            }
        }

        SIMD_INLINE void SynetAddBiasNchw16c(const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(dst))
                SynetAddBiasNchw16c<true>(bias, channels, spatial, dst);
            else
                SynetAddBiasNchw16c<false>(bias, channels, spatial, dst);
        }

        void SynetAddBias(const float * bias, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format)
        {
            if (Base::NchwCompatible(channels, spatial, format))
                SynetAddBiasNchw(bias, channels, spatial, dst);
            else if (Base::NhwcCompatible(channels, spatial, format))
                SynetAddBiasNhwc(bias, channels, spatial, dst);
            else if (format == SimdTensorFormatNchw4c)
                Sse::SynetAddBias(bias, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw8c)
                Avx::SynetAddBias(bias, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw16c)
                SynetAddBiasNchw16c(bias, channels, spatial, dst);
            else
                Base::SynetAddBias(bias, channels, spatial, dst, format);
        }

        //---------------------------------------------------------------------

        template <SimdSynetEltwiseOperationType type> __m512 SynetEltwiseLayerForward(__m512 src0, __m512 src1);

        template <> SIMD_INLINE __m512 SynetEltwiseLayerForward<SimdSynetEltwiseOperationProduct>(__m512 src0, __m512 src1)
        {
            return _mm512_mul_ps(src0, src1);
        }

        template <> SIMD_INLINE __m512 SynetEltwiseLayerForward<SimdSynetEltwiseOperationMax>(__m512 src0, __m512 src1)
        {
            return _mm512_max_ps(src0, src1);
        }

        template <> SIMD_INLINE __m512 SynetEltwiseLayerForward<SimdSynetEltwiseOperationMin>(__m512 src0, __m512 src1)
        {
            return _mm512_min_ps(src0, src1);
        }

        template <SimdSynetEltwiseOperationType type, bool align, bool mask > SIMD_INLINE void SynetEltwiseLayerForward(const float * src0, const float * src1, float * dst, size_t offset, __mmask16 tail = -1)
        {
            Store<align, mask>(dst + offset, SynetEltwiseLayerForward<type>((Load<align, mask>(src0 + offset, tail)), (Load<align, mask>(src1 + offset, tail))), tail);
        }

        template <SimdSynetEltwiseOperationType type, bool align> void SynetEltwiseLayerForward(float const * const * src, size_t count, size_t size, float * dst)
        {
            size_t aligned = AlignLo(size, QF);
            size_t partial = AlignLo(size, F);
            __mmask16 tail = __mmask16(-1) >> (F + partial - size);
            const float * src0 = src[0];
            const float * src1 = src[1];
            size_t j = 0;
            for (; j < aligned; j += QF)
            {
                SynetEltwiseLayerForward<type, align, false>(src0, src1, dst, j + F * 0);
                SynetEltwiseLayerForward<type, align, false>(src0, src1, dst, j + F * 1);
                SynetEltwiseLayerForward<type, align, false>(src0, src1, dst, j + F * 2);
                SynetEltwiseLayerForward<type, align, false>(src0, src1, dst, j + F * 3);
            }
            for (; j < partial; j += F)
                SynetEltwiseLayerForward<type, align, false>(src0, src1, dst, j);
            if (j < size)
                SynetEltwiseLayerForward<type, align, true>(src0, src1, dst, j, tail);
            for (size_t i = 2; i < count; ++i)
            {
                const float * srci = src[i];
                for (j = 0; j < aligned; j += QF)
                {
                    SynetEltwiseLayerForward<type, align, false>(dst, srci, dst, j + F * 0);
                    SynetEltwiseLayerForward<type, align, false>(dst, srci, dst, j + F * 1);
                    SynetEltwiseLayerForward<type, align, false>(dst, srci, dst, j + F * 2);
                    SynetEltwiseLayerForward<type, align, false>(dst, srci, dst, j + F * 3);
                }
                for (; j < partial; j += F)
                    SynetEltwiseLayerForward<type, align, false>(dst, srci, dst, j);
                if (j < size)
                    SynetEltwiseLayerForward<type, align, true>(dst, srci, dst, j, tail);
            }
        }

        template <bool align, bool mask> void SynetEltwiseLayerForwardSum(const float * src0, const __m512 & weight0, const float * src1, const __m512 & weight1, float * dst, size_t offset, __mmask16 tail = -1)
        {
            Store<align, mask>(dst + offset, _mm512_fmadd_ps((Load<align, mask>(src0 + offset, tail)), weight0, _mm512_mul_ps((Load<align, mask>(src1 + offset, tail)), weight1)), tail);
        }

        template <bool align, bool mask> void SynetEltwiseLayerForwardSum(const float * src, const __m512 & weight, float * dst, size_t offset, __mmask16 tail = -1)
        {
            Store<align, mask>(dst + offset, _mm512_fmadd_ps((Load<align, mask>(src + offset, tail)), weight, (Load<align, mask>(dst + offset, tail))), tail);
        }

        template <bool align> void SynetEltwiseLayerForwardSum(float const * const * src, const float * weight, size_t count, size_t size, float * dst)
        {
            size_t aligned = AlignLo(size, QF);
            size_t partial = AlignLo(size, F);
            __mmask16 tail = __mmask16(-1) >> (F + partial - size);
            const float * src0 = src[0];
            const float * src1 = src[1];
            __m512 weight0 = _mm512_set1_ps(weight[0]);
            __m512 weight1 = _mm512_set1_ps(weight[1]);
            size_t j = 0;
            for (; j < aligned; j += QF)
            {
                SynetEltwiseLayerForwardSum<align, false>(src0, weight0, src1, weight1, dst, j + F * 0);
                SynetEltwiseLayerForwardSum<align, false>(src0, weight0, src1, weight1, dst, j + F * 1);
                SynetEltwiseLayerForwardSum<align, false>(src0, weight0, src1, weight1, dst, j + F * 2);
                SynetEltwiseLayerForwardSum<align, false>(src0, weight0, src1, weight1, dst, j + F * 3);
            }
            for (; j < partial; j += F)
                SynetEltwiseLayerForwardSum<align, false>(src0, weight0, src1, weight1, dst, j);
            if (j < size)
                SynetEltwiseLayerForwardSum<align, true>(src0, weight0, src1, weight1, dst, j, tail);
            for (size_t i = 2; i < count; ++i)
            {
                const float * srci = src[i];
                __m512 weighti = _mm512_set1_ps(weight[i]);
                for (j = 0; j < aligned; j += QF)
                {
                    SynetEltwiseLayerForwardSum<align, false>(srci, weighti, dst, j + F * 0);
                    SynetEltwiseLayerForwardSum<align, false>(srci, weighti, dst, j + F * 1);
                    SynetEltwiseLayerForwardSum<align, false>(srci, weighti, dst, j + F * 2);
                    SynetEltwiseLayerForwardSum<align, false>(srci, weighti, dst, j + F * 3);
                }
                for (; j < partial; j += F)
                    SynetEltwiseLayerForwardSum<align, false>(srci, weighti, dst, j);
                if (j < size)
                    SynetEltwiseLayerForwardSum<align, true>(srci, weighti, dst, j, tail);
            }
        }

        template <bool align> void SynetEltwiseLayerForward(float const * const * src, const float * weight, size_t count, size_t size, SimdSynetEltwiseOperationType type, float * dst)
        {
            switch (type)
            {
            case SimdSynetEltwiseOperationProduct:
                SynetEltwiseLayerForward<SimdSynetEltwiseOperationProduct, align>(src, count, size, dst);
                break;
            case SimdSynetEltwiseOperationSum:
                SynetEltwiseLayerForwardSum<align>(src, weight, count, size, dst);
                break;
            case SimdSynetEltwiseOperationMax:
                SynetEltwiseLayerForward<SimdSynetEltwiseOperationMax, align>(src, count, size, dst);
                break;
            case SimdSynetEltwiseOperationMin:
                SynetEltwiseLayerForward<SimdSynetEltwiseOperationMin, align>(src, count, size, dst);
                break;
            default:
                assert(0);
            }
        }

        void SynetEltwiseLayerForward(float const * const * src, const float * weight, size_t count, size_t size, SimdSynetEltwiseOperationType type, float * dst)
        {
            assert(count >= 2);
            bool aligned = Aligned(dst) && Aligned(src[0]) && Aligned(src[1]);
            for (size_t i = 2; i < count; ++i)
                aligned = aligned && Aligned(src[i]);
            if (aligned)
                SynetEltwiseLayerForward<true>(src, weight, count, size, type, dst);
            else
                SynetEltwiseLayerForward<false>(src, weight, count, size, type, dst);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward0(const float * src, const float * bias, const float * scale, __m512 sign, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), _bias);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            Store<align, mask>(dst + offset, _mm512_add_ps(_mm512_mul_ps(_mm512_sub_ps(x, _mm512_andnot_ps(sign, x)), _scale), _mm512_max_ps(_mm512_setzero_ps(), x)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward0(const float * src, __m512 bias, __m512 scale, __m512 sign, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), bias);
            Store<align, mask>(dst + offset, _mm512_add_ps(_mm512_mul_ps(_mm512_sub_ps(x, _mm512_andnot_ps(sign, x)), scale), _mm512_max_ps(_mm512_setzero_ps(), x)), tail);
        }

        template <bool align> void SynetFusedLayerForward0(const float * src, const float * bias, const float * scale, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst));
            __m512 sign = _mm512_set1_ps(-0.0f);
            if ((trans || size == 1) && count != 1)
            {
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - count);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    for (; i < aligned; i += QF)
                    {
                        SynetFusedLayerForward0<align, false>(src, bias, scale, sign, dst, i + 0 * F);
                        SynetFusedLayerForward0<align, false>(src, bias, scale, sign, dst, i + 1 * F);
                        SynetFusedLayerForward0<align, false>(src, bias, scale, sign, dst, i + 2 * F);
                        SynetFusedLayerForward0<align, false>(src, bias, scale, sign, dst, i + 3 * F);
                    }
                    for (; i < partial; i += F)
                        SynetFusedLayerForward0<align, false>(src, bias, scale, sign, dst, i);
                    if (i < count)
                        SynetFusedLayerForward0<align, true>(src, bias, scale, sign, dst, i, tail);
                    src += count;
                    dst += count;
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - size);
                for (size_t i = 0; i < count; ++i)
                {
                    size_t j = 0;
                    __m512 _bias = _mm512_set1_ps(bias[i]);
                    __m512 _scale = _mm512_set1_ps(scale[i]);
                    for (; j < aligned; j += QF)
                    {
                        SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, j + 0 * F);
                        SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, j + 1 * F);
                        SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, j + 2 * F);
                        SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, j + 3 * F);
                    }
                    for (; j < partial; j += F)
                        SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, j);
                    if(j < size)
                        SynetFusedLayerForward0<align, true>(src, _bias, _scale, sign, dst, j, tail);
                    src += size;
                    dst += size;
                }
            }
        }

        void SynetFusedLayerForward0(const float * src, const float * bias, const float * scale, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst))
                SynetFusedLayerForward0<true>(src, bias, scale, count, size, dst, trans);
            else
                SynetFusedLayerForward0<false>(src, bias, scale, count, size, dst, trans);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward1(const float * src, const float * bias0, const float * scale1, const float * bias1, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _bias0 = Load<align, mask>(bias0 + offset, tail);
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), _bias0);
            __m512 _scale1 = Load<align, mask>(scale1 + offset, tail);
            __m512 _bias1 = Load<align, mask>(bias1 + offset, tail);
            Store<align, mask>(dst + offset, _mm512_add_ps(_mm512_fmadd_ps(_mm512_max_ps(_mm512_setzero_ps(), _mm512_sub_ps(_mm512_setzero_ps(), x)), _scale1, _bias1), _mm512_max_ps(_mm512_setzero_ps(), x)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward1(const float * src, __m512 bias0, __m512 scale1, __m512 bias1, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), bias0);
            Store<align, mask>(dst + offset, _mm512_add_ps(_mm512_fmadd_ps(_mm512_max_ps(_mm512_setzero_ps(), _mm512_sub_ps(_mm512_setzero_ps(), x)), scale1, bias1), _mm512_max_ps(_mm512_setzero_ps(), x)), tail);
        }

        template <bool align> void SynetFusedLayerForward1(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(bias0) && Aligned(scale1) && Aligned(bias1) : Aligned(size)) && Aligned(src) && Aligned(dst));
            if ((trans || size == 1) && count != 1)
            {
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - count);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    for (; i < aligned; i += QF)
                    {
                        SynetFusedLayerForward1<align, false>(src, bias0, scale1, bias1, dst, i + 0 * F);
                        SynetFusedLayerForward1<align, false>(src, bias0, scale1, bias1, dst, i + 1 * F);
                        SynetFusedLayerForward1<align, false>(src, bias0, scale1, bias1, dst, i + 2 * F);
                        SynetFusedLayerForward1<align, false>(src, bias0, scale1, bias1, dst, i + 3 * F);
                    }
                    for (; i < partial; i += F)
                        SynetFusedLayerForward1<align, false>(src, bias0, scale1, bias1, dst, i);
                    if (i < count)
                        SynetFusedLayerForward1<align, true>(src, bias0, scale1, bias1, dst, i, tail);
                    src += count;
                    dst += count;
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - size);
                for (size_t i = 0; i < count; ++i)
                {
                    size_t j = 0;
                    __m512 _bias0 = _mm512_set1_ps(bias0[i]);
                    __m512 _scale1 = _mm512_set1_ps(scale1[i]);
                    __m512 _bias1 = _mm512_set1_ps(bias1[i]);
                    for (; j < aligned; j += QF)
                    {
                        SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, j + 0 * F);
                        SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, j + 1 * F);
                        SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, j + 2 * F);
                        SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, j + 3 * F);
                    }
                    for (; j < partial; j += F)
                        SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, j);
                    if(j < size)
                        SynetFusedLayerForward1<align, true>(src, _bias0, _scale1, _bias1, dst, j, tail);
                    src += size;
                    dst += size;
                }
            }
        }

        void SynetFusedLayerForward1(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(bias0) && Aligned(scale1) && Aligned(bias1) : Aligned(size)) && Aligned(src) && Aligned(dst))
                SynetFusedLayerForward1<true>(src, bias0, scale1, bias1, count, size, dst, trans);
            else
                SynetFusedLayerForward1<false>(src, bias0, scale1, bias1, count, size, dst, trans);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward2(const float * src, const float * scale, const float * bias, __m512 slope, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            __m512 x = _mm512_fmadd_ps(_src, _scale, _bias);
            __m512 _dst = _mm512_add_ps(_mm512_max_ps(_mm512_setzero_ps(), x), _mm512_mul_ps(_mm512_min_ps(_mm512_setzero_ps(), x), slope));
            Store<align, mask>(dst + offset, _dst, tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward2(const float * src, __m512 scale, __m512 bias, __m512 slope, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 x = _mm512_fmadd_ps(_src, scale, bias);
            __m512 _dst = _mm512_add_ps(_mm512_max_ps(_mm512_setzero_ps(), x), _mm512_mul_ps(_mm512_min_ps(_mm512_setzero_ps(), x), slope));
            Store<align, mask>(dst + offset, _dst, tail);
        }

        template <bool align> void SynetFusedLayerForward2(const float * src, const float * scale, const float * bias, size_t count, size_t size, const float * slope, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst));
            __m512 _slope = _mm512_set1_ps(slope[0]);
            if ((trans || size == 1) && count != 1)
            {
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - count);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    for (; i < aligned; i += QF)
                    {
                        SynetFusedLayerForward2<align, false>(src, scale, bias, _slope, dst, i + 0 * F);
                        SynetFusedLayerForward2<align, false>(src, scale, bias, _slope, dst, i + 1 * F);
                        SynetFusedLayerForward2<align, false>(src, scale, bias, _slope, dst, i + 2 * F);
                        SynetFusedLayerForward2<align, false>(src, scale, bias, _slope, dst, i + 3 * F);
                    }
                    for (; i < partial; i += F)
                        SynetFusedLayerForward2<align, false>(src, scale, bias, _slope, dst, i);
                    if (i < count)
                        SynetFusedLayerForward2<align, true>(src, scale, bias, _slope, dst, i, tail);
                    src += count;
                    dst += count;
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - size);
                for (size_t i = 0; i < count; ++i)
                {
                    size_t j = 0;
                    __m512 _scale = _mm512_set1_ps(scale[i]);
                    __m512 _bias = _mm512_set1_ps(bias[i]);
                    for (; j < aligned; j += QF)
                    {
                        SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, j + 0 * F);
                        SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, j + 1 * F);
                        SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, j + 2 * F);
                        SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, j + 3 * F);
                    }
                    for (; j < partial; j += F)
                        SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, j);
                    if( j < size)
                        SynetFusedLayerForward2<align, true>(src, _scale, _bias, _slope, dst, j, tail);
                    src += size;
                    dst += size;
                }
            }
        }

        void SynetFusedLayerForward2(const float * src, const float * scale, const float * bias, size_t count, size_t size, const float * slope, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst))
                SynetFusedLayerForward2<true>(src, scale, bias, count, size, slope, dst, trans);
            else
                SynetFusedLayerForward2<false>(src, scale, bias, count, size, slope, dst, trans);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward3(const float * src, const float * bias, const float * scale, __m512 sign, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), _bias);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            __m512 pos = _mm512_max_ps(_mm512_setzero_ps(), x);
            __m512 neg = _mm512_min_ps(_mm512_setzero_ps(), x);
            Store<align, mask>(dst + offset, _mm512_add_ps(pos, _mm512_mul_ps(_scale, neg)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward3(const float * src, __m512 bias, __m512 scale, __m512 sign, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), bias);
            __m512 pos = _mm512_max_ps(_mm512_setzero_ps(), x);
            __m512 neg = _mm512_min_ps(_mm512_setzero_ps(), x);
            Store<align, mask>(dst + offset, _mm512_add_ps(pos, _mm512_mul_ps(scale, neg)), tail);
        }

        template <bool align> void SynetFusedLayerForward3(const float * src, const float * bias, const float * scale, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst));
            __m512 sign = _mm512_set1_ps(-0.0f);
            if ((trans || size == 1) && count != 1)
            {
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - count);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    for (; i < aligned; i += QF)
                    {
                        SynetFusedLayerForward3<align, false>(src, bias, scale, sign, dst, i + 0 * F);
                        SynetFusedLayerForward3<align, false>(src, bias, scale, sign, dst, i + 1 * F);
                        SynetFusedLayerForward3<align, false>(src, bias, scale, sign, dst, i + 2 * F);
                        SynetFusedLayerForward3<align, false>(src, bias, scale, sign, dst, i + 3 * F);
                    }
                    for (; i < partial; i += F)
                        SynetFusedLayerForward3<align, false>(src, bias, scale, sign, dst, i);
                    if (i < count)
                        SynetFusedLayerForward3<align, true>(src, bias, scale, sign, dst, i, tail);
                    src += count;
                    dst += count;
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - size);
                for (size_t i = 0; i < count; ++i)
                {
                    size_t j = 0;
                    __m512 _bias = _mm512_set1_ps(bias[i]);
                    __m512 _scale = _mm512_set1_ps(scale[i]);
                    for (; j < aligned; j += QF)
                    {
                        SynetFusedLayerForward3<align, false>(src, _bias, _scale, sign, dst, j + 0 * F);
                        SynetFusedLayerForward3<align, false>(src, _bias, _scale, sign, dst, j + 1 * F);
                        SynetFusedLayerForward3<align, false>(src, _bias, _scale, sign, dst, j + 2 * F);
                        SynetFusedLayerForward3<align, false>(src, _bias, _scale, sign, dst, j + 3 * F);
                    }
                    for (; j < partial; j += F)
                        SynetFusedLayerForward3<align, false>(src, _bias, _scale, sign, dst, j);
                    if (j < size)
                        SynetFusedLayerForward3<align, true>(src, _bias, _scale, sign, dst, j, tail);
                    src += size;
                    dst += size;
                }
            }
        }

        void SynetFusedLayerForward3(const float * src, const float * bias, const float * scale, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst))
                SynetFusedLayerForward3<true>(src, bias, scale, count, size, dst, trans);
            else
                SynetFusedLayerForward3<false>(src, bias, scale, count, size, dst, trans);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward4(const float * src, const float * bias0, __m512 scale1, __m512 bias1, float * dst0, float * dst1, size_t offset, __mmask16 tail = -1)
        {
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), (Load<align, mask>(bias0 + offset, tail)));
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), x), tail);
            Store<align, mask>(dst1 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(x, scale1, bias1)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward4(const float * src, __m512 bias0, __m512 scale1, __m512 bias1, float * dst0, float * dst1, size_t offset, __mmask16 tail = -1)
        {
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), bias0);
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), x), tail);
            Store<align, mask>(dst1 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(x, scale1, bias1)), tail);
        }

        template<bool align> void SynetFusedLayerForward4(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(bias0) : Aligned(size)) && Aligned(src) && Aligned(dst));
            __m512 _scale1 = _mm512_set1_ps(scale1[0]);
            __m512 _bias1 = _mm512_set1_ps(bias1[0]);
            if ((trans || size == 1) && count != 1)
            {
                float * dst0 = dst, *dst1 = dst + count;
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - count);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    for (; i < aligned; i += QF)
                    {
                        SynetFusedLayerForward4<align, false>(src, bias0, _scale1, _bias1, dst0, dst1, i + 0 * F);
                        SynetFusedLayerForward4<align, false>(src, bias0, _scale1, _bias1, dst0, dst1, i + 1 * F);
                        SynetFusedLayerForward4<align, false>(src, bias0, _scale1, _bias1, dst0, dst1, i + 2 * F);
                        SynetFusedLayerForward4<align, false>(src, bias0, _scale1, _bias1, dst0, dst1, i + 3 * F);
                    }
                    for (; i < partial; i += F)
                        SynetFusedLayerForward4<align, false>(src, bias0, _scale1, _bias1, dst0, dst1, i);
                    if(i < count)
                        SynetFusedLayerForward4<align, true>(src, bias0, _scale1, _bias1, dst0, dst1, i, tail);
                    src += count;
                    dst0 += 2 * count;
                    dst1 += 2 * count;
                }
            }
            else
            {
                float * dst0 = dst, *dst1 = dst + count * size;
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - size);
                for (size_t i = 0; i < count; ++i)
                {
                    size_t j = 0;
                    __m512 _bias0 = _mm512_set1_ps(bias0[i]);
                    for (; j < aligned; j += QF)
                    {
                        SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, j + 0 * F);
                        SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, j + 1 * F);
                        SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, j + 2 * F);
                        SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, j + 3 * F);
                    }
                    for (; j < partial; j += F)
                        SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, j);
                    if (j < size)
                        SynetFusedLayerForward4<align, true>(src, _bias0, _scale1, _bias1, dst0, dst1, j, tail);
                    src += size;
                    dst0 += size;
                    dst1 += size;
                }
            }
        }

        void SynetFusedLayerForward4(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(bias0) : Aligned(size)) && Aligned(src) && Aligned(dst))
                SynetFusedLayerForward4<true>(src, bias0, scale1, bias1, count, size, dst, trans);
            else
                SynetFusedLayerForward4<false>(src, bias0, scale1, bias1, count, size, dst, trans);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward8(const float * src0, const float * src1, const float * src2, float * dst, size_t offset, __mmask16 tail = -1)
        {
            Store<align, mask>(dst + offset, _mm512_add_ps((Load<align, mask>(src0 + offset, tail)), 
                _mm512_mul_ps((Load<align, mask>(src1 + offset, tail)), (Load<align, mask>(src2 + offset, tail)))), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward8(const float * src0, const float * src1, const __m512 & src2, float * dst, size_t offset, __mmask16 tail = -1)
        {
            Store<align, mask>(dst + offset, _mm512_add_ps((Load<align, mask>(src0 + offset, tail)), 
                _mm512_mul_ps((Load<align, mask>(src1 + offset, tail)), src2)), tail);
        }

        template <bool align> void SynetFusedLayerForward8(const float * src0, const float * src1, const float * src2, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(src2) : Aligned(size)) && Aligned(src0) && Aligned(src1) && Aligned(dst));
            if ((trans || size == 1) && count != 1)
            {
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                __mmask16 tail = TailMask16(count - partial);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    for (; i < aligned; i += QF)
                    {
                        SynetFusedLayerForward8<align, false>(src0, src1, src2, dst, i + 0 * F);
                        SynetFusedLayerForward8<align, false>(src0, src1, src2, dst, i + 1 * F);
                        SynetFusedLayerForward8<align, false>(src0, src1, src2, dst, i + 2 * F);
                        SynetFusedLayerForward8<align, false>(src0, src1, src2, dst, i + 3 * F);
                    }
                    for (; i < partial; i += F)
                        SynetFusedLayerForward8<align, false>(src0, src1, src2, dst, i);
                    if (i < count) 
                        SynetFusedLayerForward8<align, true>(src0, src1, src2, dst, i, tail);
                    src0 += count;
                    src1 += count;
                    dst += count;
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                __mmask16 tail = TailMask16(size - partial);
                for (size_t i = 0; i < count; ++i)
                {
                    __m512 _src2 = _mm512_set1_ps(src2[i]);
                    size_t j = 0;
                    for (; j < aligned; j += QF)
                    {
                        SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, j + 0 * F);
                        SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, j + 1 * F);
                        SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, j + 2 * F);
                        SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, j + 3 * F);
                    }
                    for (; j < partial; j += F)
                        SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, j);
                    if (j < size)
                        SynetFusedLayerForward8<align, true>(src0, src1, _src2, dst, j, tail);
                    src0 += size;
                    src1 += size;
                    dst += size;
                }
            }
        }

        void SynetFusedLayerForward8(const float * src0, const float * src1, const float * src2, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(src2) : Aligned(size)) && Aligned(src0) && Aligned(src1) && Aligned(dst))
                SynetFusedLayerForward8<true>(src0, src1, src2, count, size, dst, trans);
            else
                SynetFusedLayerForward8<false>(src0, src1, src2, count, size, dst, trans);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const float * scale, const float * bias, float * dst0, float * dst1, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(_src, _scale, _bias)), tail);
            Store<align, mask>(dst1 + offset, _src, tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const float * scale, const float * bias, float * dst0, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(_src, _scale, _bias)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const __m512 & scale, const __m512 & bias, float * dst0, float * dst1, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(_src, scale, bias)), tail);
            Store<align, mask>(dst1 + offset, _src, tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const __m512 & scale, const __m512 & bias, float * dst0, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(_src, scale, bias)), tail);
        }

        template<bool align> void SynetFusedLayerForward9(const float * src0, const float * src1, const float * scale0, const float * bias0, size_t count0, size_t count1, size_t size, float * dst0, float * dst1, SimdBool trans)
        {
            if (align)
                assert((trans || size == 1 ? Aligned(count0) && Aligned(count1) && Aligned(scale0) && Aligned(bias0) : Aligned(size)) && Aligned(src0) && Aligned(src1) && Aligned(dst0) && Aligned(dst1));
            const float * scale1 = scale0 + count0;
            const float * bias1 = bias0 + count0;
            if (trans || size == 1)
            {
                size_t aligned0 = AlignLo(count0, QF);
                size_t partial0 = AlignLo(count0, F);
                __mmask16 tail0 = TailMask16(count0 - partial0);
                size_t aligned1 = AlignLo(count1, QF);
                size_t partial1 = AlignLo(count1, F);
                __mmask16 tail1 = TailMask16(count1 - partial1);
                if (dst1)
                {
                    for (size_t j = 0; j < size; ++j)
                    {
                        size_t i = 0;
                        for (; i < aligned0; i += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, dst1, i + 0 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, dst1, i + 1 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, dst1, i + 2 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, dst1, i + 3 * F);
                        }
                        for (; i < partial0; i += F)
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, dst1, i);
                        if(i < count0)
                            SynetFusedLayerForward9<align, true>(src0, scale0, bias0, dst0, dst1, i, tail0);
                        src0 += count0;
                        dst0 += count0;
                        dst1 += count0;
                        i = 0;
                        for (; i < aligned1; i += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, dst1, i + 0 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, dst1, i + 1 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, dst1, i + 2 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, dst1, i + 3 * F);
                        }
                        for (; i < partial1; i += F)
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, dst1, i);
                        if (i < count1)
                            SynetFusedLayerForward9<align, true>(src1, scale1, bias1, dst0, dst1, i, tail1);
                        src1 += count1;
                        dst0 += count1;
                        dst1 += count1;
                    }
                }
                else
                {
                    for (size_t j = 0; j < size; ++j)
                    {
                        size_t i = 0;
                        for (; i < aligned0; i += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, i + 0 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, i + 1 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, i + 2 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, i + 3 * F);
                        }
                        for (; i < partial0; i += F)
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, i);
                        if (i < count0)
                            SynetFusedLayerForward9<align, true>(src0, scale0, bias0, dst0, i, tail0);
                        src0 += count0;
                        dst0 += count0;
                        i = 0;
                        for (; i < aligned1; i += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, i + 0 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, i + 1 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, i + 2 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, i + 3 * F);
                        }
                        for (; i < partial1; i += F)
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, i);
                        if (i < count1)
                            SynetFusedLayerForward9<align, true>(src1, scale1, bias1, dst0, i, tail1);
                        src1 += count1;
                        dst0 += count1;
                    }
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                __mmask16 tail = TailMask16(size - partial);
                if (dst1)
                {
                    for (size_t i = 0; i < count0; ++i, src0 += size, dst0 += size, dst1 += size)
                    {
                        size_t j = 0;
                        __m512 _scale0 = _mm512_set1_ps(scale0[i]);
                        __m512 _bias0 = _mm512_set1_ps(bias0[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, dst1, j + 0 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, dst1, j + 1 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, dst1, j + 2 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, dst1, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, dst1, j);
                        if (j < size)
                            SynetFusedLayerForward9<align, true>(src0, _scale0, _bias0, dst0, dst1, j, tail);
                    }
                    for (size_t i = 0; i < count1; ++i, src1 += size, dst0 += size, dst1 += size)
                    {
                        size_t j = 0;
                        __m512 _scale1 = _mm512_set1_ps(scale1[i]);
                        __m512 _bias1 = _mm512_set1_ps(bias1[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, dst1, j + 0 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, dst1, j + 1 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, dst1, j + 2 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, dst1, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, dst1, j);
                        if (j < size)
                            SynetFusedLayerForward9<align, true>(src1, _scale1, _bias1, dst0, dst1, j, tail);
                    }
                }
                else
                {
                    for (size_t i = 0; i < count0; ++i, src0 += size, dst0 += size)
                    {
                        size_t j = 0;
                        __m512 _scale0 = _mm512_set1_ps(scale0[i]);
                        __m512 _bias0 = _mm512_set1_ps(bias0[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, j + 0 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, j + 1 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, j + 2 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, j);
                        if (j < size)
                            SynetFusedLayerForward9<align, true>(src0, _scale0, _bias0, dst0, j, tail);
                    }
                    for (size_t i = 0; i < count1; ++i, src1 += size, dst0 += size)
                    {
                        size_t j = 0;
                        __m512 _scale1 = _mm512_set1_ps(scale1[i]);
                        __m512 _bias1 = _mm512_set1_ps(bias1[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, j + 0 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, j + 1 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, j + 2 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, j);
                        if (j < size)
                            SynetFusedLayerForward9<align, true>(src1, _scale1, _bias1, dst0, j, tail);
                    }
                }
            }
        }

        void SynetFusedLayerForward9(const float * src0, const float * src1, const float * scale0, const float * bias0, size_t count0, size_t count1, size_t size, float * dst0, float * dst1, SimdBool trans)
        {
            if ((trans || size == 1 ? Aligned(count0) && Aligned(count1) && Aligned(scale0) && Aligned(bias0) : Aligned(size)) && Aligned(src0) && Aligned(src1) && Aligned(dst0) && Aligned(dst1))
                SynetFusedLayerForward9<true>(src0, src1, scale0, bias0, count0, count1, size, dst0, dst1, trans);
            else
                SynetFusedLayerForward9<false>(src0, src1, scale0, bias0, count0, count1, size, dst0, dst1, trans);
        }

        void SynetInnerProductLayerForward1(const float * S0, const float * W, const float * B, size_t K, float * D)
        {
            size_t K16 = K & (~15);
            size_t K64 = K & (~63);
            const float * W0 = W + 0 * K;
            __m512 d00, d01, d02, d03;
            __m512 s0, s1, s2, s3, w0, w1, w2, w3;
            size_t k = 0;
            d00 = _mm512_setzero_ps();
            if (K64)
            {
                d01 = _mm512_setzero_ps();
                d02 = _mm512_setzero_ps();
                d03 = _mm512_setzero_ps();
                for (; k < K64; k += 64)
                {
                    s0 = _mm512_loadu_ps(S0 + k + 0 * F);
                    s1 = _mm512_loadu_ps(S0 + k + 1 * F);
                    w0 = _mm512_loadu_ps(W0 + k + 0 * F);
                    w1 = _mm512_loadu_ps(W0 + k + 1 * F);
                    d00 = _mm512_fmadd_ps(s0, w0, d00);
                    d01 = _mm512_fmadd_ps(s1, w1, d01);
                    s2 = _mm512_loadu_ps(S0 + k + 2 * F);
                    s3 = _mm512_loadu_ps(S0 + k + 3 * F);
                    w2 = _mm512_loadu_ps(W0 + k + 2 * F);
                    w3 = _mm512_loadu_ps(W0 + k + 3 * F);
                    d02 = _mm512_fmadd_ps(s2, w2, d02);
                    d03 = _mm512_fmadd_ps(s3, w3, d03);
                }
                d00 = _mm512_add_ps(_mm512_add_ps(d00, d01), _mm512_add_ps(d02, d03));
            }
            for (; k < K16; k += 16)
            {
                s0 = _mm512_loadu_ps(S0 + k);
                w0 = _mm512_loadu_ps(W0 + k);
                d00 = _mm512_fmadd_ps(s0, w0, d00);
            }
            if (k < K)
            {
                __mmask16 tail = __mmask16(-1) >> (16 + k - K);
                s0 = _mm512_maskz_loadu_ps(tail, S0 + k);
                w0 = _mm512_maskz_loadu_ps(tail, W0 + k);
                d00 = _mm512_fmadd_ps(s0, w0, d00);
            }
            D[0] = Avx512f::ExtractSum(d00) + B[0];
        }

        void SynetInnerProductLayerForward4(const float * S0, const float * W, const float * B, size_t K, float * D)
        {
            size_t K16 = K & (~15);
            size_t K32 = K & (~31);
            const float * W0 = W + 0 * K;
            const float * W1 = W + 1 * K;
            const float * W2 = W + 2 * K;
            const float * W3 = W + 3 * K;
            __m512 d00, d01, d10, d11, d20, d21, d30, d31;
            __m512 s0, s1, w0, w1;
            size_t k = 0;
            d00 = _mm512_setzero_ps();
            d10 = _mm512_setzero_ps();
            d20 = _mm512_setzero_ps();
            d30 = _mm512_setzero_ps();
            if (K32)
            {
                d01 = _mm512_setzero_ps();
                d11 = _mm512_setzero_ps();
                d21 = _mm512_setzero_ps();
                d31 = _mm512_setzero_ps();
                for (; k < K16; k += 32)
                {
                    s0 = _mm512_loadu_ps(S0 + k + 0 * F);
                    s1 = _mm512_loadu_ps(S0 + k + 1 * F);
                    w0 = _mm512_loadu_ps(W0 + k + 0 * F);
                    w1 = _mm512_loadu_ps(W0 + k + 1 * F);
                    d00 = _mm512_fmadd_ps(s0, w0, d00);
                    d01 = _mm512_fmadd_ps(s1, w1, d01);
                    w0 = _mm512_loadu_ps(W1 + k + 0 * F);
                    w1 = _mm512_loadu_ps(W1 + k + 1 * F);
                    d10 = _mm512_fmadd_ps(s0, w0, d10);
                    d11 = _mm512_fmadd_ps(s1, w1, d11);
                    w0 = _mm512_loadu_ps(W2 + k + 0 * F);
                    w1 = _mm512_loadu_ps(W2 + k + 1 * F);
                    d20 = _mm512_fmadd_ps(s0, w0, d20);
                    d21 = _mm512_fmadd_ps(s1, w1, d21);
                    w0 = _mm512_loadu_ps(W3 + k + 0 * F);
                    w1 = _mm512_loadu_ps(W3 + k + 1 * F);
                    d30 = _mm512_fmadd_ps(s0, w0, d30);
                    d31 = _mm512_fmadd_ps(s1, w1, d31);
                }
                d00 = _mm512_add_ps(d00, d01);
                d10 = _mm512_add_ps(d10, d11);
                d20 = _mm512_add_ps(d20, d21);
                d30 = _mm512_add_ps(d30, d31);
            }
            for (; k < K16; k += 16)
            {
                s0 = _mm512_loadu_ps(S0 + k + 0 * F);
                w0 = _mm512_loadu_ps(W0 + k + 0 * F);
                d00 = _mm512_fmadd_ps(s0, w0, d00);
                w0 = _mm512_loadu_ps(W1 + k + 0 * F);
                d10 = _mm512_fmadd_ps(s0, w0, d10);
                w0 = _mm512_loadu_ps(W2 + k + 0 * F);
                d20 = _mm512_fmadd_ps(s0, w0, d20);
                w0 = _mm512_loadu_ps(W3 + k + 0 * F);
                d30 = _mm512_fmadd_ps(s0, w0, d30);
            }
            if (k < K)
            {
                __mmask16 tail = __mmask16(-1) >> (16 + k - K);
                s0 = _mm512_maskz_loadu_ps(tail, S0 + k);
                w0 = _mm512_maskz_loadu_ps(tail, W0 + k);
                d00 = _mm512_fmadd_ps(s0, w0, d00);
                w0 = _mm512_maskz_loadu_ps(tail, W1 + k);
                d10 = _mm512_fmadd_ps(s0, w0, d10);
                w0 = _mm512_maskz_loadu_ps(tail, W2 + k);
                d20 = _mm512_fmadd_ps(s0, w0, d20);
                w0 = _mm512_maskz_loadu_ps(tail, W3 + k);
                d30 = _mm512_fmadd_ps(s0, w0, d30);
            }
            _mm_storeu_ps(D, _mm_add_ps(Avx512f::Extract4Sums(d00, d10, d20, d30), _mm_loadu_ps(B)));
        }

        void SynetInnerProductLayerForward(const float * src, const float * weight, const float * bias, size_t count, size_t size, float * dst)
        {
            float _bias[4] = { 0, 0, 0, 0 };
            size_t count4 = AlignLo(count, 4);
            size_t i = 0;
            for (; i < count4; i += 4)
                SynetInnerProductLayerForward4(src, weight + i * size, (bias ? bias + i : _bias), size, dst + i);
            for (; i < count; ++i)
                SynetInnerProductLayerForward1(src, weight + i * size, (bias ? bias + i : _bias), size, dst + i);
        }

        SIMD_INLINE __m512 NoseSquareSum(const float * src)
        {
            __m512 s0 = _mm512_maskz_loadu_ps(0xFFFC, src - 2);
            __m512 s1 = _mm512_maskz_loadu_ps(0xFFFE, src - 1);
            __m512 s2 = _mm512_loadu_ps(src);
            __m512 s3 = _mm512_loadu_ps(src + 1);
            __m512 s4 = _mm512_loadu_ps(src + 2);
            return _mm512_add_ps(_mm512_fmadd_ps(s0, s0, _mm512_mul_ps(s1, s1)), _mm512_fmadd_ps(s2, s2, _mm512_fmadd_ps(s3, s3, _mm512_mul_ps(s4, s4))));
        }

        SIMD_INLINE __m512 BodySquareSum(const float * src)
        {
            __m512 s0 = _mm512_loadu_ps(src - 2);
            __m512 s1 = _mm512_loadu_ps(src - 1);
            __m512 s2 = _mm512_loadu_ps(src);
            __m512 s3 = _mm512_loadu_ps(src + 1);
            __m512 s4 = _mm512_loadu_ps(src + 2);
            return _mm512_add_ps(_mm512_fmadd_ps(s0, s0, _mm512_mul_ps(s1, s1)), _mm512_fmadd_ps(s2, s2, _mm512_fmadd_ps(s3, s3, _mm512_mul_ps(s4, s4))));
        }

        SIMD_INLINE __m512 TailSquareSum(const float * src)
        {
            __m512 s0 = _mm512_loadu_ps(src - 2);
            __m512 s1 = _mm512_loadu_ps(src - 1);
            __m512 s2 = _mm512_loadu_ps(src);
            __m512 s3 = _mm512_maskz_loadu_ps(0x7FFF, src + 1);
            __m512 s4 = _mm512_maskz_loadu_ps(0x3FFF, src + 2);
            return _mm512_add_ps(_mm512_fmadd_ps(s0, s0, _mm512_mul_ps(s1, s1)), _mm512_fmadd_ps(s2, s2, _mm512_fmadd_ps(s3, s3, _mm512_mul_ps(s4, s4))));
        }

        template <bool align> void SynetLrnLayerCrossChannels(const float * src, size_t half, size_t count, size_t size, const float * k, float * dst, SimdBool trans)
        {
            __m512 k0 = _mm512_set1_ps(k[0]);
            __m512 k1 = _mm512_set1_ps(k[1]);
            __m512 k2 = _mm512_set1_ps(k[2]);
            Avx512f::Pow pow;
            if (trans)
            {
                if (half != 2 || count < F + half)
                {
                    Avx2::SynetLrnLayerCrossChannels(src, half, count, size, k, dst, trans);
                    return;
                }
                size_t aligned = AlignLo(count - half, F);
                for (size_t j = 0; j < size; ++j)
                {
                    Avx512f::Store<align>(dst + 0, _mm512_mul_ps(Avx512f::Load<align>(src + 0), pow(_mm512_fmadd_ps(k1, NoseSquareSum(src + 0), k0), k2)));
                    for (size_t i = F; i < aligned; i += F)
                        Avx512f::Store<align>(dst + i, _mm512_mul_ps(Avx512f::Load<align>(src + i), pow(_mm512_fmadd_ps(k1, BodySquareSum(src + i), k0), k2)));
                    if (aligned != count - half)
                    {
                        size_t i = count - half - F;
                        Avx512f::Store<false>(dst + i, _mm512_mul_ps(Avx512f::Load<false>(src + i), pow(_mm512_fmadd_ps(k1, BodySquareSum(src + i), k0), k2)));
                    }
                    size_t i = count - F;
                    Avx512f::Store<false>(dst + i, _mm512_mul_ps(Avx512f::Load<false>(src + i), pow(_mm512_fmadd_ps(k1, TailSquareSum(src + i), k0), k2)));
                    src += count;
                    dst += count;
                }
            }
            else
            {
                size_t aligned = AlignLo(size, F);
                __mmask16 tail = TailMask16(size - aligned);
                Array32f sum(size, true), zero(size, true);

                for (size_t i = 0; i < half; ++i)
                {
                    const float * pos = src + i * size;
                    size_t j = 0;
                    for (; j < aligned; j += F)
                    {
                        __m512 _pos = Avx512f::Load<align>(pos + j);
                        Avx512f::Store<true>(sum.data + j, _mm512_fmadd_ps(_pos, _pos, Avx512f::Load<true>(sum.data + j)));
                    }
                    if (j < size)
                    {
                        __m512 _pos = Avx512f::Load<align, true>(pos + j, tail);
                        __m512 _sum = Avx512f::Load<true, true>(sum.data + j, tail);
                        Avx512f::Store<true, true>(sum.data + j, _mm512_fmadd_ps(_pos, _pos, _sum), tail);
                    }
                }

                for (size_t i = 0; i < count; ++i)
                {
                    const float * pos = (i < count - half) ? src + half * size : zero.data;
                    const float * neg = (i > half) ? src - (half + 1) * size : zero.data;
                    size_t j = 0;
                    for (; j < aligned; j += F)
                    {
                        __m512 _pos = Avx512f::Load<align>(pos + j);
                        __m512 _neg = Avx512f::Load<align>(neg + j);
                        __m512 _sum = Avx512f::Load<true>(sum.data + j);
                        _sum = _mm512_fmadd_ps(_pos, _pos, _mm512_fnmadd_ps(_neg, _neg, _sum));
                        __m512 _src = Avx512f::Load<align>(src + j);
                        Avx512f::Store<true>(sum.data + j, _sum);
                        Avx512f::Store<align>(dst + j, _mm512_mul_ps(_src, pow(_mm512_fmadd_ps(k1, _sum, k0), k2)));
                    }
                    if (j < size)
                    {
                        __m512 _pos = Avx512f::Load<align, true>(pos + j, tail);
                        __m512 _neg = Avx512f::Load<align, true>(neg + j, tail);
                        __m512 _sum = Avx512f::Load<true, true>(sum.data + j, tail);
                        _sum = _mm512_fmadd_ps(_pos, _pos, _mm512_fnmadd_ps(_neg, _neg, _sum));
                        __m512 _src = Avx512f::Load<align, true>(src + j, tail);
                        Avx512f::Store<true, true>(sum.data + j, _sum, tail);
                        Avx512f::Store<align, true>(dst + j, _mm512_mul_ps(_src, pow(_mm512_fmadd_ps(k1, _sum, k0), k2)), tail);
                    }
                    src += size;
                    dst += size;
                }
            }
        }

        void SynetLrnLayerCrossChannels(const float * src, size_t half, size_t count, size_t size, const float * k, float * dst, SimdBool trans)
        {
            if (Aligned(src) && Aligned(dst) && (trans ? Aligned(count) : Aligned(size)))
                SynetLrnLayerCrossChannels<true>(src, half, count, size, k, dst, trans);
            else
                SynetLrnLayerCrossChannels<false>(src, half, count, size, k, dst, trans);
        }

        SIMD_INLINE void PoolingMaxHwc1(const float * src, size_t srcS, size_t srcC, size_t kH, size_t kW, const __m512 & min, float * dst, __mmask16 tail = -1)
        {
            __m512 max0 = min;
            for (size_t h = 0; h < kH; ++h)
            {
                for (size_t w = 0; w < kW; ++w)
                {
                    max0 = _mm512_max_ps(max0, _mm512_maskz_loadu_ps(tail, src + w * srcC + 0 * F));
                }
                src += srcS;
            }
            _mm512_mask_storeu_ps(dst + 0 * F, tail, max0);
        }

        SIMD_INLINE void PoolingMaxHwc2(const float * src, size_t srcS, size_t srcC, size_t kH, size_t kW, const __m512 & min, float * dst)
        {
            __m512 max0 = min;
            __m512 max1 = min;
            for (size_t h = 0; h < kH; ++h)
            {
                for (size_t w = 0; w < kW; ++w)
                {
                    max0 = _mm512_max_ps(max0, _mm512_loadu_ps(src + w * srcC + 0 * F));
                    max1 = _mm512_max_ps(max1, _mm512_loadu_ps(src + w * srcC + 1 * F));
                }
                src += srcS;
            }
            _mm512_storeu_ps(dst + 0 * F, max0);
            _mm512_storeu_ps(dst + 1 * F, max1);
        }

        SIMD_INLINE void PoolingMaxHwc4(const float * src, size_t srcS, size_t srcC, size_t kH, size_t kW, const __m512 & min, float * dst)
        {
            __m512 max0 = min;
            __m512 max1 = min;
            __m512 max2 = min;
            __m512 max3 = min;
            for (size_t h = 0; h < kH; ++h)
            {
                for (size_t w = 0; w < kW; ++w)
                {
                    max0 = _mm512_max_ps(max0, _mm512_loadu_ps(src + w * srcC + 0 * F));
                    max1 = _mm512_max_ps(max1, _mm512_loadu_ps(src + w * srcC + 1 * F));
                    max2 = _mm512_max_ps(max2, _mm512_loadu_ps(src + w * srcC + 2 * F));
                    max3 = _mm512_max_ps(max3, _mm512_loadu_ps(src + w * srcC + 3 * F));
                }
                src += srcS;
            }
            _mm512_storeu_ps(dst + 0 * F, max0);
            _mm512_storeu_ps(dst + 1 * F, max1);
            _mm512_storeu_ps(dst + 2 * F, max2);
            _mm512_storeu_ps(dst + 3 * F, max3);
        }

        SIMD_INLINE void PoolingMaxHwc8(const float * src, size_t srcS, size_t srcC, size_t kH, size_t kW, const __m512 & min, float * dst)
        {
            __m512 max0 = min;
            __m512 max1 = min;
            __m512 max2 = min;
            __m512 max3 = min;
            __m512 max4 = min;
            __m512 max5 = min;
            __m512 max6 = min;
            __m512 max7 = min;
            for (size_t h = 0; h < kH; ++h)
            {
                for (size_t w = 0; w < kW; ++w)
                {
                    max0 = _mm512_max_ps(max0, _mm512_loadu_ps(src + w * srcC + 0 * F));
                    max1 = _mm512_max_ps(max1, _mm512_loadu_ps(src + w * srcC + 1 * F));
                    max2 = _mm512_max_ps(max2, _mm512_loadu_ps(src + w * srcC + 2 * F));
                    max3 = _mm512_max_ps(max3, _mm512_loadu_ps(src + w * srcC + 3 * F));
                    max4 = _mm512_max_ps(max4, _mm512_loadu_ps(src + w * srcC + 4 * F));
                    max5 = _mm512_max_ps(max5, _mm512_loadu_ps(src + w * srcC + 5 * F));
                    max6 = _mm512_max_ps(max6, _mm512_loadu_ps(src + w * srcC + 6 * F));
                    max7 = _mm512_max_ps(max7, _mm512_loadu_ps(src + w * srcC + 7 * F));
                }
                src += srcS;
            }
            _mm512_storeu_ps(dst + 0 * F, max0);
            _mm512_storeu_ps(dst + 1 * F, max1);
            _mm512_storeu_ps(dst + 2 * F, max2);
            _mm512_storeu_ps(dst + 3 * F, max3);
            _mm512_storeu_ps(dst + 4 * F, max4);
            _mm512_storeu_ps(dst + 5 * F, max5);
            _mm512_storeu_ps(dst + 6 * F, max6);
            _mm512_storeu_ps(dst + 7 * F, max7);
        }

        void SynetPoolingForwardMax(const float * src, size_t srcC, size_t srcH, size_t srcW, size_t kernelY, size_t kernelX,
            size_t strideY, size_t strideX, size_t padY, size_t padX, float * dst, size_t dstH, size_t dstW, SimdBool trans)
        {
            if (trans)
            {
                size_t srcS = srcW * srcC;
                size_t srcCF1 = AlignLo(srcC, 1 * F);
                size_t srcCF2 = AlignLo(srcC, 2 * F);
                size_t srcCF4 = AlignLo(srcC, 4 * F);
                size_t srcCF8 = AlignLo(srcC, 8 * F);
                __m512 min = _mm512_set1_ps(-FLT_MAX);
                __mmask16 tail = TailMask16(srcC - srcCF1);
                for (size_t ph = 0; ph < dstH; ++ph)
                {
                    size_t hStart = ph * strideY - padY;
                    size_t hEnd = Simd::Min(hStart + kernelY, srcH);
                    hStart = Simd::Max<ptrdiff_t>(0, hStart);
                    for (size_t pw = 0; pw < dstW; ++pw)
                    {
                        size_t wStart = pw * strideX - padX;
                        size_t wEnd = Simd::Min(wStart + kernelX, srcW);
                        wStart = Simd::Max<ptrdiff_t>(0, wStart);
                        const float * ps = src + hStart * srcS + wStart * srcC;
                        size_t c = 0;
                        for (; c < srcCF8; c += 8 * F)
                            PoolingMaxHwc8(ps + c, srcS, srcC, hEnd - hStart, wEnd - wStart, min, dst + c);
                        for (; c < srcCF4; c += 4 * F)
                            PoolingMaxHwc4(ps + c, srcS, srcC, hEnd - hStart, wEnd - wStart, min, dst + c);
                        for (; c < srcCF2; c += 2 * F)
                            PoolingMaxHwc2(ps + c, srcS, srcC, hEnd - hStart, wEnd - wStart, min, dst + c);
                        for (; c < srcCF1; c += 1 * F)
                            PoolingMaxHwc1(ps + c, srcS, srcC, hEnd - hStart, wEnd - wStart, min, dst + c);
                        if (c < srcC)
                            PoolingMaxHwc1(ps + c, srcS, srcC, hEnd - hStart, wEnd - wStart, min, dst + c, tail);
                        dst += srcC;
                    }
                }
                return;
            }
            else
            {
                if (strideY == 1 && strideX == 1 && kernelY == 3 && kernelX == 3 && srcH == dstH && srcW == dstW && dstW > F)
                {
                    for (size_t c = 0; c < srcC; ++c, src += srcH * srcW, dst += dstH * dstW)
                        Avx512f::NeuralPooling1x1Max3x3(src, srcW, srcW, srcH, dst, dstW);
                    return;
                }
                if (strideY == 2 && strideX == 2 && kernelY == 2 && kernelX == 2 && padY == 0 && padX == 0 && dstW >=  F)
                {
                    for (size_t c = 0; c < srcC; ++c, src += srcH * srcW, dst += dstH * dstW)
                        Avx512f::NeuralPooling2x2Max2x2(src, srcW, srcW, srcH, dst, dstW);
                    return;
                }
                if (strideY == 2 && strideX == 2 && kernelY == 3 && kernelX == 3 && padY == 0 && padX == 0 && dstW > F)
                {
                    for (size_t c = 0; c < srcC; ++c, src += srcH * srcW, dst += dstH * dstW)
                        Avx512f::NeuralPooling2x2Max3x3(src, srcW, srcW, srcH, dst, dstW);
                    return;
                }
            }
            Avx2::SynetPoolingForwardMax(src, srcC, srcH, srcW, kernelY, kernelX, strideY, strideX, padY, padX, dst, dstH, dstW, trans);
       }

        template <bool align, bool mask> SIMD_INLINE void SynetPreluLayerForward(const float * src, const float * slope, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 _slope = Load<align, mask>(slope + offset, tail);
            __m512 pos = _mm512_max_ps(_mm512_setzero_ps(), _src);
            __m512 neg = _mm512_min_ps(_mm512_setzero_ps(), _src);
            Store<align, mask>(dst + offset, _mm512_add_ps(pos, _mm512_mul_ps(_slope, neg)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetPreluLayerForward(const float * src, __m512 slope, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 pos = _mm512_max_ps(_mm512_setzero_ps(), _src);
            __m512 neg = _mm512_min_ps(_mm512_setzero_ps(), _src);
            Store<align, mask>(dst + offset, _mm512_add_ps(pos, _mm512_mul_ps(slope, neg)), tail);
        }

        template <bool align> void SynetPreluLayerForward(const float * src, const float * slope, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(slope) : Aligned(size)) && Aligned(src) && Aligned(dst));
            if ((trans || size == 1) && count != 1)
            {
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - count);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    for (; i < aligned; i += QF)
                    {
                        SynetPreluLayerForward<align, false>(src, slope, dst, i + F * 0);
                        SynetPreluLayerForward<align, false>(src, slope, dst, i + F * 1);
                        SynetPreluLayerForward<align, false>(src, slope, dst, i + F * 2);
                        SynetPreluLayerForward<align, false>(src, slope, dst, i + F * 3);
                    }
                    for (; i < partial; i += F)
                        SynetPreluLayerForward<align, false>(src, slope, dst, i);
                    if(i < count)
                        SynetPreluLayerForward<align, true>(src, slope, dst, i, tail);
                    src += count;
                    dst += count;
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - size);
                for (size_t i = 0; i < count; ++i)
                {
                    size_t j = 0;
                    __m512 _slope = _mm512_set1_ps(slope[i]);
                    for (; j < aligned; j += QF)
                    {
                        SynetPreluLayerForward<align, false>(src, _slope, dst, j + F * 0);
                        SynetPreluLayerForward<align, false>(src, _slope, dst, j + F * 1);
                        SynetPreluLayerForward<align, false>(src, _slope, dst, j + F * 2);
                        SynetPreluLayerForward<align, false>(src, _slope, dst, j + F * 3);
                    }
                    for (; j < partial; j += F)
                        SynetPreluLayerForward<align, false>(src, _slope, dst, j);
                    if (i < count)
                        SynetPreluLayerForward<align, true>(src, _slope, dst, j, tail);
                    src += size;
                    dst += size;
                }
            }
        }

        void SynetPreluLayerForward(const float * src, const float * slope, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(slope) : Aligned(size)) && Aligned(src) && Aligned(dst))
                SynetPreluLayerForward<true>(src, slope, count, size, dst, trans);
            else
                SynetPreluLayerForward<false>(src, slope, count, size, dst, trans);
        }

        template <bool align> void SynetRestrictRange(const float * src, size_t size, const float * lower, const float * upper, float * dst)
        {
            assert(lower[0] <= upper[0]);
            if (align)
                assert(Aligned(src) && Aligned(dst));
            float min = *lower;
            float max = *upper;
            __m512 _min = _mm512_set1_ps(min);
            __m512 _max = _mm512_set1_ps(max);
            size_t sizeF = Simd::AlignLo(size, F);
            size_t sizeQF = Simd::AlignLo(size, QF);
            size_t i = 0;
            for (; i < sizeQF; i += QF)
            {
                Store<align>(dst + i + 0 * F, _mm512_min_ps(_mm512_max_ps(_min, Load<align>(src + i + 0 * F)), _max));
                Store<align>(dst + i + 1 * F, _mm512_min_ps(_mm512_max_ps(_min, Load<align>(src + i + 1 * F)), _max));
                Store<align>(dst + i + 2 * F, _mm512_min_ps(_mm512_max_ps(_min, Load<align>(src + i + 2 * F)), _max));
                Store<align>(dst + i + 3 * F, _mm512_min_ps(_mm512_max_ps(_min, Load<align>(src + i + 3 * F)), _max));
            }
            for (; i < sizeF; i += F)
                Store<align>(dst + i, _mm512_min_ps(_mm512_max_ps(_min, Load<align>(src + i)), _max));
            if(i < size)
            {
                __mmask16 tail = TailMask16(size - i);
                Store<align, true>(dst + i, _mm512_min_ps(_mm512_max_ps(_min, (Load<align, true>(src + i, tail))), _max), tail);
            }
        }

        void SynetRestrictRange(const float * src, size_t size, const float * lower, const float * upper, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                SynetRestrictRange<true>(src, size, lower, upper, dst);
            else
                SynetRestrictRange<false>(src, size, lower, upper, dst);
        }

        //---------------------------------------------------------------------

        template <bool align, bool mask> SIMD_INLINE void SynetScaleLayerForward(const float * src, const float * scale, const float * bias, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            Store<align, mask>(dst + offset, _mm512_fmadd_ps(_src, _scale, _bias), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetScaleLayerForward(const float * src, const float * scale, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            Store<align, mask>(dst + offset, _mm512_mul_ps(_src, _scale), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetScaleLayerForward(const float * src, const __m512 & scale, const __m512 & bias, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            Store<align, mask>(dst + offset, _mm512_fmadd_ps(_src, scale, bias), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetScaleLayerForward(const float * src, const __m512 & scale, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            Store<align, mask>(dst + offset, _mm512_mul_ps(_src, scale), tail);
        }

        template <bool align> SIMD_INLINE void SynetScaleLayerForward(const float * src, const float * scale, const float * bias, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst));
            if ((trans || size == 1) && count != 1)
            {
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - count);
                if (bias)
                {
                    for (size_t j = 0; j < size; ++j)
                    {
                        size_t i = 0;
                        for (; i < aligned; i += QF)
                        {
                            SynetScaleLayerForward<align, false>(src, scale, bias, dst, i + F * 0);
                            SynetScaleLayerForward<align, false>(src, scale, bias, dst, i + F * 1);
                            SynetScaleLayerForward<align, false>(src, scale, bias, dst, i + F * 2);
                            SynetScaleLayerForward<align, false>(src, scale, bias, dst, i + F * 3);
                        }
                        for (; i < partial; i += F)
                            SynetScaleLayerForward<align, false>(src, scale, bias, dst, i);
                        if (i < count)
                            SynetScaleLayerForward<align, true>(src, scale, bias, dst, i, tail);
                        src += count;
                        dst += count;
                    }
                }
                else
                {
                    for (size_t j = 0; j < size; ++j)
                    {
                        size_t i = 0;
                        for (; i < aligned; i += QF)
                        {
                            SynetScaleLayerForward<align, false>(src, scale, dst, i + F * 0);
                            SynetScaleLayerForward<align, false>(src, scale, dst, i + F * 1);
                            SynetScaleLayerForward<align, false>(src, scale, dst, i + F * 2);
                            SynetScaleLayerForward<align, false>(src, scale, dst, i + F * 3);
                        }
                        for (; i < partial; i += F)
                            SynetScaleLayerForward<align, false>(src, scale,  dst, i);
                        if (i < count)
                            SynetScaleLayerForward<align, true>(src, scale, dst, i, tail);
                        src += count;
                        dst += count;
                    }
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                __mmask16 tail = __mmask16(-1) >> (F + partial - size);
                if (bias)
                {
                    for (size_t i = 0; i < count; ++i)
                    {
                        size_t j = 0;
                        __m512 _scale = _mm512_set1_ps(scale[i]);
                        __m512 _bias = _mm512_set1_ps(bias[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, j + F * 0);
                            SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, j + F * 1);
                            SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, j + F * 2);
                            SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, j + F * 3);
                        }
                        for (; j < partial; j += F)
                            SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, j);
                        if (j < size)
                            SynetScaleLayerForward<align, true>(src, _scale, _bias, dst, j, tail);
                        src += size;
                        dst += size;
                    }
                }
                else
                {
                    for (size_t i = 0; i < count; ++i)
                    {
                        size_t j = 0;
                        __m512 _scale = _mm512_set1_ps(scale[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetScaleLayerForward<align, false>(src, _scale, dst, j + F * 0);
                            SynetScaleLayerForward<align, false>(src, _scale, dst, j + F * 1);
                            SynetScaleLayerForward<align, false>(src, _scale, dst, j + F * 2);
                            SynetScaleLayerForward<align, false>(src, _scale, dst, j + F * 3);
                        }
                        for (; j < partial; j += F)
                            SynetScaleLayerForward<align, false>(src, _scale, dst, j);
                        if (j < size)
                            SynetScaleLayerForward<align, true>(src, _scale, dst, j, tail);
                        src += size;
                        dst += size;
                    }
                }
            }
        }

        template <bool align> void SynetScaleLayerForwardNchw(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(spatial) && Aligned(dst));

            size_t aligned = AlignLo(spatial, QF);
            size_t partial = AlignLo(spatial, F);
            __mmask16 tail = TailMask16(spatial - partial);
            if (bias)
            {
                for (size_t c = 0; c < channels; ++c)
                {
                    size_t s = 0;
                    __m512 _scale = _mm512_set1_ps(scale[c]);
                    __m512 _bias = _mm512_set1_ps(bias[c]);
                    for (; s < aligned; s += QF)
                    {
                        SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, s + F * 0);
                        SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, s + F * 1);
                        SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, s + F * 2);
                        SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, s + F * 3);
                    }
                    for (; s < partial; s += F)
                        SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, s);
                    if (s < spatial)
                        SynetScaleLayerForward<align, true>(src, _scale, _bias, dst, s, tail);
                    src += spatial;
                    dst += spatial;
                }
            }
            else
            {
                for (size_t c = 0; c < channels; ++c)
                {
                    size_t s = 0;
                    __m512 _scale = _mm512_set1_ps(scale[c]);
                    for (; s < aligned; s += QF)
                    {
                        SynetScaleLayerForward<align, false>(src, _scale, dst, s + F * 0);
                        SynetScaleLayerForward<align, false>(src, _scale, dst, s + F * 1);
                        SynetScaleLayerForward<align, false>(src, _scale, dst, s + F * 2);
                        SynetScaleLayerForward<align, false>(src, _scale, dst, s + F * 3);
                    }
                    for (; s < partial; s += F)
                        SynetScaleLayerForward<align, false>(src, _scale, dst, s);
                    if (s < spatial)
                        SynetScaleLayerForward<align, true>(src, _scale, dst, s, tail);
                    src += spatial;
                    dst += spatial;
                }
            }
        }

        SIMD_INLINE void SynetScaleLayerForwardNchw(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(spatial) && Aligned(dst))
                SynetScaleLayerForwardNchw<true>(src, scale, bias, channels, spatial, dst);
            else
                SynetScaleLayerForwardNchw<false>(src, scale, bias, channels, spatial, dst);
        }

        template <bool align> void SynetScaleLayerForwardNhwc(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(scale) && Aligned(bias) && Aligned(channels) && Aligned(dst));

            size_t aligned = AlignLo(channels, QF);
            size_t partial = AlignLo(channels, F);
            __mmask16 tail = TailMask16(channels - partial);
            if (bias)
            {
                for (size_t s = 0; s < spatial; ++s)
                {
                    size_t c = 0;
                    for (; c < aligned; c += QF)
                    {
                        SynetScaleLayerForward<align, false>(src, scale, bias, dst, c + F * 0);
                        SynetScaleLayerForward<align, false>(src, scale, bias, dst, c + F * 1);
                        SynetScaleLayerForward<align, false>(src, scale, bias, dst, c + F * 2);
                        SynetScaleLayerForward<align, false>(src, scale, bias, dst, c + F * 3);
                    }
                    for (; c < partial; c += F)
                        SynetScaleLayerForward<align, false>(src, scale, bias, dst, c);
                    if (c < channels)
                        SynetScaleLayerForward<align, true>(src, scale, bias, dst, c, tail);
                    src += channels;
                    dst += channels;
                }
            }
            else
            {
                for (size_t s = 0; s < spatial; ++s)
                {
                    size_t c = 0;
                    for (; c < aligned; c += QF)
                    {
                        SynetScaleLayerForward<align, false>(src, scale, dst, c + F * 0);
                        SynetScaleLayerForward<align, false>(src, scale, dst, c + F * 1);
                        SynetScaleLayerForward<align, false>(src, scale, dst, c + F * 2);
                        SynetScaleLayerForward<align, false>(src, scale, dst, c + F * 3);
                    }
                    for (; c < partial; c += F)
                        SynetScaleLayerForward<align, false>(src, scale, dst, c);
                    if (c < channels)
                        SynetScaleLayerForward<align, true>(src, scale, dst, c, tail);
                    src += channels;
                    dst += channels;
                }
            }
        }

        template <bool align> void SynetScaleLayerForwardNhwc3(const float * src, const float * scale, const float * bias, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(dst));

            size_t spatial3 = spatial * 3;
            size_t spatialF3 = AlignLo(spatial, F) * 3;
            if (bias)
            {
                size_t s = 0;
                if (spatialF3)
                {
                    float _scale[F * 3], _bias[F * 3];
                    for (size_t i = 0; i < F; ++i)
                        for (size_t c = 0; c < 3; ++c)
                            _scale[i * 3 + c] = scale[c], _bias[i * 3 + c] = bias[c];
                    __m512 _scale0 = Load<false>(_scale + 0 * F);
                    __m512 _scale1 = Load<false>(_scale + 1 * F);
                    __m512 _scale2 = Load<false>(_scale + 2 * F);
                    __m512 _bias0 = Load<false>(_bias + 0 * F);
                    __m512 _bias1 = Load<false>(_bias + 1 * F);
                    __m512 _bias2 = Load<false>(_bias + 2 * F);
                    for (; s < spatialF3; s += F * 3)
                    {
                        SynetScaleLayerForward<align, false>(src, _scale0, _bias0, dst, s + F * 0);
                        SynetScaleLayerForward<align, false>(src, _scale1, _bias1, dst, s + F * 1);
                        SynetScaleLayerForward<align, false>(src, _scale2, _bias2, dst, s + F * 2);
                    }
                }
                for (; s < spatial3; s += 3)
                {
                    dst[s + 0] = src[s + 0] * scale[0] + bias[0];
                    dst[s + 1] = src[s + 1] * scale[1] + bias[1];
                    dst[s + 2] = src[s + 2] * scale[2] + bias[2];
                }
            }
            else
            {
                size_t s = 0;
                if (spatialF3)
                {
                    float _scale[F * 3];
                    for (size_t i = 0; i < F; ++i)
                        for (size_t c = 0; c < 3; ++c)
                            _scale[i * 3 + c] = scale[c];
                    __m512 _scale0 = Load<false>(_scale + 0 * F);
                    __m512 _scale1 = Load<false>(_scale + 1 * F);
                    __m512 _scale2 = Load<false>(_scale + 2 * F);
                    for (; s < spatialF3; s += F * 3)
                    {
                        SynetScaleLayerForward<align, false>(src, _scale0, dst, s + F * 0);
                        SynetScaleLayerForward<align, false>(src, _scale1, dst, s + F * 1);
                        SynetScaleLayerForward<align, false>(src, _scale2, dst, s + F * 2);
                    }
                }
                for (; s < spatial3; s += 3)
                {
                    dst[s + 0] = src[s + 0] * scale[0];
                    dst[s + 1] = src[s + 1] * scale[1];
                    dst[s + 2] = src[s + 2] * scale[2];
                }
            }
        }

        SIMD_INLINE void SynetScaleLayerForwardNhwc(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (channels == 3)
            {
                if (Aligned(src) && Aligned(dst))
                    SynetScaleLayerForwardNhwc3<true>(src, scale, bias, spatial, dst);
                else
                    SynetScaleLayerForwardNhwc3<false>(src, scale, bias, spatial, dst);
            }
            else
            {
                if (Aligned(src) && Aligned(scale) && Aligned(bias) && Aligned(channels) && Aligned(dst))
                    SynetScaleLayerForwardNhwc<true>(src, scale, bias, channels, spatial, dst);
                else
                    SynetScaleLayerForwardNhwc<false>(src, scale, bias, channels, spatial, dst);
            }
        }

        template <bool align> void SynetScaleLayerForwardNchw16c(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(dst));

            size_t spatialF = spatial * F;
            size_t spatial4F = AlignLo(spatial, 4)*F;
            if (bias)
            {
                for (size_t c = 0; c < channels; c += F)
                {
                    __m512 _scale = Load<false>(scale + c);
                    __m512 _bias = Load<false>(bias + c);
                    size_t s = 0;
                    for (; s < spatial4F; s += 4 * F)
                    {
                        SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, s + F * 0);
                        SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, s + F * 1);
                        SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, s + F * 2);
                        SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, s + F * 3);
                    }
                    for (; s < spatialF; s += F)
                        SynetScaleLayerForward<align, false>(src, _scale, _bias, dst, s);
                    src += spatialF;
                    dst += spatialF;
                }
            }
            else
            {
                for (size_t c = 0; c < channels; c += F)
                {
                    __m512 _scale = Load<false>(scale + c);
                    size_t s = 0;
                    for (; s < spatial4F; s += 4 * F)
                    {
                        SynetScaleLayerForward<align, false>(src, _scale, dst, s + F * 0);
                        SynetScaleLayerForward<align, false>(src, _scale, dst, s + F * 1);
                        SynetScaleLayerForward<align, false>(src, _scale, dst, s + F * 2);
                        SynetScaleLayerForward<align, false>(src, _scale, dst, s + F * 3);
                    }
                    for (; s < spatialF; s += F)
                        SynetScaleLayerForward<align, false>(src, _scale, dst, s);
                    src += spatialF;
                    dst += spatialF;
                }
            }
        }

        SIMD_INLINE void SynetScaleLayerForwardNchw16c(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                SynetScaleLayerForwardNchw16c<true>(src, scale, bias, channels, spatial, dst);
            else
                SynetScaleLayerForwardNchw16c<false>(src, scale, bias, channels, spatial, dst);
        }

        void SynetScaleLayerForward(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format)
        {
            if (Base::NchwCompatible(channels, spatial, format))
                SynetScaleLayerForwardNchw(src, scale, bias, channels, spatial, dst);
            else if (Base::NhwcCompatible(channels, spatial, format))
                SynetScaleLayerForwardNhwc(src, scale, bias, channels, spatial, dst);
            else if (format == SimdTensorFormatNchw4c)
                Sse::SynetScaleLayerForward(src, scale, bias, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw8c)
                Avx2::SynetScaleLayerForward(src, scale, bias, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw16c)
                SynetScaleLayerForwardNchw16c(src, scale, bias, channels, spatial, dst);
            else
                Base::SynetScaleLayerForward(src, scale, bias, channels, spatial, dst, format);
        }

        //---------------------------------------------------------------------

        void SynetSoftmaxLayerForward(const float * src, size_t outer, size_t count, size_t inner, float * dst)
        {
            Avx512f::Exp exp;
            if (inner == 1 && count == 2)
            {
                size_t aligned = Simd::AlignLo(outer, F);
                size_t o = 0;
                for (; o < aligned; o += F)
                {
                    __m512 s0 = _mm512_loadu_ps(src + 0);
                    __m512 s1 = _mm512_loadu_ps(src + F);
                    __m512 ss0 = _mm512_shuffle_ps(s0, s1, 0x88);
                    __m512 ss1 = _mm512_shuffle_ps(s0, s1, 0xDD);
                    __m512 max = _mm512_max_ps(ss0, ss1);
                    __m512 exp0 = exp.Exponent(_mm512_sub_ps(ss0, max));
                    __m512 exp1 = exp.Exponent(_mm512_sub_ps(ss1, max));
                    __m512 sum = _mm512_add_ps(exp0, exp1);
                    __m512 d0 = _mm512_div_ps(exp0, sum);
                    __m512 d1 = _mm512_div_ps(exp1, sum);
                    _mm512_storeu_ps(dst + 0, _mm512_unpacklo_ps(d0, d1));
                    _mm512_storeu_ps(dst + F, _mm512_unpackhi_ps(d0, d1));
                    src += DF;
                    dst += DF;
                }
                for (; o < outer; ++o)
                {
                    float max = Simd::Max(src[0], src[1]);
                    float exp0 = ::exp(src[0] - max);
                    float exp1 = ::exp(src[1] - max);
                    float sum = exp0 + exp1;
                    dst[0] = exp0 / sum;
                    dst[1] = exp1 / sum;
                    src += 2;
                    dst += 2;
                }
            }
            else
            {
                size_t aligned = Simd::AlignLo(inner, F);
                __mmask16 tail = TailMask16(inner - aligned);
                Array32f tmp(inner * 2);
                const float * s;
                float * max = tmp.data, *sum = tmp.data + inner, *d;
                for (size_t o = 0; o < outer; ++o)
                {
                    memcpy(max, src, inner * sizeof(float));
                    s = src + inner;
                    for (size_t c = 1; c < count; ++c)
                    {
                        size_t i = 0;
                        for (; i < aligned; i += F)
                            _mm512_storeu_ps(max + i, _mm512_max_ps(_mm512_loadu_ps(s + i), _mm512_loadu_ps(max + i)));
                        if(i < inner)
                            _mm512_mask_storeu_ps(max + i, tail, _mm512_max_ps(_mm512_maskz_loadu_ps(tail, s + i), _mm512_maskz_loadu_ps(tail, max + i)));
                        s += inner;
                    }

                    s = src;
                    d = dst;
                    memset(sum, 0, inner * sizeof(float));
                    for (size_t c = 0; c < count; ++c)
                    {
                        size_t i = 0;
                        for (; i < aligned; i += F)
                        {
                            __m512 _d = exp.Exponent(_mm512_sub_ps(_mm512_loadu_ps(s + i), _mm512_loadu_ps(max + i)));
                            _mm512_storeu_ps(d + i, _d);
                            _mm512_storeu_ps(sum + i, _mm512_add_ps(_d, _mm512_loadu_ps(sum + i)));
                        }
                        if(i < inner)
                        {
                            __m512 _d = exp.Exponent(_mm512_sub_ps(_mm512_maskz_loadu_ps(tail, s + i), _mm512_maskz_loadu_ps(tail, max + i)));
                            _mm512_mask_storeu_ps(d + i, tail, _d);
                            _mm512_mask_storeu_ps(sum + i, tail, _mm512_add_ps(_d, _mm512_maskz_loadu_ps(tail, sum + i)));
                        }
                        s += inner;
                        d += inner;
                    }

                    d = dst;
                    for (size_t c = 0; c < count; ++c)
                    {
                        size_t i = 0;
                        for (; i < aligned; i += F)
                            _mm512_storeu_ps(d + i, _mm512_div_ps(_mm512_loadu_ps(d + i), _mm512_loadu_ps(sum + i)));
                        if(i < inner)
                            _mm512_mask_storeu_ps(d + i, tail, _mm512_div_ps(_mm512_maskz_loadu_ps(tail, d + i), _mm512_maskz_loadu_ps(tail, sum + i)));
                        d += inner;
                    }
                    src += count * inner;
                    dst += count * inner;
                }
            }
        }
    }
#endif// SIMD_AVX512F_ENABLE
}
