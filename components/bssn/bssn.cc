#include "bssn.h"
#include "../../cosmo_globals.h"
#include "../../utils/math.h"

namespace cosmo
{

/**
 * @brief Constructor for BSSN class
 * @details Allocate memory for fields, add fields to map,
 * create reference FRW integrator, and call BSSN::init.
 */
BSSN::BSSN(
  const tbox::Dimension& dim_in,
  tbox::Database& database_in,
  std::ostream* l_stream_in = 0):
  lstream(l_stream),
  database(&database),
  barrier_and_time(true),
  dim(dim_in),
  d_adaption_threshold(database.getDoubleWithDefault("adaptation_threshold", 1.0)),
  d_finest_dbg_plot_ln(database.getIntegerWithDefault("finest_dbg_plot_ln", 99)),
  KO_damping_coefficient(database.getDoubleWithDefault("KO_damping_coefficient",0)),
  gd_eta(database.getDoubleWithDefault("gd_eta", 0.0)),
  variable_db(hier::VariableDatabase::getDatabase()),
  gaugeHandler(new BSSNGaugeHandler(config)),
{

  // FRW reference integrator
  //frw = new FRW<real_t> (0.0, 0.0);

  
  
  // BSSN fields
  // BSSN_APPLY_TO_FIELDS(RK4_ARRAY_ALLOC)
  // BSSN_APPLY_TO_FIELDS(RK4_ARRAY_ADDMAP)

1  // // BSSN source fields
  // BSSN_APPLY_TO_SOURCES(GEN1_ARRAY_ALLOC)
  // BSSN_APPLY_TO_SOURCES(GEN1_ARRAY_ADDMAP)

  // // any additional arrays for calcuated quantities
  // BSSN_APPLY_TO_GEN1_EXTRAS(GEN1_ARRAY_ALLOC)
  // BSSN_APPLY_TO_GEN1_EXTRAS(GEN1_ARRAY_ADDMAP)

  init();
}

BSSN::~BSSN()
{
  // BSSN_APPLY_TO_FIELDS(RK4_ARRAY_DELETE)
  // BSSN_APPLY_TO_SOURCES(GEN1_ARRAY_DELETE)
  // BSSN_APPLY_TO_GEN1_EXTRAS(GEN1_ARRAY_DELETE)
}

void BSSN::registerCurrentContext(
  boost::shared_ptr<hier::VariableContext> &context,
  int ghost_width)
{
  BSSN_APPLY_TO_FIELDS_ARGS(BSSN_REG_TO_CONTEXT, context, c, ghost_width);
}

void BSSN::registerPreviousContext(
  boost::shared_ptr<hier::VariableContext> &context,
  int ghost_width)
{
  BSSN_APPLY_TO_FIELDS_ARGS(BSSN_REG_TO_CONTEXT, context, p, ghost_width);
}

void BSSN::registerFinalContext(
  boost::shared_ptr<hier::VariableContext> &context,
  int ghost_width)
{
  BSSN_APPLY_TO_FIELDS_ARGS(BSSN_REG_TO_CONTEXT, context, f, ghost_width);
}

void BSSN::registerActiveContext(
  boost::shared_ptr<hier::VariableContext> &context,
  int ghost_width)
{
  BSSN_APPLY_TO_FIELDS_ARGS(BSSN_REG_TO_CONTEXT, context, a, ghost_width);
}

void BSSN::registerScratchContext(
  boost::shared_ptr<hier::VariableContext> &context,
  int ghost_width)
{
  BSSN_APPLY_TO_FIELDS_ARGS(BSSN_REG_TO_CONTEXT, context, s, ghost_width);
}
  
  
  
/**
 * @brief Normalize the conformal difference metric, make sure the conformal
 * extrinsic curvature is trace-free.
 */
void BSSN::set_DIFFgamma_Aij_norm()
{
  idx_t i, j, k;

  /* This potentially breaks conservation of trace:
   * need to come up with something else to preserve both
   * trace + determinant constraints?
   */
# pragma omp parallel for default(shared) private(i, j, k)
  LOOP3(i, j, k)
  {
    idx_t idx = NP_INDEX(i,j,k);

    // 1 - det(1 + DiffGamma)
    real_t one_minus_det_gamma = -1.0*(
      DIFFgamma11->_array_a[idx] + DIFFgamma22->_array_a[idx] + DIFFgamma33->_array_a[idx]
      - pw2(DIFFgamma12->_array_a[idx]) - pw2(DIFFgamma13->_array_a[idx]) - pw2(DIFFgamma23->_array_a[idx])
      + DIFFgamma11->_array_a[idx]*DIFFgamma22->_array_a[idx] + DIFFgamma11->_array_a[idx]*DIFFgamma33->_array_a[idx] + DIFFgamma22->_array_a[idx]*DIFFgamma33->_array_a[idx]
      - pw2(DIFFgamma23->_array_a[idx])*DIFFgamma11->_array_a[idx] - pw2(DIFFgamma13->_array_a[idx])*DIFFgamma22->_array_a[idx] - pw2(DIFFgamma12->_array_a[idx])*DIFFgamma33->_array_a[idx]
      + 2.0*DIFFgamma12->_array_a[idx]*DIFFgamma13->_array_a[idx]*DIFFgamma23->_array_a[idx] + DIFFgamma11->_array_a[idx]*DIFFgamma22->_array_a[idx]*DIFFgamma33->_array_a[idx]
    );

    // accurately compute 1 - det(g)^(1/3), without roundoff error
    // = -( det(g)^(1/3) - 1 )
    // = -( exp{log[det(g)^(1/3)]} - 1 )
    // = -( expm1{log[det(g)]/3} )
    // = -expm1{log1p[-one_minus_det_gamma]/3.0}
    real_t one_minus_det_gamma_thirdpow = -1.0*expm1(log1p(-1.0*one_minus_det_gamma)/3.0);

    // Perform the equivalent of re-scaling the conformal metric so det(gamma) = 1
    // gamma -> gamma / det(gamma)^(1/3)
    // DIFFgamma -> (delta + DiffGamma) / det(gamma)^(1/3) - delta
    //            = ( DiffGamma + delta*[1 - det(gamma)^(1/3)] ) / ( 1 - [1 - det(1 + DiffGamma)^1/3] )
    DIFFgamma11->_array_a[idx] = (DIFFgamma11->_array_a[idx] + one_minus_det_gamma_thirdpow) / (1.0 - one_minus_det_gamma_thirdpow);
    DIFFgamma22->_array_a[idx] = (DIFFgamma22->_array_a[idx] + one_minus_det_gamma_thirdpow) / (1.0 - one_minus_det_gamma_thirdpow);
    DIFFgamma33->_array_a[idx] = (DIFFgamma33->_array_a[idx] + one_minus_det_gamma_thirdpow) / (1.0 - one_minus_det_gamma_thirdpow);
    DIFFgamma12->_array_a[idx] = (DIFFgamma12->_array_a[idx]) / (1.0 - one_minus_det_gamma_thirdpow);
    DIFFgamma13->_array_a[idx] = (DIFFgamma13->_array_a[idx]) / (1.0 - one_minus_det_gamma_thirdpow);
    DIFFgamma23->_array_a[idx] = (DIFFgamma23->_array_a[idx]) / (1.0 - one_minus_det_gamma_thirdpow);

    // re-scale A_ij / ensure it is trace-free
    // need inverse gamma for finding Tr(A)
    real_t gammai11 = 1.0 + DIFFgamma22->_array_a[idx] + DIFFgamma33->_array_a[idx] - pw2(DIFFgamma23->_array_a[idx]) + DIFFgamma22->_array_a[idx]*DIFFgamma33->_array_a[idx];
    real_t gammai22 = 1.0 + DIFFgamma11->_array_a[idx] + DIFFgamma33->_array_a[idx] - pw2(DIFFgamma13->_array_a[idx]) + DIFFgamma11->_array_a[idx]*DIFFgamma33->_array_a[idx];
    real_t gammai33 = 1.0 + DIFFgamma11->_array_a[idx] + DIFFgamma22->_array_a[idx] - pw2(DIFFgamma12->_array_a[idx]) + DIFFgamma11->_array_a[idx]*DIFFgamma22->_array_a[idx];
    real_t gammai12 = DIFFgamma13->_array_a[idx]*DIFFgamma23->_array_a[idx] - DIFFgamma12->_array_a[idx]*(1.0 + DIFFgamma33->_array_a[idx]);
    real_t gammai13 = DIFFgamma12->_array_a[idx]*DIFFgamma23->_array_a[idx] - DIFFgamma13->_array_a[idx]*(1.0 + DIFFgamma22->_array_a[idx]);
    real_t gammai23 = DIFFgamma12->_array_a[idx]*DIFFgamma13->_array_a[idx] - DIFFgamma23->_array_a[idx]*(1.0 + DIFFgamma11->_array_a[idx]);
    real_t trA = gammai11*A11->_array_a[idx] + gammai22*A22->_array_a[idx] + gammai33*A33->_array_a[idx]
      + 2.0*(gammai12*A12->_array_a[idx] + gammai13*A13->_array_a[idx] + gammai23*A23->_array_a[idx]);
    // A_ij -> ( A_ij - 1/3 gamma_ij A )
    A11->_array_a[idx] = ( A11->_array_a[idx] - 1.0/3.0*(1.0 + DIFFgamma11->_array_a[idx])*trA ) / (1.0 - one_minus_det_gamma_thirdpow);
    A22->_array_a[idx] = ( A22->_array_a[idx] - 1.0/3.0*(1.0 + DIFFgamma22->_array_a[idx])*trA ) / (1.0 - one_minus_det_gamma_thirdpow);
    A33->_array_a[idx] = ( A33->_array_a[idx] - 1.0/3.0*(1.0 + DIFFgamma33->_array_a[idx])*trA ) / (1.0 - one_minus_det_gamma_thirdpow);
    A12->_array_a[idx] = ( A12->_array_a[idx] - 1.0/3.0*DIFFgamma12->_array_a[idx]*trA ) / (1.0 - one_minus_det_gamma_thirdpow);
    A13->_array_a[idx] = ( A13->_array_a[idx] - 1.0/3.0*DIFFgamma13->_array_a[idx]*trA ) / (1.0 - one_minus_det_gamma_thirdpow);
    A23->_array_a[idx] = ( A23->_array_a[idx] - 1.0/3.0*DIFFgamma23->_array_a[idx]*trA ) / (1.0 - one_minus_det_gamma_thirdpow);
  }
}

/**
 * @brief Initialize fields in BSSN class to defaults
 * @details BSSN fields initialized to a flat (difference) metric with zero source;
 * thus all fields in all registers are zeroed. Reference integrator unaffected.
 */
void BSSN::init()
{
  idx_t idx;

  idx_t i, j, k;
  LOOP3(i, j, k)
  {
    idx = NP_INDEX(i,j,k);

    // default flat static vacuum spacetime.
    BSSN_ZERO_ARRAYS(_p, idx)
    BSSN_ZERO_ARRAYS(_a, idx)
    BSSN_ZERO_ARRAYS(_c, idx)
    BSSN_ZERO_ARRAYS(_f, idx)

    BSSN_ZERO_GEN1_EXTRAS()
    BSSN_ZERO_SOURCES()
  }
}

/**
 * @brief Set integration timestep (eg, raytracing code calls this with negative dt when beginning to integrate backwards)
 * 
 * @param dt new timesetp
 */
void BSSN::setDt(real_t dt)
{
  BSSN_SET_DT(dt);
}

/**
 * @brief Set Kriess-Oliger damping coefficient (numerical dissipation strength)
 * Default is zero (no dissipation).
 */
void BSSN::setKODampingCoefficient(real_t coefficient)
{
  KO_damping_coefficient = coefficient;
}

/**
 * @brief Call RK4Register class step initialization; normalize Aij and DIFFgammaIJ fields
 * @details See RK4Register::stepInit() method.
 */
void BSSN::stepInit(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy)
{
  BSSN_RK_INITIALIZE; // macro calls stepInit for all fields

  boost::shared_ptr<geom::CartesianGridGeometry> grid_geometry_(
    BOOST_CAST<geom::CartesianGridGeometry, hier::BaseGridGeometry>(
      hierarchy->getGridGeometry()));

  TBOX_ASSERT(grid_geometry_);

  geom::CartesianGridGeometry& grid_geometry = *grid_geometry_;

  space_refine_op =
       grid_geometry.
       lookupRefineOperator(chi, "CONSERVATIVE_LINEAR_REFINE");

  time_refine_op =
    grid_geometry.
    lookupTimeInterpolateOperator(chi, "STD_LINEAR_TIME_INTERPOLATE");

  space_coarsen_op =
      grid_geometry.
      lookupCoarsenOperator(chi, "CONSERVATIVE_COARSEN");


  
// # if NORMALIZE_GAMMAIJ_AIJ
//     set_DIFFgamma_Aij_norm(); // norms _a register
// # endif
}

/**
 * @brief Call BSSN::RKEvolvePt for all points
 */
void BSSN::RKEvolvePatch(const boost::shared_ptr<hier::Patch> & patch)
{
  const hier::Box& box = patch->getBox();

  const int * lower = &box.lower()[0];
  const int * upper = &box.upper()[0];

  BSSNData bd = {0};
  
  for(int k = lower[2]; k <= upper[2]; k++)
  {
    for(int j = lower[1]; j <= upper[1]; j++)
    {
      for(int i = lower[0]; i <= upper[0]; i++)
      {
        set_bd_values(i, j, k, &bd);
        BSSN_RK_EVOLVE_PT;
      }
    }
  }
}

void BSSN::setLevelTime(
  const boost::shared_ptr<hier::PatchLevel> & level,
  double from_t, double to_t)
{
  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;
    BSSN_APPLY_TO_FIELDS_ARGS(BSSN_SET_PATCH_TIME, from_t, to_t);
  }
}
  
