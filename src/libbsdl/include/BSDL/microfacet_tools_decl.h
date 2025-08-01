// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#pragma once

#include <BSDL/config.h>
#include <BSDL/tools.h>
#include <Imath/ImathVec.h>

BSDL_ENTER_NAMESPACE

struct GGXDist {
    BSDL_INLINE_METHOD GGXDist(float rough, float aniso,
                               bool flip_aniso = false)
        : ax(SQR(rough)), ay(ax)
    {
        assert(rough >= 0 && rough <= 1);
        assert(aniso >= 0 && aniso <= 1);
        if (flip_aniso)
            aniso = -aniso;
        constexpr float ALPHA_MIN = 1e-5f;
        ax                        = std::max(ax * (1 + aniso), ALPHA_MIN);
        ay                        = std::max(ay * (1 - aniso), ALPHA_MIN);
    }
    GGXDist() = default;

    BSDL_INLINE_METHOD float D(const Imath::V3f& Hr) const;
    // When using sample_for_refl this gives you D_refl / D for convenience
    BSDL_INLINE_METHOD float D_refl_D(const Imath::V3f& wo,
                                      const Imath::V3f& m) const;
    BSDL_INLINE_METHOD float G1(Imath::V3f w) const;
    BSDL_INLINE_METHOD float G2_G1(Imath::V3f wi, Imath::V3f wo) const;
    // Plain visible micro normal sampling. PDF is G1 * D
    BSDL_INLINE_METHOD Imath::V3f sample(const Imath::V3f& wo, float randu,
                                         float randv) const;

    // Reflection specific improved sample function. Gives you a micro
    // normal that won't reflect under the surface. PDF is D_refl_D() * D
    BSDL_INLINE_METHOD Imath::V3f
    sample_for_refl(const Imath::V3f& wo, float randu, float randv) const;

    BSDL_INLINE_METHOD float roughness() const { return std::max(ax, ay); }

private:
    float ax, ay;
};

template<typename BSDF> struct TabulatedEnergyCurve {
    BSDL_INLINE_METHOD TabulatedEnergyCurve(const float roughness,
                                            const float fresnel_index)
        : roughness(roughness), fresnel_index(fresnel_index)
    {
    }

    BSDL_INLINE_METHOD float interpolate_emiss(int i) const;
    BSDL_INLINE_METHOD float get_Emiss_avg() const;
    BSDL_INLINE_METHOD float Emiss_eval(float c) const;

private:
    float roughness, fresnel_index;
};

// Not a full BxDF, just enough implemented to allow baking tables
template<typename Dist> struct MiniMicrofacet {
    // describe how tabulation should be done
    static constexpr int Nc = 16;
    static constexpr int Nr = 16;
    static constexpr int Nf = 1;

    static constexpr float get_cosine(int i)
    {
        // we don't use a uniform spacing of cosines because we want a bit more resolution near 0
        // where the energy compensation tables tend to vary more quickly
        return std::max(SQR(float(i) * (1.0f / (Nc - 1))), 1e-6f);
    }

    explicit MiniMicrofacet(float rough, float) : d(rough, 0.0f) {}

    BSDL_INLINE_METHOD Sample sample(Imath::V3f wo, float randu, float randv,
                                     float randw) const;

private:
    Dist d;
};

namespace spi {

struct MiniMicrofacetGGX : public MiniMicrofacet<GGXDist> {
    explicit MiniMicrofacetGGX(float, float rough, float)
        : MiniMicrofacet<GGXDist>(rough, 0.0f)
    {
    }
    struct Energy {
        float data[Nf * Nr * Nc];
    };
    static BSDL_INLINE_METHOD Energy& get_energy();

    static constexpr const char* NS = "spi";
    static const char* lut_header() { return "SPI/microfacet_tools_luts.h"; }
    static const char* struct_name() { return "MiniMicrofacetGGX"; }
};

}  // namespace spi

template<typename Fresnel> struct MicrofacetMS {
    // describe how tabulation should be done
    static constexpr int Nc = 16;
    static constexpr int Nr = 16;
    static constexpr int Nf = 32;

    static constexpr float get_cosine(int i)
    {
        // we don't use a uniform spacing of cosines because we want a bit more resolution near 0
        // where the energy compensation tables tend to vary more quickly
        return std::max(SQR(float(i) * (1.0f / (Nc - 1))), 1e-6f);
    }
    static constexpr const char* name() { return "MicrofacetMS"; }

    BSDL_INLINE_METHOD MicrofacetMS() {}

    explicit BSDL_INLINE_METHOD MicrofacetMS(float cosNO, float roughness_index,
                                             float fresnel_index);
    BSDL_INLINE_METHOD MicrofacetMS(const GGXDist& dist, const Fresnel& fresnel,
                                    float cosNO, float roughness);
    BSDL_INLINE_METHOD Sample eval(Imath::V3f wo, Imath::V3f wi) const;
    BSDL_INLINE_METHOD Sample sample(Imath::V3f wo, float randu, float randv,
                                     float randw) const;

    BSDL_INLINE_METHOD const Fresnel& getFresnel() const { return f; }

private:
    BSDL_INLINE_METHOD Power computeFmiss() const;

    GGXDist d;
    Fresnel f;
    float Eo;
    float Eo_avg;
};

// Turquin style microfacet with multiple scattering
template<typename Fresnel>
BSDL_INLINE Sample
eval_turquin_microms_reflection(const GGXDist& dist, const Fresnel& fresnel,
                                float E_ms, const Imath::V3f& wo,
                                const Imath::V3f& wi);

template<typename Fresnel>
BSDL_INLINE Sample
sample_turquin_microms_reflection(const GGXDist& dist, const Fresnel& fresnel,
                                  float E_ms, const Imath::V3f& wo,
                                  const Imath::V3f& rnd);

// SPI style microfacet with multiple scattering
template<typename Dist, typename Fresnel>
BSDL_INLINE Sample
eval_spi_microms_reflection(const Dist& dist, const Fresnel& fresnel,
                            float E_ms, float E_ms_avg, const Imath::V3f& wo,
                            const Imath::V3f& wi);

template<typename Dist, typename Fresnel>
BSDL_INLINE Sample
sample_spi_microms_reflection(const Dist& dist, const Fresnel& fresnel,
                              float E_ms, float E_ms_avg, const Imath::V3f& wo,
                              const Imath::V3f& rnd);

BSDL_LEAVE_NAMESPACE
