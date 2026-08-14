/**
 * @file nucleus.cpp
 * @brief NucleusRegistry default registration
 */

#include <opencog/nervous/nucleus.hpp>
#include <opencog/nervous/nuclei/thalamus.hpp>
#include <opencog/nervous/nuclei/hypothalamus.hpp>
#include <opencog/nervous/nuclei/amygdala.hpp>
#include <opencog/nervous/nuclei/hippocampus.hpp>
#include <opencog/nervous/nuclei/basal_ganglia.hpp>
#include <opencog/nervous/nuclei/prefrontal_cortex.hpp>
#include <opencog/nervous/nuclei/brainstem_autonomic.hpp>
#include <opencog/nervous/nuclei/cerebellum.hpp>
#include <opencog/nervous/nuclei/anterior_cingulate.hpp>
#include <opencog/nervous/nuclei/insula.hpp>

namespace opencog::nerv {

void NucleusRegistry::register_defaults() {
    add_nucleus<ThalamusNucleus>();
    add_nucleus<HypothalamusNucleus>();
    add_nucleus<AmygdalaNucleus>();
    add_nucleus<HippocampusNucleus>();
    add_nucleus<BasalGangliaNucleus>();
    add_nucleus<PrefrontalCortexNucleus>();
    add_nucleus<BrainstemAutonomicNucleus>();
    add_nucleus<CerebellumNucleus>();
    add_nucleus<AnteriorCingulateNucleus>();
    add_nucleus<InsulaNucleus>();
}

} // namespace opencog::nerv