void BSSN::RKEvolveLevel(
  const boost::shared_ptr<hier::PatchLevel> & level,
  double from_t,
  double to_t)
{
  xfer::RefineAlgorithm refiner;
  boost::shared_ptr<xfer::RefineSchedule> refine_schedule;
  
  BSSN_APPLY_TO_FIELDS_ARGS(BSSN_REGISTER_SAME_LEVEL_REFINE_A,refiner,space_refine_op);

  refine_schedule = refiner.createSchedule(level, NULL);
  
  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;
    BSSN_APPLY_TO_FIELDS(BSSN_PDATA_ALL_INIT);
    BSSN_APPLY_TO_FIELDS(BSSN_MDA_ACCESS_ALL_INIT);
    
    RKEvolvePatch(patch);
    K1FinalizePatch(patch, to_t - from_t);  
  }

  level->getBoxLevel()->getMPI().Barrier();
  refine_schedule->fillData(to_t);

  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;
    BSSN_APPLY_TO_FIELDS(BSSN_PDATA_ALL_INIT);
    BSSN_APPLY_TO_FIELDS(BSSN_MDA_ACCESS_ALL_INIT);
    
    RKEvolvePatch(patch);
    K2FinalizePatch(patch, to_t - from_t);
  }

  
  level->getBoxLevel()->getMPI().Barrier();
  refine_schedule->fillData(to_t);

  
  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;
    BSSN_APPLY_TO_FIELDS(BSSN_PDATA_ALL_INIT);
    BSSN_APPLY_TO_FIELDS(BSSN_MDA_ACCESS_ALL_INIT);

    RKEvolvePatch(patch);
    K3FinalizePatch(patch, to_t - from_t);
    
  }

  level->getBoxLevel()->getMPI().Barrier();
  refine_schedule->fillData(to_t);

  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;
    BSSN_APPLY_TO_FIELDS(BSSN_PDATA_ALL_INIT);
    BSSN_APPLY_TO_FIELDS(BSSN_MDA_ACCESS_ALL_INIT);
    
    RKEvolvePatch(patch);
    K4FinalizePatch(patch, to_t - from_t);  
  }

  BSSN_APPLY_TO_FIELDS_ARGS(BSSN_REGISTER_SAME_LEVEL_REFINE_F,refiner,space_refine_op);

  refine_schedule = refiner.createSchedule(level, NULL);

  level->getBoxLevel()->getMPI().Barrier();
  refine_schedule->fillData(to_t);

}
void BSSN::K1FinalizePatch(
  const boost::shared_ptr<hier::Patch> & patch, double dt)
{
  const hier::Box& box = patch->getBox();

  const int * lower = &box.lower()[0];
  const int * upper = &box.upper()[0];

  
  for(int k = lower[2]; k <= upper[2]; k++)
  {
    for(int j = lower[1]; j <= upper[1]; j++)
    {
      for(int i = lower[0]; i <= upper[0]; i++)
      {
        BSSN_FINALIZE_K(1);
      }
    }
  }
}

void BSSN::K2FinalizePatch(
  const boost::shared_ptr<hier::Patch> & patch, double dt)
{
  const hier::Box& box = patch->getBox();

  const int * lower = &box.lower()[0];
  const int * upper = &box.upper()[0];

  
  for(int k = lower[2]; k <= upper[2]; k++)
  {
    for(int j = lower[1]; j <= upper[1]; j++)
    {
      for(int i = lower[0]; i <= upper[0]; i++)
      {
        BSSN_FINALIZE_K(2);
      }
    }
  }
}


void BSSN::K3FinalizePatch(
  const boost::shared_ptr<hier::Patch> & patch, double dt)
{
  const hier::Box& box = patch->getBox();

  const int * lower = &box.lower()[0];
  const int * upper = &box.upper()[0];

  
  for(int k = lower[2]; k <= upper[2]; k++)
  {
    for(int j = lower[1]; j <= upper[1]; j++)
    {
      for(int i = lower[0]; i <= upper[0]; i++)
      {
        BSSN_FINALIZE_K(3);
      }
    }
  }
}


void BSSN::K4FinalizePatch(
  const boost::shared_ptr<hier::Patch> & patch, double dt)
{
  const hier::Box& box = patch->getBox();

  const int * lower = &box.lower()[0];
  const int * upper = &box.upper()[0];

  
  for(int k = lower[2]; k <= upper[2]; k++)
  {
    for(int j = lower[1]; j <= upper[1]; j++)
    {
      for(int i = lower[0]; i <= upper[0]; i++)
      {
        BSSN_FINALIZE_K(4);
      }
    }
  }
}

/**
 * @brief zero all BSSN source term fields
 */
