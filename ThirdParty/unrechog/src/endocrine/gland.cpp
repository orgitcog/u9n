/**
 * @file gland.cpp
 * @brief VirtualGland base and GlandRegistry implementations
 */

#include <opencog/endocrine/gland.hpp>
#include <opencog/endocrine/glands/hpa_axis.hpp>
#include <opencog/endocrine/glands/dopaminergic.hpp>
#include <opencog/endocrine/glands/serotonergic.hpp>
#include <opencog/endocrine/glands/noradrenergic.hpp>
#include <opencog/endocrine/glands/oxytocinergic.hpp>
#include <opencog/endocrine/glands/thyroid.hpp>
#include <opencog/endocrine/glands/circadian.hpp>
#include <opencog/endocrine/glands/pancreatic.hpp>
#include <opencog/endocrine/glands/immune.hpp>
#include <opencog/endocrine/glands/endocannabinoid.hpp>

namespace opencog::endo {

void GlandRegistry::register_defaults() {
    add_gland<HPAAxis>(HPAAxis::default_config());
    add_gland<DopaminergicGland>(DopaminergicGland::default_config());
    add_gland<SerotonergicGland>(SerotonergicGland::default_config());
    add_gland<NoradrenergicGland>(NoradrenergicGland::default_config());
    add_gland<OxytocinergicGland>(OxytocinergicGland::default_config());
    add_gland<ThyroidGland>(ThyroidGland::default_config());
    add_gland<CircadianGland>(CircadianGland::default_config());
    add_gland<PancreaticGland>(PancreaticGland::default_config());
    add_gland<ImmuneMonitor>(ImmuneMonitor::default_config());
    add_gland<EndocannabinoidGland>(EndocannabinoidGland::default_config());
}

} // namespace opencog::endo
