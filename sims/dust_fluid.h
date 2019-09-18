#ifndef COSMO_DUST_FLUID_SIM_H
#define COSMO_DUST_FLUID_SIM_H

#include "sim.h"
#include "../cosmo_includes.h"
#include "../components/boundaries/sommerfield.h"
#include "../components/boundaries/periodic.h"
#include "../components/dust_fluid/dust_fluid.h"


namespace cosmo
{

/**
 * derived class based on CosmoSim class (sim.h)
 */
class DustFluidSim : public CosmoSim
{
public:
  DustFluid * dustFluidSim;

  
  
  DustFluidSim(
    const std::shared_ptr<hier::PatchHierarchy>& hierarchy,
    const tbox::Dimension& dim_in,
    std::shared_ptr<tbox::InputDatabase>& input_db_in,
    std::ostream* l_stream_in,
    std::string simulation_type_in,
    std::string vis_filename_in);
  
  ~DustFluidSim();
  
  void init();
  void setICs(const std::shared_ptr<hier::PatchHierarchy>& hierarchy);
  void initDustFluidStep(
    const std::shared_ptr<hier::PatchHierarchy>& hierarchy);
  void outputDustFluidStep(
    const std::shared_ptr<hier::PatchHierarchy>& hierarchy);
  void runDustFluidStep(
    const std::shared_ptr<hier::PatchHierarchy>& hierarchy,
  double from_t, double to_t);
  virtual void runStep(
    const std::shared_ptr<hier::PatchHierarchy>& hierarchy);
  double getDt(
    const std::shared_ptr<hier::PatchHierarchy>& hierarchy);

  
  virtual void
    initializeLevelData(
      /*! Hierarchy to initialize */
      const std::shared_ptr<hier::PatchHierarchy>& hierarchy,
      /*! Level to initialize */
      const int level_number,
      const double init_data_time,
      const bool can_be_refined,
      /*! Whether level is being introduced for the first time */
      const bool initial_time,
      /*! Level to copy data from */
      const std::shared_ptr<hier::PatchLevel>& old_level =
      std::shared_ptr<hier::PatchLevel>(),
      /*! Whether data on new patch needs to be allocated */
      const bool allocate_data = true);

  virtual void
    resetHierarchyConfiguration(
      /*! New hierarchy */
      const std::shared_ptr<hier::PatchHierarchy>& new_hierarchy,
      /*! Coarsest level */ int coarsest_level,
      /*! Finest level */ int finest_level);

  virtual void
    applyGradientDetector(
      const std::shared_ptr<hier::PatchHierarchy>& hierarchy,
      const int level_number,
      const double error_data_time,
      const int tag_index,
      const bool initial_time,
      const bool uses_richardson_extrapolation);

  virtual void putToRestart(
    const std::shared_ptr<tbox::Database>& restart_db) const;
  void getFromRestart();
  
  bool initLevel(
    const std::shared_ptr<hier::PatchHierarchy>& hierarchy, idx_t ln);
  void addBSSNExtras(
    const std::shared_ptr<hier::PatchLevel> & level);
  void addBSSNExtras(
    const std::shared_ptr<hier::Patch> & patch);
  void RKEvolveLevel(
    const std::shared_ptr<hier::PatchHierarchy>& hierarchy,
    idx_t ln,
    double from_t,
    double to_t);
  void RKEvolvePatch(
    const std::shared_ptr<hier::Patch> & patch, real_t dt);
  void RKEvolvePatchBD(
    const std::shared_ptr<hier::Patch> & patch, real_t dt);

  
  void advanceLevel(
    const std::shared_ptr<hier::PatchHierarchy>& hierarchy,
    int ln,
    double from_t,
    double to_t);


 private:
  std::shared_ptr<tbox::Database> cosmo_dust_fluid_db;

  /* std::vector<std::shared_ptr<xfer::RefineSchedule>> */
  /*   pre_refine_schedules, post_refine_schedules; */

  /* std::vector<std::shared_ptr<xfer::CoarsenSchedule>> */
  /*   coarsen_schedules; */
  int freeze_fluid_step;
};

} /* namespace cosmo */

#endif