void BSSN::clearSrc()
{
  idx_t i, j, k;
# pragma omp parallel for default(shared) private(i, j, k)
  LOOP3(i, j, k)
  {
    idx_t idx = NP_INDEX(i,j,k);
    BSSN_ZERO_SOURCES()
  }
}


  
void BSSN::advanceLevel(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy,
  int ln,
  double from_t,
  double to_t)
{
  if( ln >= hierarchy->getNumberOfLevels())
    return;
  
  double dt = to_t - from_t;
  const boost::shared_ptr<hier::PatchLevel> level(
    hierarchy->getPatchLevel(ln));

  //RK advance interior(including innner ghost cells) of level
  RKEvolveLevel(level, from_t, to_t);

  setLevelTime(level, from_t, to_t);
  
  if(ln > 0)
  {
    boost::shared_ptr<xfer::PatchLevelBorderFillPattern> border_fill_pattern (
      new xfer::PatchLevelBorderFillPattern());

    xfer::RefineAlgorithm refiner;
    boost::shared_ptr<xfer::RefineSchedule> refine_schedule;
  
    BSSN_APPLY_TO_FIELDS_ARGS(BSSN_REGISTER_INTER_LEVEL_REFINE_F,refiner,space_refine_op,time_refine_op);

    refine_schedule = refiner.createSchedule(
      border_fill_pattern,
      level,
      level,
      ln-1
      hierarchy,
      NULL, TRUE, NULL);

    level->getBoxLevel()->getMPI().Barrier();
    refine_schedule->fillData(to_t);

  }
  
  advanceLevel(hierarchy, ln+1, from_t, from_t + (to_t - from_t)/2.0);

  advanceLevel(hierarchy, ln+1, from_t + (to_t - from_t)/2.0, to_t);


  if(ln < hierarchy->getNumberOfLevels() -1 )
  {
    xfer::CoarsenAlgorithm coarsener(dim);
  

    boost::shared_ptr<xfer::CoarsenSchedule> coarsen_schedule;

    BSSN_APPLY_TO_FIELDS_ARGS(BSSN_REGISTER_COARSEN_F, coarsener, coarsen_op);
    
      
    coarsen_schedule = coarsener.createSchedule(level, hierarchy->getPatchLevel(ln+1));
    level->getBoxLevel()->getMPI().Barrier();
    coarsen_schedule->coarsenData();

    xfer::RefineAlgorithm post_refiner;

    boost::shared_ptr<xfer::RefineSchedule> refine_schedule;

    
    BSSN_APPLY_TO_FIELDS_ARGS(BSSN_REGISTER_SAME_LEVEL_REFINE_F,post_refiner,space_refine_op);

    refine_schedule = post_refiner.createSchedule(level, NULL);

    level->getBoxLevel()->getMPI().Barrier();
    refine_schedule->fillData(to_t);
  }

  math::PatchCellDataOpsReal<double> pcellmath;


  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;
    BSSN_APPLY_TO_FIELDS_ARGS(BSSN_SWAP,pcellmath);
  }  

}
  
/**
 * @brief Call Perform a full RK4 step, minus initialization.
 * @details Calls:
 * BSSN::RKEvolve, BSSN::K1Finalize, BSSN::RKEvolve, BSSN::K2Finalize,
 * BSSN::RKEvolve, BSSN::K3Finalize, BSSN::RKEvolve, BSSN::K4Finalize
 */
void BSSN::step(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy, double from_t, double to_t)
{
  // stepInit must still be called
  // see RK4 pdf in docs

  t_advance_hier->start();

  advanceLevel(hierarchy,
               0,
               from_t,
               to_t);
  t_advance_hier->stop();

}


/*
******************************************************************************

 Calculate independent quantities for later use
(minimize # times derivatives are computed, etc)

******************************************************************************
*/


/**
 * @brief Populate values in a BSSNData struct
 * @details Compute all of them, except full metric m (TODO)
 * 
 * @param i x-index
 * @param j y-index
 * @param k z-index
 * @param bd BSSNData struct to populate
 */
void BSSN::set_bd_values(idx_t i, idx_t j, idx_t k, BSSNData *bd)
{
  bd->i = i;
  bd->j = j;
  bd->k = k;
  
  // need to set FRW quantities first

  //Have not figured out the initial value of FRW 
  
  bd->chi_FRW = ;
  bd->K_FRW = ;
  bd->rho_FRW = ;
  bd->S_FRW = ;
  
  // draw data from cache
  set_local_vals(bd);
  set_gammai_values(i, j, k, bd);

  // non-DIFF quantities
  bd->chi      =   bd->DIFFchi + bd->chi_FRW;
  bd->K        =   bd->DIFFK + bd->K_FRW;
  bd->gamma11  =   bd->DIFFgamma11 + 1.0;
  bd->gamma12  =   bd->DIFFgamma12;
  bd->gamma13  =   bd->DIFFgamma13;
  bd->gamma22  =   bd->DIFFgamma22 + 1.0;
  bd->gamma23  =   bd->DIFFgamma23;
  bd->gamma33  =   bd->DIFFgamma33 + 1.0;
  bd->r        =   bd->DIFFr + bd->rho_FRW;
  bd->S        =   bd->DIFFS + bd->S_FRW;
  bd->alpha    =   bd->DIFFalpha + 1.0;
  
  // pre-compute re-used quantities
  // gammas & derivs first
  calculate_Acont(bd);
  calculate_dgamma(bd);
  calculate_ddgamma(bd);
  calculate_dalpha_dphi(bd);
  calculate_dK(bd);
# if USE_Z4c_DAMPING
    calculate_dtheta(bd);
# endif
# if USE_BSSN_SHIFT
    calculate_dbeta(bd);
    calculate_dexpN(bd);
# endif

  // Christoffels depend on metric & derivs.
  calculate_conformal_christoffels(bd);
  // DDw depend on christoffels, metric, and derivs
  calculateDDphi(bd);
  calculateDDalphaTF(bd);
  // Ricci depends on DDphi
  calculateRicciTF(bd);

  // Hamiltonian constraint
  bd->H = hamiltonianConstraintCalc(bd);
}


/**
 * @brief Set "local values"; set BSSNData values corresponding to field
 * values at a point.
 *
 * @param      bd    BSSNData struct with idx set.
 */
void BSSN::set_local_vals(BSSNData *bd)
{
  BSSN_APPLY_TO_FIELDS(RK4_SET_LOCAL_VALUES);
  BSSN_APPLY_TO_GEN1_EXTRAS(GEN1_SET_LOCAL_VALUES);
  BSSN_APPLY_TO_SOURCES(GEN1_SET_LOCAL_VALUES);
}

/**
 * @brief Compute and store inverse conformal difference metric components given
 * the conformal difference metric in a BSSNData struct
 * @details Computed assuming \f$det(\bar{\gamma}_{ij}) = 1\f$
 * 
 * @param i x-index
 * @param j y-index
 * @param k z-index
 * @param bd BSSNData containing initialized conformal difference metric components
 */
void BSSN::set_gammai_values(idx_t i, idx_t j, idx_t k, BSSNData *bd)
{
  bd->gammai11 = 1.0 + bd->DIFFgamma22 + bd->DIFFgamma33 - pw2(bd->DIFFgamma23) + bd->DIFFgamma22*bd->DIFFgamma33;
  bd->gammai22 = 1.0 + bd->DIFFgamma11 + bd->DIFFgamma33 - pw2(bd->DIFFgamma13) + bd->DIFFgamma11*bd->DIFFgamma33;
  bd->gammai33 = 1.0 + bd->DIFFgamma11 + bd->DIFFgamma22 - pw2(bd->DIFFgamma12) + bd->DIFFgamma11*bd->DIFFgamma22;
  bd->gammai12 = bd->DIFFgamma13*bd->DIFFgamma23 - bd->DIFFgamma12*(1.0 + bd->DIFFgamma33);
  bd->gammai13 = bd->DIFFgamma12*bd->DIFFgamma23 - bd->DIFFgamma13*(1.0 + bd->DIFFgamma22);
  bd->gammai23 = bd->DIFFgamma12*bd->DIFFgamma13 - bd->DIFFgamma23*(1.0 + bd->DIFFgamma11);
}

/**
 * @brief Calculate contravariant version of conformal trace-free extrinsic
 * curvature, \f$\bar{A}^{ij}\f$.
 *
 * @param bd BSSNData struct with inverse metric, Aij already computed.
 */
void BSSN::calculate_Acont(BSSNData *bd)
{
  // A^ij is calculated from A_ij by raising wrt. the conformal metric
  BSSN_APPLY_TO_IJ_PERMS(BSSN_CALCULATE_ACONT)

  // calculate A_ij A^ij term
  AijAij_a(bd->i,bd->j,bd->k) = bd->Acont11*bd->A11 + bd->Acont22*bd->A22 + bd->Acont33*bd->A33
      + 2.0*(bd->Acont12*bd->A12 + bd->Acont13*bd->A13 + bd->Acont23*bd->A23);
  bd->AijAij = AijAij_a(bd->i,bd->j,bd->k);
}

/**
 * @brief Compute partial derivatives of the conformal metric, store in a
 * BSSNData instance
 *
 * @param bd BSSNData struct reference
 */
void BSSN::calculate_dgamma(BSSNData *bd)
{
  BSSN_APPLY_TO_IJK_PERMS(BSSN_CALCULATE_DGAMMA)
}

/**
 * @brief Compute second partial derivatives of the conformal metric, store in
 * a BSSNData instance
 *
 * @param bd BSSNData struct reference
 */
void BSSN::calculate_ddgamma(BSSNData *bd)
{
  BSSN_APPLY_TO_IJ_PERMS(BSSN_CALCULATE_DIDJGAMMA_PERMS)
}

/**
 * @brief Compute partial derivatives of the lapse and conformal factor, store
 * in a BSSNData instance
 *
 * @param bd BSSNData struct reference
 */
void BSSN::calculate_dalpha_dphi(BSSNData *bd)
{
  // normal derivatives of phi
  bd->d1phi = derivative(bd->i, bd->j, bd->k, 1, DIFFphi##_a);
  bd->d2phi = derivative(bd->i, bd->j, bd->k, 2, DIFFphi##_a);
  bd->d3phi = derivative(bd->i, bd->j, bd->k, 3, DIFFphi##_a);

  // second derivatives of phi
  bd->d1d1phi = double_derivative(bd->i, bd->j, bd->k, 1, 1, DIFFphi##_a);
  bd->d2d2phi = double_derivative(bd->i, bd->j, bd->k, 2, 2, DIFFphi##_a);
  bd->d3d3phi = double_derivative(bd->i, bd->j, bd->k, 3, 3, DIFFphi##_a);
  bd->d1d2phi = double_derivative(bd->i, bd->j, bd->k, 1, 2, DIFFphi##_a);
  bd->d1d3phi = double_derivative(bd->i, bd->j, bd->k, 1, 3, DIFFphi##_a);
  bd->d2d3phi = double_derivative(bd->i, bd->j, bd->k, 2, 3, DIFFphi##_a);

  // normal derivatives of alpha
  bd->d1a = derivative(bd->i, bd->j, bd->k, 1, DIFFalpha##_a);
  bd->d2a = derivative(bd->i, bd->j, bd->k, 2, DIFFalpha##_a);
  bd->d3a = derivative(bd->i, bd->j, bd->k, 3, DIFFalpha##_a);
}

/**
 * @brief Compute partial derivatives of the trace of the extrinsic curvature,
 * store in a BSSNData instance
 *
 * @param bd BSSNData struct reference
 */
void BSSN::calculate_dK(BSSNData *bd)
{
  // normal derivatives of K
  bd->d1K = derivative(bd->i, bd->j, bd->k, 1, DIFFK##_a);
  bd->d2K = derivative(bd->i, bd->j, bd->k, 2, DIFFK##_a);
  bd->d3K = derivative(bd->i, bd->j, bd->k, 3, DIFFK##_a);
}

#if USE_Z4c_DAMPING
void BSSN::calculate_dtheta(BSSNData *bd)
{
  // normal derivatives of phi
  bd->d1theta = derivative(bd->i, bd->j, bd->k, 1, theta##_a);
  bd->d2theta = derivative(bd->i, bd->j, bd->k, 2, theta##_a);
  bd->d3theta = derivative(bd->i, bd->j, bd->k, 3, theta##_a);
}
#endif

#if USE_BSSN_SHIFT
void BSSN::calculate_dbeta(BSSNData *bd)
{
  bd->d1beta1 = derivative(bd->i, bd->j, bd->k, 1, beta1##_a);
  bd->d1beta2 = derivative(bd->i, bd->j, bd->k, 1, beta2##_a);
  bd->d1beta3 = derivative(bd->i, bd->j, bd->k, 1, beta3##_a);
  bd->d2beta1 = derivative(bd->i, bd->j, bd->k, 2, beta1##_a);
  bd->d2beta2 = derivative(bd->i, bd->j, bd->k, 2, beta2##_a);
  bd->d2beta3 = derivative(bd->i, bd->j, bd->k, 2, beta3##_a);
  bd->d3beta1 = derivative(bd->i, bd->j, bd->k, 3, beta1##_a);
  bd->d3beta2 = derivative(bd->i, bd->j, bd->k, 3, beta2##_a);
  bd->d3beta3 = derivative(bd->i, bd->j, bd->k, 3, beta3##_a);
}
void BSSN::calculate_dexpN(BSSNData *bd)
{
  bd->d1expN = upwind_derivative(bd->i, bd->j, bd->k, 1, expN##_a, bd->beta1);
  bd->d2expN = upwind_derivative(bd->i, bd->j, bd->k, 2, expN##_a, bd->beta2);
  bd->d3expN = upwind_derivative(bd->i, bd->j, bd->k, 3, expN##_a, bd->beta3);
}
#endif


/*
******************************************************************************

Compute "dependent" quantities (depend on previously calc'd vals)

******************************************************************************
*/

void BSSN::calculate_conformal_christoffels(BSSNData *bd)
{
  // christoffel symbols: \Gamma^i_{jk} = Gijk
  BSSN_APPLY_TO_IJK_PERMS(BSSN_CALCULATE_CHRISTOFFEL)
  // "lowered" christoffel symbols: \Gamma_{ijk} = GLijk
  BSSN_APPLY_TO_IJK_PERMS(BSSN_CALCULATE_CHRISTOFFEL_LOWER)

  bd->Gammad1 = bd->G111*bd->gammai11 + bd->G122*bd->gammai22 + bd->G133*bd->gammai33
    + 2.0*(bd->G112*bd->gammai12 + bd->G113*bd->gammai13 + bd->G123*bd->gammai23);
  bd->Gammad2 = bd->G211*bd->gammai11 + bd->G222*bd->gammai22 + bd->G233*bd->gammai33
    + 2.0*(bd->G212*bd->gammai12 + bd->G213*bd->gammai13 + bd->G223*bd->gammai23);
  bd->Gammad3 = bd->G311*bd->gammai11 + bd->G322*bd->gammai22 + bd->G333*bd->gammai33
    + 2.0*(bd->G312*bd->gammai12 + bd->G313*bd->gammai13 + bd->G323*bd->gammai23);
}

void BSSN::calculateDDphi(BSSNData *bd)
{
  // double covariant derivatives, using unitary metric
  bd->D1D1phi = bd->d1d1phi - (bd->G111*bd->d1phi + bd->G211*bd->d2phi + bd->G311*bd->d3phi);
  bd->D2D2phi = bd->d2d2phi - (bd->G122*bd->d1phi + bd->G222*bd->d2phi + bd->G322*bd->d3phi);
  bd->D3D3phi = bd->d3d3phi - (bd->G133*bd->d1phi + bd->G233*bd->d2phi + bd->G333*bd->d3phi);

  bd->D1D2phi = bd->d1d2phi - (bd->G112*bd->d1phi + bd->G212*bd->d2phi + bd->G312*bd->d3phi);
  bd->D1D3phi = bd->d1d3phi - (bd->G113*bd->d1phi + bd->G213*bd->d2phi + bd->G313*bd->d3phi);
  bd->D2D3phi = bd->d2d3phi - (bd->G123*bd->d1phi + bd->G223*bd->d2phi + bd->G323*bd->d3phi);  
}

void BSSN::calculateDDalphaTF(BSSNData *bd)
{
  // double covariant derivatives - use non-unitary metric - extra pieces that depend on phi!
  // the gammaIldlphi are needed for the BSSN_CALCULATE_DIDJALPHA macro
  real_t gammai1ldlphi = bd->gammai11*bd->d1phi + bd->gammai12*bd->d2phi + bd->gammai13*bd->d3phi;
  real_t gammai2ldlphi = bd->gammai21*bd->d1phi + bd->gammai22*bd->d2phi + bd->gammai23*bd->d3phi;
  real_t gammai3ldlphi = bd->gammai31*bd->d1phi + bd->gammai32*bd->d2phi + bd->gammai33*bd->d3phi;
  // Calculates full (not trace-free) piece:
  BSSN_APPLY_TO_IJ_PERMS(BSSN_CALCULATE_DIDJALPHA)

  // subtract trace (traced with full spatial metric but subtracted later)
  bd->DDaTR = bd->gammai11*bd->D1D1aTF + bd->gammai22*bd->D2D2aTF + bd->gammai33*bd->D3D3aTF
      + 2.0*(bd->gammai12*bd->D1D2aTF + bd->gammai13*bd->D1D3aTF + bd->gammai23*bd->D2D3aTF);
  bd->D1D1aTF -= (1.0/3.0)*bd->gamma11*bd->DDaTR;
  bd->D1D2aTF -= (1.0/3.0)*bd->gamma12*bd->DDaTR;
  bd->D1D3aTF -= (1.0/3.0)*bd->gamma13*bd->DDaTR;
  bd->D2D2aTF -= (1.0/3.0)*bd->gamma22*bd->DDaTR;
  bd->D2D3aTF -= (1.0/3.0)*bd->gamma23*bd->DDaTR;
  bd->D3D3aTF -= (1.0/3.0)*bd->gamma33*bd->DDaTR;

  // scale trace back (=> contracted with "real" metric)
  bd->DDaTR *= exp(-4.0*bd->phi);
}

/* Calculate trace-free ricci tensor components */
void BSSN::calculateRicciTF(BSSNData *bd)
{
  // unitary pieces
  BSSN_APPLY_TO_IJ_PERMS(BSSN_CALCULATE_RICCI_UNITARY)

  /* calculate unitary Ricci scalar. */
  bd->unitRicci = bd->Uricci11*bd->gammai11 + bd->Uricci22*bd->gammai22 + bd->Uricci33*bd->gammai33
            + 2.0*(bd->Uricci12*bd->gammai12 + bd->Uricci13*bd->gammai13 + bd->Uricci23*bd->gammai23);

  /* Phi- contribution */
# if EXCLUDE_SECOND_ORDER_FRW
  real_t expression = (
    bd->gammai11*bd->D1D1phi + bd->gammai22*bd->D2D2phi + bd->gammai33*bd->D3D3phi
    + 2.0*( bd->gammai12*bd->D1D2phi + bd->gammai13*bd->D1D3phi + bd->gammai23*bd->D2D3phi )
  );
  bd->ricci11 = bd->Uricci11 - 2.0*( bd->D1D1phi + bd->gamma11*(expression) );
  bd->ricci12 = bd->Uricci12 - 2.0*( bd->D1D2phi + bd->gamma12*(expression) );
  bd->ricci13 = bd->Uricci13 - 2.0*( bd->D1D3phi + bd->gamma13*(expression) );
  bd->ricci22 = bd->Uricci22 - 2.0*( bd->D2D2phi + bd->gamma22*(expression) );
  bd->ricci23 = bd->Uricci23 - 2.0*( bd->D2D3phi + bd->gamma23*(expression) );
  bd->ricci33 = bd->Uricci33 - 2.0*( bd->D3D3phi + bd->gamma33*(expression) );
# else
  real_t expression = (
    bd->gammai11*(bd->D1D1phi + 2.0*bd->d1phi*bd->d1phi)
    + bd->gammai22*(bd->D2D2phi + 2.0*bd->d2phi*bd->d2phi)
    + bd->gammai33*(bd->D3D3phi + 2.0*bd->d3phi*bd->d3phi)
    + 2.0*(
      bd->gammai12*(bd->D1D2phi + 2.0*bd->d1phi*bd->d2phi)
      + bd->gammai13*(bd->D1D3phi + 2.0*bd->d1phi*bd->d3phi)
      + bd->gammai23*(bd->D2D3phi + 2.0*bd->d2phi*bd->d3phi)
    )
  );

  bd->ricci11 = bd->Uricci11 - 2.0*( bd->D1D1phi - 2.0*bd->d1phi*bd->d1phi + bd->gamma11*(expression) );
  bd->ricci12 = bd->Uricci12 - 2.0*( bd->D1D2phi - 2.0*bd->d1phi*bd->d2phi + bd->gamma12*(expression) );
  bd->ricci13 = bd->Uricci13 - 2.0*( bd->D1D3phi - 2.0*bd->d1phi*bd->d3phi + bd->gamma13*(expression) );
  bd->ricci22 = bd->Uricci22 - 2.0*( bd->D2D2phi - 2.0*bd->d2phi*bd->d2phi + bd->gamma22*(expression) );
  bd->ricci23 = bd->Uricci23 - 2.0*( bd->D2D3phi - 2.0*bd->d2phi*bd->d3phi + bd->gamma23*(expression) );
  bd->ricci33 = bd->Uricci33 - 2.0*( bd->D3D3phi - 2.0*bd->d3phi*bd->d3phi + bd->gamma33*(expression) );
# endif

  /* calculate full Ricci scalar at this point */
  bd->ricci = exp(-4.0*bd->phi)*(
      bd->ricci11*bd->gammai11 + bd->ricci22*bd->gammai22 + bd->ricci33*bd->gammai33
      + 2.0*(bd->ricci12*bd->gammai12 + bd->ricci13*bd->gammai13 + bd->ricci23*bd->gammai23)
    );
  /* store ricci scalar here too. */
  ricci_a(bd->i,bd->j,bd->k) = bd->ricci;

  /* remove trace. Note that \bar{gamma}_{ij}*\bar{gamma}^{kl}R_{kl} = (unbarred gammas). */
  bd->ricciTF11 = bd->ricci11 - (1.0/3.0)*exp(4.0*bd->phi)*bd->gamma11*bd->ricci;
  bd->ricciTF12 = bd->ricci12 - (1.0/3.0)*exp(4.0*bd->phi)*bd->gamma12*bd->ricci;
  bd->ricciTF13 = bd->ricci13 - (1.0/3.0)*exp(4.0*bd->phi)*bd->gamma13*bd->ricci;
  bd->ricciTF22 = bd->ricci22 - (1.0/3.0)*exp(4.0*bd->phi)*bd->gamma22*bd->ricci;
  bd->ricciTF23 = bd->ricci23 - (1.0/3.0)*exp(4.0*bd->phi)*bd->gamma23*bd->ricci;
  bd->ricciTF33 = bd->ricci33 - (1.0/3.0)*exp(4.0*bd->phi)*bd->gamma33*bd->ricci;

  return;
}




/*
******************************************************************************

Evolution equation calculations

******************************************************************************
*/

real_t BSSN::ev_DIFFgamma11(BSSNData *bd) { return BSSN_DT_DIFFGAMMAIJ(1, 1) + 0.5*BS_H_DAMPING_AMPLITUDE*dt*bd->H*bd->DIFFgamma11 - KO_dissipation_Q(bd->i, bd->j, bd->k, DIFFgamma11##_a, KO_damping_coefficient); }
real_t BSSN::ev_DIFFgamma12(BSSNData *bd) { return BSSN_DT_DIFFGAMMAIJ(1, 2) + 0.5*BS_H_DAMPING_AMPLITUDE*dt*bd->H*bd->DIFFgamma12 - KO_dissipation_Q(bd->i, bd->j, bd->k, DIFFgamma12##_a, KO_damping_coefficient); }
real_t BSSN::ev_DIFFgamma13(BSSNData *bd) { return BSSN_DT_DIFFGAMMAIJ(1, 3) + 0.5*BS_H_DAMPING_AMPLITUDE*dt*bd->H*bd->DIFFgamma13 - KO_dissipation_Q(bd->i, bd->j, bd->k, DIFFgamma13##_a, KO_damping_coefficient); }
real_t BSSN::ev_DIFFgamma22(BSSNData *bd) { return BSSN_DT_DIFFGAMMAIJ(2, 2) + 0.5*BS_H_DAMPING_AMPLITUDE*dt*bd->H*bd->DIFFgamma22 - KO_dissipation_Q(bd->i, bd->j, bd->k, DIFFgamma22##_a, KO_damping_coefficient); }
real_t BSSN::ev_DIFFgamma23(BSSNData *bd) { return BSSN_DT_DIFFGAMMAIJ(2, 3) + 0.5*BS_H_DAMPING_AMPLITUDE*dt*bd->H*bd->DIFFgamma23 - KO_dissipation_Q(bd->i, bd->j, bd->k, DIFFgamma23##_a, KO_damping_coefficient); }
real_t BSSN::ev_DIFFgamma33(BSSNData *bd) { return BSSN_DT_DIFFGAMMAIJ(3, 3) + 0.5*BS_H_DAMPING_AMPLITUDE*dt*bd->H*bd->DIFFgamma33 - KO_dissipation_Q(bd->i, bd->j, bd->k, DIFFgamma33##_a, KO_damping_coefficient); }

real_t BSSN::ev_A11(BSSNData *bd) { return BSSN_DT_AIJ(1, 1) - 1.0*BS_H_DAMPING_AMPLITUDE*dt*bd->A11*bd->H - KO_dissipation_Q(bd->i, bd->j, bd->k, A11##_a, KO_damping_coefficient); }
real_t BSSN::ev_A12(BSSNData *bd) { return BSSN_DT_AIJ(1, 2) - 1.0*BS_H_DAMPING_AMPLITUDE*dt*bd->A12*bd->H - KO_dissipation_Q(bd->i, bd->j, bd->k, A12##_a, KO_damping_coefficient); }
real_t BSSN::ev_A13(BSSNData *bd) { return BSSN_DT_AIJ(1, 3) - 1.0*BS_H_DAMPING_AMPLITUDE*dt*bd->A13*bd->H - KO_dissipation_Q(bd->i, bd->j, bd->k, A13##_a, KO_damping_coefficient); }
real_t BSSN::ev_A22(BSSNData *bd) { return BSSN_DT_AIJ(2, 2) - 1.0*BS_H_DAMPING_AMPLITUDE*dt*bd->A22*bd->H - KO_dissipation_Q(bd->i, bd->j, bd->k, A22##_a, KO_damping_coefficient); }
real_t BSSN::ev_A23(BSSNData *bd) { return BSSN_DT_AIJ(2, 3) - 1.0*BS_H_DAMPING_AMPLITUDE*dt*bd->A23*bd->H - KO_dissipation_Q(bd->i, bd->j, bd->k, A23##_a, KO_damping_coefficient); }
real_t BSSN::ev_A33(BSSNData *bd) { return BSSN_DT_AIJ(3, 3) - 1.0*BS_H_DAMPING_AMPLITUDE*dt*bd->A33*bd->H - KO_dissipation_Q(bd->i, bd->j, bd->k, A33##_a, KO_damping_coefficient); }

real_t BSSN::ev_Gamma1(BSSNData *bd) { return BSSN_DT_GAMMAI(1) - KO_dissipation_Q(bd->i, bd->j, bd->k, Gamma1##_a, KO_damping_coefficient); }
real_t BSSN::ev_Gamma2(BSSNData *bd) { return BSSN_DT_GAMMAI(2) - KO_dissipation_Q(bd->i, bd->j, bd->k, Gamma2##_a, KO_damping_coefficient); }
real_t BSSN::ev_Gamma3(BSSNData *bd) { return BSSN_DT_GAMMAI(3) - KO_dissipation_Q(bd->i, bd->j, bd->k, Gamma3##_a, KO_damping_coefficient); }

real_t BSSN::ev_DIFFK(BSSNData *bd)
{
  return (
    - bd->DDaTR
    + bd->alpha*(
#       if EXCLUDE_SECOND_ORDER_FRW
          1.0/3.0*(bd->DIFFK + 2.0*bd->theta)*2.0*bd->K_FRW
#       else
          1.0/3.0*(bd->DIFFK + 2.0*bd->theta)*(bd->DIFFK + 2.0*bd->theta + 2.0*bd->K_FRW)
#       endif

#       if !(EXCLUDE_SECOND_ORDER_SMALL)
          + bd->AijAij
#       endif
    )
    + 4.0*PI*bd->alpha*(bd->r + bd->S)
    + upwind_derivative(bd->i, bd->j, bd->k, 1, DIFFK##_a, bd->beta1)
    + upwind_derivative(bd->i, bd->j, bd->k, 2, DIFFK##_a, bd->beta2)
    + upwind_derivative(bd->i, bd->j, bd->k, 3, DIFFK##_a, bd->beta3)
    - 1.0*JM_K_DAMPING_AMPLITUDE*bd->H*exp(-5.0*bd->phi)
    + Z4c_K1_DAMPING_AMPLITUDE*(1.0 - Z4c_K2_DAMPING_AMPLITUDE)*bd->theta
    - KO_dissipation_Q(bd->i, bd->j, bd->k, DIFFK##_a, KO_damping_coefficient)
  );
}

real_t BSSN::ev_DIFFphi(BSSNData *bd)
{
  return (
    0.1*BS_H_DAMPING_AMPLITUDE*dt*bd->H
    -1.0/6.0*(
      bd->alpha*(bd->DIFFK + 2.0*bd->theta)
      - bd->DIFFalpha*bd->K_FRW
      - ( bd->d1beta1 + bd->d2beta2 + bd->d3beta3 )
    )
    + upwind_derivative(bd->i, bd->j, bd->k, 1, DIFFphi##_a, bd->beta1)
    + upwind_derivative(bd->i, bd->j, bd->k, 2, DIFFphi##_a, bd->beta2)
    + upwind_derivative(bd->i, bd->j, bd->k, 3, DIFFphi##_a, bd->beta3)
    - KO_dissipation_Q(bd->i, bd->j, bd->k, DIFFphi##_a, KO_damping_coefficient)
  );
}

real_t BSSN::ev_DIFFalpha(BSSNData *bd)
{
  return gaugeHandler->ev_lapse(bd)
    #if USE_BSSN_SHIFT
    + upwind_derivative(bd->i, bd->j, bd->k, 1, DIFFalpha##_a, bd->beta1)
    + upwind_derivative(bd->i, bd->j, bd->k, 2, DIFFalpha##_a, bd->beta2)
    + upwind_derivative(bd->i, bd->j, bd->k, 3, DIFFalpha##_a, bd->beta3)
    #endif
    - KO_dissipation_Q(bd->i, bd->j, bd->k, DIFFalpha##_a, KO_damping_coefficient);
}

#if USE_Z4c_DAMPING
real_t BSSN::ev_theta(BSSNData *bd)
{
  return (
    0.5*bd->alpha*(
      bd->ricci + 2.0/3.0*pw2(bd->K + 2.0*bd->theta) - bd->AijAij - 16.0*PI*( bd->r )
    )
    #if USE_BSSN_SHIFT
    + upwind_derivative(bd->i, bd->j, bd->k, 1, theta##_a, bd->beta1)
    + upwind_derivative(bd->i, bd->j, bd->k, 2, theta##_a, bd->beta2)
    + upwind_derivative(bd->i, bd->j, bd->k, 3, theta##_a, bd->beta3)
    #endif
    - bd->alpha*Z4c_K1_DAMPING_AMPLITUDE*(2.0 + Z4c_K2_DAMPING_AMPLITUDE)*bd->theta
  ) - KO_dissipation_Q(bd->i, bd->j, bd->k, theta##_a, KO_damping_coefficient);
}
#endif

#if USE_BSSN_SHIFT
real_t BSSN::ev_beta1(BSSNData *bd)
{
  return gaugeHandler->ev_shift1(bd)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 1, beta1##_a, bd->beta1)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 2, beta1##_a, bd->beta2)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 3, beta1##_a, bd->beta3)
    - KO_dissipation_Q(bd->i, bd->j, bd->k, beta1##_a, KO_damping_coefficient);
}

real_t BSSN::ev_beta2(BSSNData *bd)
{
  return gaugeHandler->ev_shift2(bd)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 1, beta2##_a, bd->beta1)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 2, beta2##_a, bd->beta2)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 3, beta2##_a, bd->beta3)
    - KO_dissipation_Q(bd->i, bd->j, bd->k, beta2##_a, KO_damping_coefficient);
}

real_t BSSN::ev_beta3(BSSNData *bd)
{
  return gaugeHandler->ev_shift3(bd)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 1, beta3##_a, bd->beta1)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 2, beta3##_a, bd->beta2)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 3, beta3##_a, bd->beta3)
    - KO_dissipation_Q(bd->i, bd->j, bd->k, beta3##_a, KO_damping_coefficient);
}

real_t BSSN::ev_expN(BSSNData *bd)
{
  return
      upwind_derivative(bd->i, bd->j, bd->k, 1, expN##_a, bd->beta1)
    + upwind_derivative(bd->i, bd->j, bd->k, 2, expN##_a, bd->beta2)
    + upwind_derivative(bd->i, bd->j, bd->k, 3, expN##_a, bd->beta3)
    -bd->alpha * bd->K/3.0;
}
#endif

#if USE_GAMMA_DRIVER
real_t BSSN::ev_auxB1(BSSNData *bd)
{
  return 0.75*ev_Gamma1(bd)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 1, auxB1##_a, bd->beta1)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 2, auxB1##_a, bd->beta2)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 3, auxB1##_a, bd->beta3)
    - gd_eta * bd->auxB1;
}

real_t BSSN::ev_auxB2(BSSNData *bd)
{
  return 0.75*ev_Gamma2(bd)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 1, auxB2##_a, bd->beta1)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 2, auxB2##_a, bd->beta2)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 3, auxB2##_a, bd->beta3)
    - gd_eta * bd->auxB2;
}

real_t BSSN::ev_auxB3(BSSNData *bd)
{
  return 0.75*ev_Gamma3(bd)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 1, auxB3##_a, bd->beta1)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 2, auxB3##_a, bd->beta2)
    //+ upwind_derivative(bd->i, bd->j, bd->k, 3, auxB3##_a, bd->beta3)
    - gd_eta * bd->auxB3;
}
#endif

/*
******************************************************************************

Constraint violtion calculations

******************************************************************************
*/

void BSSN::setConstraintCalcs(real_t H_values[7], real_t M_values[7],
  real_t G_values[7], real_t A_values[7], real_t S_values[7])
{
  idx_t i, j, k;
  BSSN_INITIALIZE_CONSTRAINT_STAT_VARS(H);
  BSSN_INITIALIZE_CONSTRAINT_STAT_VARS(M);
  BSSN_INITIALIZE_CONSTRAINT_STAT_VARS(G);
  BSSN_INITIALIZE_CONSTRAINT_STAT_VARS(A);
  BSSN_INITIALIZE_CONSTRAINT_STAT_VARS(S);

# pragma omp parallel for default(shared) private(i, j, k) reduction(+:mean_H,\
mean_H_scale,mean_H_scaled,mean_M,mean_M_scale,mean_M_scaled,mean_G,mean_G_scale,\
mean_G_scaled,mean_A,mean_A_scale,mean_A_scaled,mean_S,mean_S_scale,mean_S_scaled)
  LOOP3(i,j,k)
  {
    // populate BSSNData struct
    BSSNData bd = {0};
    set_bd_values(i, j, k, &bd);

    // Hamiltonian constraint
    BSSN_COMPUTE_CONSTRAINT_STAT_VARS(H, hamiltonianConstraintCalc, hamiltonianConstraintScale);
    BSSN_COMPUTE_CONSTRAINT_MEAN_VARS(H);

    // momentum constraint calculations
    BSSN_COMPUTE_CONSTRAINT_STAT_VARS_VEC(M, momentumConstraintCalc, momentumConstraintScale);
    BSSN_COMPUTE_CONSTRAINT_MEAN_VARS(M);

    // Christoffel constraint calculations
    BSSN_COMPUTE_CONSTRAINT_STAT_VARS_VEC(G, christoffelConstraintCalc, christoffelConstraintScale);
    BSSN_COMPUTE_CONSTRAINT_MEAN_VARS(G);

    // Aij trace free constraint calculations
    BSSN_COMPUTE_CONSTRAINT_STAT_VARS(A, AijTFConstraintCalc, AijTFConstraintScale);
    BSSN_COMPUTE_CONSTRAINT_MEAN_VARS(A);

    // unit det metric constraint calculations
    BSSN_COMPUTE_CONSTRAINT_STAT_VARS(S, unitDetConstraintCalc, unitDetConstraintScale);
    BSSN_COMPUTE_CONSTRAINT_MEAN_VARS(S);

    // set max/min values
#   pragma omp critical
    {
      BSSN_COMPUTE_CONSTRAINT_MAXES(H);
      BSSN_COMPUTE_CONSTRAINT_MAXES(M);
      BSSN_COMPUTE_CONSTRAINT_MAXES(G);
      BSSN_COMPUTE_CONSTRAINT_MAXES(A);
      BSSN_COMPUTE_CONSTRAINT_MAXES(S);
    }
  }
  // total -> mean
  BSSN_NORMALIZE_MEAN(H);
  BSSN_NORMALIZE_MEAN(M);
  BSSN_NORMALIZE_MEAN(G);
  BSSN_NORMALIZE_MEAN(A);
  BSSN_NORMALIZE_MEAN(S);

  // stdev relies on mean calcs
# pragma omp parallel for default(shared) private(i, j, k) reduction(+:stdev_H,\
stdev_H_scaled,stdev_M,stdev_M_scaled,stdev_G,stdev_G_scaled,stdev_A,\
stdev_A_scaled,stdev_S,stdev_S_scaled)
  LOOP3(i,j,k)
  {
    // populate BSSNData struct
    BSSNData bd = {0};
    set_bd_values(i, j, k, &bd);

    // Hamiltonian constraint
    BSSN_COMPUTE_CONSTRAINT_STAT_VARS(H, hamiltonianConstraintCalc, hamiltonianConstraintScale);
    BSSN_COMPUTE_CONSTRAINT_STDEV_VARS(H);

    // momentum constraint calculations
    BSSN_COMPUTE_CONSTRAINT_STAT_VARS_VEC(M, momentumConstraintCalc, momentumConstraintScale);
    BSSN_COMPUTE_CONSTRAINT_STDEV_VARS(M);

    // Christoffel constraint calculations
    BSSN_COMPUTE_CONSTRAINT_STAT_VARS_VEC(G, christoffelConstraintCalc, christoffelConstraintScale);
    BSSN_COMPUTE_CONSTRAINT_STDEV_VARS(G);

    // Aij trace free constraint calculations
    BSSN_COMPUTE_CONSTRAINT_STAT_VARS(A, AijTFConstraintCalc, AijTFConstraintScale);
    BSSN_COMPUTE_CONSTRAINT_STDEV_VARS(A);

    // unit det metric constraint calculations
    BSSN_COMPUTE_CONSTRAINT_STAT_VARS(S, unitDetConstraintCalc, unitDetConstraintScale);
    BSSN_COMPUTE_CONSTRAINT_STDEV_VARS(S);
  }
  BSSN_NORMALIZE_STDEV(H);
  BSSN_NORMALIZE_STDEV(M);
  BSSN_NORMALIZE_STDEV(G);
  BSSN_NORMALIZE_STDEV(A);
  BSSN_NORMALIZE_STDEV(S);

  BSSN_STORE_CONSTRAINT_STAT_VARS(H);
  BSSN_STORE_CONSTRAINT_STAT_VARS(M);
  BSSN_STORE_CONSTRAINT_STAT_VARS(G);
  BSSN_STORE_CONSTRAINT_STAT_VARS(A);
  BSSN_STORE_CONSTRAINT_STAT_VARS(S);

  return;
}

real_t BSSN::hamiltonianConstraintCalc(BSSNData *bd)
{
# if USE_Z4c_DAMPING
    real_t theta = bd->theta;
# else
    real_t theta = 0.0;
# endif

    return -exp(5.0*bd->phi)/8.0*(
      bd->ricci + 2.0/3.0*pw2(bd->K + 2.0*theta) - bd->AijAij - 16.0*PI*bd->r
    );
}

real_t BSSN::hamiltonianConstraintScale(BSSNData *bd)
{
# if USE_Z4c_DAMPING
    real_t theta = bd->theta;
# else
    real_t theta = 0.0;
# endif

  // sqrt sum of sq. of terms for appx. mag / scale
  return (exp(5.0*bd->phi)/8.0)*
    sqrt( pw2(bd->ricci) + pw2(bd->AijAij)
          + pw2(2.0/3.0*pw2(bd->K + 2.0*theta))
          + pw2(16.0*PI*bd->r)
    );
}

real_t BSSN::momentumConstraintCalc(BSSNData *bd, idx_t d)
{
  // needs bd vals calc'd first
  switch(d)
  {
    case 1:
      return BSSN_MI(1);
    case 2:
      return BSSN_MI(2);
    case 3:
      return BSSN_MI(3);
  }

  /* xxx */
  throw -1;
  return 0;
}

real_t BSSN::momentumConstraintScale(BSSNData *bd, idx_t d)
{
  // needs bd vals calc'd first
  switch(d)
  {
    case 1:
      return BSSN_MI_SCALE(1);
    case 2:
      return BSSN_MI_SCALE(2);
    case 3:
      return BSSN_MI_SCALE(3);
  }

  /* xxx */
  throw -1;
  return 0;
}

/**
 * @brief Compute statistics about algebraic constraint violation:
 * \bar{\Gamma}^i = \bar{\gamma}^{jk} \bar{\Gamma}^i_{jk}
 */
real_t BSSN::christoffelConstraintCalc(BSSNData *bd, idx_t d)
{
  // needs bd vals calc'd first
  switch(d)
  {
    case 1:
      return BSSN_GI_CALC(1);
    case 2:
      return BSSN_GI_CALC(2);
    case 3:
      return BSSN_GI_CALC(3);
  }

  /* xxx */
  throw -1;
  return 0;
}

real_t BSSN::christoffelConstraintScale(BSSNData *bd, idx_t d)
{
  // needs bd vals calc'd first
  switch(d)
  {
    case 1:
      return BSSN_GI_SCALE(1);
    case 2:
      return BSSN_GI_SCALE(2);
    case 3:
      return BSSN_GI_SCALE(3);
  }

  /* xxx */
  throw -1;
  return 0;
}

/**
 * @brief Compute statistics about algebraic constraint violation:
 * \bar{\gamma}^{ij} \bar{A}_{ij} = 0
 */
real_t BSSN::AijTFConstraintCalc(BSSNData *bd)
{
  return bd->gammai11*bd->A11 + bd->gammai22*bd->A22 + bd->gammai33*bd->A33
    + 2.0*(bd->gammai12*bd->A12 + bd->gammai13*bd->A13 + bd->gammai23*bd->A23);
}

real_t BSSN::AijTFConstraintScale(BSSNData *bd)
{
  return fabs(bd->gammai11*bd->A11) + fabs(bd->gammai22*bd->A22) + fabs(bd->gammai33*bd->A33)
    + 2.0*(fabs(bd->gammai12*bd->A12) + fabs(bd->gammai13*bd->A13) + fabs(bd->gammai23*bd->A23));
}

/**
 * @brief Compute statistics about algebraic constraint violation:
 * det(\bar{\gamma}_{ij}) = 1
 * In terms of reference variables.
 */
real_t BSSN::unitDetConstraintCalc(BSSNData *bd)
{
  return fabs(
    bd->DIFFgamma11 + bd->DIFFgamma22 + bd->DIFFgamma33
    - pw2(bd->DIFFgamma12) - pw2(bd->DIFFgamma13) - pw2(bd->DIFFgamma23)
    + bd->DIFFgamma11*bd->DIFFgamma22 + bd->DIFFgamma11*bd->DIFFgamma33 + bd->DIFFgamma22*bd->DIFFgamma33
    - pw2(bd->DIFFgamma23)*bd->DIFFgamma11 - pw2(bd->DIFFgamma13)*bd->DIFFgamma22 - pw2(bd->DIFFgamma12)*bd->DIFFgamma33
    + 2.0*bd->DIFFgamma12*bd->DIFFgamma13*bd->DIFFgamma23 + bd->DIFFgamma11*bd->DIFFgamma22*bd->DIFFgamma33
  );
}

real_t BSSN::unitDetConstraintScale(BSSNData *bd)
{
  return (
    fabs(bd->DIFFgamma11) + fabs(bd->DIFFgamma22) + fabs(bd->DIFFgamma33)
    + pw2(bd->DIFFgamma12) + pw2(bd->DIFFgamma13) + pw2(bd->DIFFgamma23)
    + fabs(bd->DIFFgamma11*bd->DIFFgamma22) + fabs(bd->DIFFgamma11*bd->DIFFgamma33) + fabs(bd->DIFFgamma22*bd->DIFFgamma33)
    + pw2(bd->DIFFgamma23)*bd->DIFFgamma11 + pw2(bd->DIFFgamma13)*bd->DIFFgamma22 + pw2(bd->DIFFgamma12)*bd->DIFFgamma33
    + 2.0*fabs(bd->DIFFgamma12*bd->DIFFgamma13*bd->DIFFgamma23) + fabs(bd->DIFFgamma11*bd->DIFFgamma22*bd->DIFFgamma33)
  );
}



#if USE_COSMOTRACE
/*
******************************************************************************

Populate a RaytracePrimitives struct with values from a BSSN struct
 (plus derivatives on the A_ij field)

******************************************************************************
*/

void BSSN::setRaytracePrimitives(RayTrace<real_t, idx_t> *rt)
{
  setRaytraceCornerPrimitives(rt);
  rt->interpolatePrimitives();
}

void BSSN::setRaytraceCornerPrimitives(RayTrace<real_t, idx_t> *rt)
{
  BSSNData bd = {0};

  struct RaytracePrimitives<real_t> corner_rp[2][2][2];

  idx_t x_idx = rt->getRayIDX(1, dx, NX);
  idx_t y_idx = rt->getRayIDX(2, dx, NY);
  idx_t z_idx = rt->getRayIDX(3, dx, NZ);

  set_bd_values(x_idx, y_idx, z_idx, &bd);
  corner_rp[0][0][0] = getRaytraceData(&bd);
  set_bd_values(x_idx, y_idx, (z_idx + 1) % NZ, &bd);
  corner_rp[0][0][1] = getRaytraceData(&bd);
  set_bd_values(x_idx, (y_idx + 1) % NY, z_idx, &bd);
  corner_rp[0][1][0] = getRaytraceData(&bd);
  set_bd_values(x_idx, (y_idx + 1) % NY, (z_idx + 1) % NZ, &bd);
  corner_rp[0][1][1] = getRaytraceData(&bd);
  set_bd_values((x_idx + 1) % NX, y_idx, z_idx, &bd);
  corner_rp[1][0][0] = getRaytraceData(&bd);
  set_bd_values((x_idx + 1) % NX, y_idx, (z_idx + 1) % NZ, &bd);
  corner_rp[1][0][1] = getRaytraceData(&bd);
  set_bd_values((x_idx + 1) % NX, (y_idx + 1) % NY, z_idx, &bd);
  corner_rp[1][1][0] = getRaytraceData(&bd);
  set_bd_values((x_idx + 1) % NX, (y_idx + 1) % NY, (z_idx + 1) % NZ, &bd);
  corner_rp[1][1][1] = getRaytraceData(&bd);

  rt->copyInCornerPrimitives(corner_rp);
}

RaytracePrimitives<real_t> BSSN::getRaytraceData(BSSNData *bd)
{
  RaytracePrimitives<real_t> rp = {0};

  // normalization factor
  real_t P = exp(4.0*bd->phi);

  // metric
  rp.g[0] = P*bd->gamma11; rp.g[1] = P*bd->gamma12; rp.g[2] = P*bd->gamma13;
  rp.g[3] = P*bd->gamma22; rp.g[4] = P*bd->gamma23; rp.g[5] = P*bd->gamma33;
  // inverse metric
  rp.gi[0] = bd->gammai11/P; rp.gi[1] = bd->gammai12/P; rp.gi[2] = bd->gammai13/P;
  rp.gi[3] = bd->gammai22/P; rp.gi[4] = bd->gammai23/P; rp.gi[5] = bd->gammai33/P;
  // derivatives of metric
  rp.dg[0][0] = BSSN_RP_DG(1,1,1); rp.dg[0][1] = BSSN_RP_DG(1,2,1); rp.dg[0][2] = BSSN_RP_DG(1,3,1);
  rp.dg[0][3] = BSSN_RP_DG(2,2,1); rp.dg[0][4] = BSSN_RP_DG(2,3,1); rp.dg[0][5] = BSSN_RP_DG(3,3,1);
  rp.dg[1][0] = BSSN_RP_DG(1,1,2); rp.dg[1][1] = BSSN_RP_DG(1,2,2); rp.dg[1][2] = BSSN_RP_DG(1,3,2);
  rp.dg[1][3] = BSSN_RP_DG(2,2,2); rp.dg[1][4] = BSSN_RP_DG(2,3,2); rp.dg[1][5] = BSSN_RP_DG(3,3,2);
  rp.dg[2][0] = BSSN_RP_DG(1,1,3); rp.dg[2][1] = BSSN_RP_DG(1,2,3); rp.dg[2][2] = BSSN_RP_DG(1,3,3);
  rp.dg[2][3] = BSSN_RP_DG(2,2,3); rp.dg[2][4] = BSSN_RP_DG(2,3,3); rp.dg[2][5] = BSSN_RP_DG(3,3,3);
  // second derivatives of metric
  rp.ddg[0][0] = BSSN_RP_DDG(1,1,1,1); rp.ddg[0][1] = BSSN_RP_DDG(1,2,1,1); rp.ddg[0][2] = BSSN_RP_DDG(1,3,1,1);
  rp.ddg[0][3] = BSSN_RP_DDG(2,2,1,1); rp.ddg[0][4] = BSSN_RP_DDG(2,3,1,1); rp.ddg[0][5] = BSSN_RP_DDG(3,3,1,1);
  rp.ddg[1][0] = BSSN_RP_DDG(1,1,1,2); rp.ddg[1][1] = BSSN_RP_DDG(1,2,1,2); rp.ddg[1][2] = BSSN_RP_DDG(1,3,1,2);
  rp.ddg[1][3] = BSSN_RP_DDG(2,2,1,2); rp.ddg[1][4] = BSSN_RP_DDG(2,3,1,2); rp.ddg[1][5] = BSSN_RP_DDG(3,3,1,2);
  rp.ddg[2][0] = BSSN_RP_DDG(1,1,1,3); rp.ddg[2][1] = BSSN_RP_DDG(1,2,1,3); rp.ddg[2][2] = BSSN_RP_DDG(1,3,1,3);
  rp.ddg[2][3] = BSSN_RP_DDG(2,2,1,3); rp.ddg[2][4] = BSSN_RP_DDG(2,3,1,3); rp.ddg[2][5] = BSSN_RP_DDG(3,3,1,3);
  rp.ddg[3][0] = BSSN_RP_DDG(1,1,2,2); rp.ddg[3][1] = BSSN_RP_DDG(1,2,2,2); rp.ddg[3][2] = BSSN_RP_DDG(1,3,2,2);
  rp.ddg[3][3] = BSSN_RP_DDG(2,2,2,2); rp.ddg[3][4] = BSSN_RP_DDG(2,3,2,2); rp.ddg[3][5] = BSSN_RP_DDG(3,3,2,2);
  rp.ddg[4][0] = BSSN_RP_DDG(1,1,2,3); rp.ddg[4][1] = BSSN_RP_DDG(1,2,2,3); rp.ddg[4][2] = BSSN_RP_DDG(1,3,2,3);
  rp.ddg[4][3] = BSSN_RP_DDG(2,2,2,3); rp.ddg[4][4] = BSSN_RP_DDG(2,3,2,3); rp.ddg[4][5] = BSSN_RP_DDG(3,3,2,3);
  rp.ddg[5][0] = BSSN_RP_DDG(1,1,3,3); rp.ddg[5][1] = BSSN_RP_DDG(1,2,3,3); rp.ddg[5][2] = BSSN_RP_DDG(1,3,3,3);
  rp.ddg[5][3] = BSSN_RP_DDG(2,2,3,3); rp.ddg[5][4] = BSSN_RP_DDG(2,3,3,3); rp.ddg[5][5] = BSSN_RP_DDG(3,3,3,3);

  // extrinsic curvature:
  rp.K[0] = BSSN_RP_K(1,1); rp.K[1] = BSSN_RP_K(1,2); rp.K[2] = BSSN_RP_K(1,3);
  rp.K[3] = BSSN_RP_K(2,2); rp.K[4] = BSSN_RP_K(2,3); rp.K[5] = BSSN_RP_K(3,3);
  // derivatives of extrinsic curvature
  rp.dK[0][0] = BSSN_RP_DK(1,1,1); rp.dK[0][1] = BSSN_RP_DK(1,2,1); rp.dK[0][2] = BSSN_RP_DK(1,3,1);
  rp.dK[0][3] = BSSN_RP_DK(2,2,1); rp.dK[0][4] = BSSN_RP_DK(2,3,1); rp.dK[0][5] = BSSN_RP_DK(3,3,1);
  rp.dK[1][0] = BSSN_RP_DK(1,1,2); rp.dK[1][1] = BSSN_RP_DK(1,2,2); rp.dK[1][2] = BSSN_RP_DK(1,3,2);
  rp.dK[1][3] = BSSN_RP_DK(2,2,2); rp.dK[1][4] = BSSN_RP_DK(2,3,2); rp.dK[1][5] = BSSN_RP_DK(3,3,2);
  rp.dK[2][0] = BSSN_RP_DK(1,1,3); rp.dK[2][1] = BSSN_RP_DK(1,2,3); rp.dK[2][2] = BSSN_RP_DK(1,3,3);
  rp.dK[2][3] = BSSN_RP_DK(2,2,3); rp.dK[2][4] = BSSN_RP_DK(2,3,3); rp.dK[2][5] = BSSN_RP_DK(3,3,3);

  // 3-Christoffel symbols
  // raised first index
  rp.G[0][0] = BSSN_RP_GAMMA(1,1,1); rp.G[0][1] = BSSN_RP_GAMMA(1,2,1); rp.G[0][2] = BSSN_RP_GAMMA(1,3,1);
  rp.G[0][3] = BSSN_RP_GAMMA(2,2,1); rp.G[0][4] = BSSN_RP_GAMMA(2,3,1); rp.G[0][5] = BSSN_RP_GAMMA(3,3,1);
  rp.G[1][0] = BSSN_RP_GAMMA(1,1,2); rp.G[1][1] = BSSN_RP_GAMMA(1,2,2); rp.G[1][2] = BSSN_RP_GAMMA(1,3,2);
  rp.G[1][3] = BSSN_RP_GAMMA(2,2,2); rp.G[1][4] = BSSN_RP_GAMMA(2,3,2); rp.G[1][5] = BSSN_RP_GAMMA(3,3,2);
  rp.G[2][0] = BSSN_RP_GAMMA(1,1,3); rp.G[2][1] = BSSN_RP_GAMMA(1,2,3); rp.G[2][2] = BSSN_RP_GAMMA(1,3,3);
  rp.G[2][3] = BSSN_RP_GAMMA(2,2,3); rp.G[2][4] = BSSN_RP_GAMMA(2,3,3); rp.G[2][5] = BSSN_RP_GAMMA(3,3,3);
  // lower first index
  rp.GL[0][0] = BSSN_RP_GAMMAL(1,1,1); rp.GL[0][1] = BSSN_RP_GAMMAL(1,2,1); rp.GL[0][2] = BSSN_RP_GAMMAL(1,3,1);
  rp.GL[0][3] = BSSN_RP_GAMMAL(2,2,1); rp.GL[0][4] = BSSN_RP_GAMMAL(2,3,1); rp.GL[0][5] = BSSN_RP_GAMMAL(3,3,1);
  rp.GL[1][0] = BSSN_RP_GAMMAL(1,1,2); rp.GL[1][1] = BSSN_RP_GAMMAL(1,2,2); rp.GL[1][2] = BSSN_RP_GAMMAL(1,3,2);
  rp.GL[1][3] = BSSN_RP_GAMMAL(2,2,2); rp.GL[1][4] = BSSN_RP_GAMMAL(2,3,2); rp.GL[1][5] = BSSN_RP_GAMMAL(3,3,2);
  rp.GL[2][0] = BSSN_RP_GAMMAL(1,1,3); rp.GL[2][1] = BSSN_RP_GAMMAL(1,2,3); rp.GL[2][2] = BSSN_RP_GAMMAL(1,3,3);
  rp.GL[2][3] = BSSN_RP_GAMMAL(2,2,3); rp.GL[2][4] = BSSN_RP_GAMMAL(2,3,3); rp.GL[2][5] = BSSN_RP_GAMMAL(3,3,3);

  // 3-Ricci tensor
  rp.Ricci[0] = bd->ricci11; rp.Ricci[1] = bd->ricci12;
  rp.Ricci[2] = bd->ricci13; rp.Ricci[3] = bd->ricci22;
  rp.Ricci[4] = bd->ricci23; rp.Ricci[5] = bd->ricci33;

  rp.rho = bd->r;
  rp.trK = bd->K;

  return rp;
}
#endif // if USE_COSMOTRACE

} // namespace cosmo
