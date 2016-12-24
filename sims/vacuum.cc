#include "vacuum.h"
#include "../cosmo_includes.h"
#include "vacuum_macros.h"
#include "../utils/math.h"
#include "SAMRAI/xfer/PatchLevelBorderFillPattern.h"

using namespace SAMRAI;

namespace cosmo
{
  
VacuumSim::VacuumSim(
  const tbox::Dimension& dim_in,
  boost::shared_ptr<tbox::InputDatabase>& input_db_in,
  std::ostream* l_stream_in,
  std::string simulation_type_in,
  std::string vis_filename_in):CosmoSim(
    dim_in, input_db_in, l_stream_in, simulation_type_in, vis_filename_in),
  cosmo_vacuum_db(input_db_in->getDatabase("VacuumSim"))
{

  t_init->start();

  std::string bd_type = cosmo_vacuum_db->getString("boundary_type");

  //TBOX_ASSERT(bd_type);

  
  if(bd_type == "sommerfield")
  {

    cosmoPS = new SommerfieldBD(dim, bd_type);

  }
  else
    TBOX_ERROR("Unsupported boundary type!\n");

  // initialize base class

  cosmo_vacuum_db = input_db->getDatabase("VacuumSim");
    

  tbox::pout<<"Running 'vacuum' type simulation.\n";

  BSSN_APPLY_TO_FIELDS(ADD_VAR_TO_LIST);

  variable_id_list.push_back(weight_idx);

  hier::VariableDatabase::getDatabase()->printClassData(tbox::plog);
  
  t_init->stop();  
}

VacuumSim::~VacuumSim() {
}

  
void VacuumSim::init()
{
  
}

/**
 * @brief      Set vacuum initial conditions
 *
 * @param[in]  map to BSSN fields
 * @param      initialized IOData
 */
void VacuumSim::setICs(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy)
{
  tbox::plog<<"Setting initial conditions (ICs).";

  TBOX_ASSERT(gridding_algorithm);
  
  gridding_algorithm->printClassData(tbox::plog);
  gridding_algorithm->makeCoarsestLevel(0.0);

  
  while(hierarchy->getNumberOfLevels() < hierarchy->getMaxNumberOfLevels())
  {
    int pre_level_num = hierarchy->getNumberOfLevels();
    std::vector<int> tag_buffer(hierarchy->getMaxNumberOfLevels());
    for (idx_t ln = 0; ln < static_cast<int>(tag_buffer.size()); ++ln) {
      tag_buffer[ln] = 0;
    }
    gridding_algorithm->regridAllFinerLevels(
      0,
      tag_buffer,
      0,
      0.0);
    int post_level_num = hierarchy->getNumberOfLevels();
    if(post_level_num == pre_level_num) break;
  }
  
  tbox::plog<<"Finished setting ICs. with hierarchy has "
            <<hierarchy->getNumberOfLevels()<<" levels\n";
}

void VacuumSim::initVacuumStep(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy)
{
  bssnSim->stepInit(hierarchy);
}


void VacuumSim::initLevel(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy,
  idx_t ln)
{
  std::string ic_type = cosmo_vacuum_db->getString("ic_type");

  //TBOX_ASSERT(ic_type);

  boost::shared_ptr<hier::PatchLevel> level(hierarchy->getPatchLevel(ln));
  
  if(ic_type == "static_blackhole")
  {
    math::HierarchyCellDataOpsReal<double> hcellmath(hierarchy, ln, ln);

    BSSN_APPLY_TO_FIELDS_ARGS(RK4_ARRAY_ZERO,hcellmath);
    BSSN_APPLY_TO_SOURCES_ARGS(EXTRA_ARRAY_ZERO,hcellmath);
    BSSN_APPLY_TO_GEN1_EXTRAS_ARGS(EXTRA_ARRAY_ZERO,hcellmath);

    bssn_ic_static_blackhole(hierarchy,ln);
    
  }
  else
    TBOX_ERROR("Undefined IC type!\n");

  // xfer::RefineAlgorithm refiner;
  // boost::shared_ptr<xfer::RefineSchedule> refine_schedule;
  // //BSSN_APPLY_TO_FIELDS_ARGS(VAC_REGISTER_SPACE_REFINE_A,refiner,space_refine_op);

  // refiner.registerRefine(bssnSim->DIFFchi_a_idx,  
  //                        bssnSim->DIFFchi_a_idx,  
  //                        bssnSim->DIFFchi_a_idx,  
  //                        space_refine_op);

  
  // refine_schedule =
  //   refiner.createSchedule(level);

  // tbox::pout<<"starting filling ghost cells...\n";
  // level->getBoxLevel()->getMPI().Barrier();
  // refine_schedule->fillData(0.0);
  // level->getBoxLevel()->getMPI().Barrier();
  // tbox::pout<<"ending filling ghost cells...\n";
}

  
void VacuumSim::computeVectorWeights(
   const boost::shared_ptr<hier::PatchHierarchy>& hierarchy)
{
  TBOX_ASSERT(hierarchy);
  TBOX_ASSERT_DIM_OBJDIM_EQUALITY1(dim, *hierarchy);

  int weight_id = weight_idx;
  int coarsest_ln = 0;
  int finest_ln = hierarchy->getFinestLevelNumber();


  int ln;
  for (ln = finest_ln; ln >= coarsest_ln; --ln) {

    /*
     * On every level, first assign cell volume to vector weight.
     */

    boost::shared_ptr<hier::PatchLevel> level(hierarchy->getPatchLevel(ln));
    for (hier::PatchLevel::iterator p(level->begin());
         p != level->end(); ++p) {
      const boost::shared_ptr<hier::Patch>& patch = *p;
      boost::shared_ptr<geom::CartesianPatchGeometry> patch_geometry(
        BOOST_CAST<geom::CartesianPatchGeometry, hier::PatchGeometry>(
          patch->getPatchGeometry()));

      TBOX_ASSERT(patch_geometry);

      const double* dx = patch_geometry->getDx();
      double cell_vol = dx[0];
      if (dim > tbox::Dimension(1)) {
        cell_vol *= dx[1];
      }

      if (dim > tbox::Dimension(2)) {
        cell_vol *= dx[2];
      }

      boost::shared_ptr<pdat::CellData<double> > w(
        BOOST_CAST<pdat::CellData<double>, hier::PatchData>(
          patch->getPatchData(weight_id)));
      TBOX_ASSERT(w);
      w->fillAll(cell_vol);
    }

    /*
     * On all but the finest level, assign 0 to vector
     * weight to cells covered by finer cells.
     */

    if (ln < finest_ln) {

      /*
       * First get the boxes that describe index space of the next finer
       * level and coarsen them to describe corresponding index space
       * at this level.
       */

      boost::shared_ptr<hier::PatchLevel> next_finer_level(
        hierarchy->getPatchLevel(ln + 1));
      hier::BoxContainer coarsened_boxes = next_finer_level->getBoxes();
      hier::IntVector coarsen_ratio(next_finer_level->getRatioToLevelZero());
      coarsen_ratio /= level->getRatioToLevelZero();
      coarsened_boxes.coarsen(coarsen_ratio);

      /*
       * Then set vector weight to 0 wherever there is
       * a nonempty intersection with the next finer level.
       * Note that all assignments are local.
       */

      for (hier::PatchLevel::iterator p(level->begin());
           p != level->end(); ++p) {

        const boost::shared_ptr<hier::Patch>& patch = *p;
        for (hier::BoxContainer::iterator i = coarsened_boxes.begin();
             i != coarsened_boxes.end(); ++i) {

          hier::Box intersection = *i * (patch->getBox());
          if (!intersection.empty()) {
            boost::shared_ptr<pdat::CellData<double> > w(
              BOOST_CAST<pdat::CellData<double>, hier::PatchData>(
                patch->getPatchData(weight_id)));
            TBOX_ASSERT(w);
            w->fillAll(0.0, intersection);

          }  // assignment only in non-empty intersection
        }  // loop over coarsened boxes from finer level
      }  // loop over patches in level
    }  // all levels except finest
  }  // loop over levels
  
}

  
void VacuumSim::initializeLevelData(
   /*! Hierarchy to initialize */
   const boost::shared_ptr<hier::PatchHierarchy>& hierarchy,
   /*! Level to initialize */
   const int ln,
   const double init_data_time,
   const bool can_be_refined,
   /*! Whether level is being introduced for the first time */
   const bool initial_time,
   /*! Level to copy data from */
   const boost::shared_ptr<hier::PatchLevel>& old_level,
   const bool allocate_data)
{
   NULL_USE(init_data_time);
   NULL_USE(can_be_refined);
   NULL_USE(initial_time);

   boost::shared_ptr<hier::PatchHierarchy> patch_hierarchy(hierarchy);

   /*
    * Reference the level object with the given index from the hierarchy.
    */
   boost::shared_ptr<hier::PatchLevel> level(hierarchy->getPatchLevel(ln));

   /*
    * If instructed, allocate all patch data on the level.
    * Allocate only persistent data.  Scratch data will
    * generally be allocated and deallocated as needed.
    */
   

   math::HierarchyCellDataOpsReal<double> hcellmath(hierarchy, ln, ln);
      
   
   if (allocate_data)
   {
     BSSN_APPLY_TO_FIELDS(RK4_ARRAY_ALLOC);
     BSSN_APPLY_TO_SOURCES(EXTRA_ARRAY_ALLOC);
     BSSN_APPLY_TO_GEN1_EXTRAS(EXTRA_ARRAY_ALLOC);
     level->allocatePatchData(weight_idx);
   }
   
   if(init_data_time < EPS) //at beginning
   {
     initLevel(patch_hierarchy, ln);

     bssnSim->copyAToP(hcellmath);
   
     computeVectorWeights(hierarchy);

     return;
   }
   tbox::pout<<"Flag\n";
   //BSSN_APPLY_TO_FIELDS_ARGS(RK4_ARRAY_ZERO,hcellmath);
   BSSN_APPLY_TO_SOURCES_ARGS(EXTRA_ARRAY_ZERO,hcellmath);
   BSSN_APPLY_TO_GEN1_EXTRAS_ARGS(EXTRA_ARRAY_ZERO,hcellmath);
       
   /*
    * Refine solution data from coarser level and, if provided, old level.
    */
   
   xfer::RefineAlgorithm refiner;

   boost::shared_ptr<hier::RefineOperator> accurate_refine_op =
     space_refine_op;
     
   TBOX_ASSERT(accurate_refine_op);

   //registering refine variables
   BSSN_APPLY_TO_FIELDS_ARGS(VAC_REGISTER_SPACE_REFINE_A,refiner,accurate_refine_op);
                             
   boost::shared_ptr<xfer::RefineSchedule> refine_schedule;

   level->getBoxLevel()->getMPI().Barrier();
   if (ln > 0)
   {
     /*
      * Include coarser levels in setting data
      */
     refine_schedule =
       refiner.createSchedule(
         level,
         old_level,
         ln - 1,
         hierarchy,
         cosmoPS);
   }
   else
   {
     /*
      * There is no coarser level, and source data comes only
      * from old_level, if any.
      */
     if (old_level)
     {
       refine_schedule =
         refiner.createSchedule(level,
                                old_level,
                                NULL);
     }
     else
     {
       refine_schedule =
         refiner.createSchedule(level,
                                level,
                                NULL);
     }
   }
   level->getBoxLevel()->getMPI().Barrier();
     

   if (refine_schedule)
   {
     refine_schedule->fillData(0.0);
     // It is null if this is the bottom level.
   }
   else
   {
     TBOX_ERROR(
       "Can not get refine schedule, check your code!\n");
   }


   //BSSN_APPLY_TO_FIELDS(BSSN_COPY_A_TO_P);
   bssnSim->copyAToP(hcellmath);
   
   level->getBoxLevel()->getMPI().Barrier();
   /* Set vector weight. */
   computeVectorWeights(hierarchy);
   //hierarchy->recursivePrint(tbox::plog, "\t", 2);
   // for( hier::PatchLevel::iterator pit(level->begin());
   //      pit != level->end(); ++pit)
   // {
   //   const boost::shared_ptr<hier::Patch> & patch = *pit;
   //   //patch->recursivePrint(tbox::plog, "\t", 2);
   //   cosmo_io->printPatch(patch, tbox::plog, bssnSim->DIFFK_a_idx);
   // }
   //hier::VariableDatabase::getDatabase()->printClassData(tbox::plog);
}

void VacuumSim::applyGradientDetector(
   const boost::shared_ptr<hier::PatchHierarchy>& hierarchy_,
   const int ln,
   const double error_data_time,
   const int tag_index,
   const bool initial_time,
   const bool uses_richardson_extrapolation)
{
   NULL_USE(uses_richardson_extrapolation);
   NULL_USE(error_data_time);
   NULL_USE(initial_time);

   if (lstream) {
      *lstream
      << "VaccumSim("  << ")::applyGradientDetector"
      << std::endl;
   }
   hier::PatchHierarchy& hierarchy = *hierarchy_;
   boost::shared_ptr<geom::CartesianGridGeometry> grid_geometry_(
     BOOST_CAST<geom::CartesianGridGeometry, hier::BaseGridGeometry>(
       hierarchy.getGridGeometry()));
   double max_der_norm = 0;
   hier::PatchLevel& level =
      (hier::PatchLevel &) * hierarchy.getPatchLevel(ln);
   size_t ntag = 0, ntotal = 0;
   //double maxestimate = 0;
   for (hier::PatchLevel::iterator pi(level.begin());
        pi != level.end(); ++pi)
   {
      hier::Patch& patch = **pi;

      const boost::shared_ptr<geom::CartesianPatchGeometry> patch_geom(
        BOOST_CAST<geom::CartesianPatchGeometry, hier::PatchGeometry>(
          patch.getPatchGeometry()));

      boost::shared_ptr<hier::PatchData> tag_data(
         patch.getPatchData(tag_index));
      ntotal += patch.getBox().numberCells().getProduct();
      if (!tag_data)
      {
         TBOX_ERROR(
            "Data index " << tag_index << " does not exist for patch.\n");
      }
      boost::shared_ptr<pdat::CellData<int> > tag_cell_data_(
         BOOST_CAST<pdat::CellData<int>, hier::PatchData>(tag_data));
      TBOX_ASSERT(tag_cell_data_);
      
      boost::shared_ptr<pdat::CellData<double>> K_data(
        BOOST_CAST<pdat::CellData<double>, hier::PatchData>(
          patch.getPatchData(bssnSim->DIFFchi_a_idx)));


      arr_t K = pdat::ArrayDataAccess::access<DIM, real_t>(
        K_data->getArrayData());
      
      if (!K_data) {
         TBOX_ERROR("Data index " << bssnSim->DIFFchi_p_idx
                                  << " does not exist for patch.\n");
      }
      pdat::CellData<idx_t>& tag_cell_data = *tag_cell_data_;
      //pdat::CellData<real_t> & weight = *weight_;
          
      tag_cell_data.fill(0);
      
      hier::Box::iterator iend(patch.getBox().end());

      for (hier::Box::iterator i(patch.getBox().begin()); i != iend; ++i)
      {
         const pdat::CellIndex cell_index(*i);
         max_der_norm = tbox::MathUtilities<double>::Max(
           max_der_norm,
           derivative_norm(
             cell_index(0),
             cell_index(1),
             cell_index(2),
             K));
         if(derivative_norm(
              cell_index(0),
              cell_index(1),
              cell_index(2),
              K) > adaption_threshold)
         {
          
           tag_cell_data(cell_index) = 1;
           ++ntag;
         }
       
      }
      /*for (hier::Box::iterator i(patch.getBox().begin()); i != iend; ++i)
      {
         const pdat::CellIndex cell_index(*i);
         if(ln == 2)
         {
           const pdat::CellIndex c2(hier::Index(128-cell_index(0)-1, cell_index(1), cell_index(2)));
           if(tag_cell_data(c2) != tag_cell_data(cell_index))
             tbox::pout<<"Ooooops "<<cell_index(0)<<" "<<ln<<"\n";
         }

         }*/

   }
const tbox::SAMRAI_MPI& mpi(hierarchy.getMPI());
if (mpi.getSize() > 1)
 {
   mpi.AllReduce(&max_der_norm, 1, MPI_MAX);
 }

   tbox::plog << "Adaption threshold is " << adaption_threshold << "\n";
   tbox::plog << "Number of cells tagged on level " << ln << " is "
              << ntag << "/" << ntotal << "\n";
   tbox::plog << "Max norm is " << max_der_norm << "\n";
}
  
void VacuumSim::outputVacuumStep(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy)
{
    // prepBSSNOutput();
    // io_bssn_fields_snapshot(iodata, step, bssnSim->fields);
    // io_bssn_fields_powerdump(iodata, step, bssnSim->fields, fourier);
    // io_bssn_dump_statistics(iodata, step, bssnSim->fields, bssnSim->frw);
    // io_bssn_constraint_violation(iodata, step, bssnSim);
  boost::shared_ptr<appu::VisItDataWriter> visit_writer(
    new appu::VisItDataWriter(
    dim, "VisIt Writer", vis_filename + ".visit"));

  tbox::pout<<"step: "<<step<<"/"<<num_steps<<"\n";
  
  bssnSim->output_max_H_constaint(hierarchy, weight_idx);
 
  cosmo_io->registerVariablesWithPlotter(*visit_writer, step);
  cosmo_io->dumpData(hierarchy, *visit_writer, step, cur_t);
  
}

void VacuumSim::runVacuumStep(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy,
  double from_t, double to_t)
{
  t_RK_steps->start();
    // Full RK step minus init()
  advanceLevel(hierarchy,
               0,
               from_t,
               to_t);

  t_RK_steps->stop();
}

double VacuumSim::getDt(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy)
{
  boost::shared_ptr<geom::CartesianGridGeometry> grid_geometry_(
    BOOST_CAST<geom::CartesianGridGeometry, hier::BaseGridGeometry>(
      hierarchy->getGridGeometry()));
  geom::CartesianGridGeometry& grid_geometry = *grid_geometry_;

  
  return (grid_geometry.getDx()[0]) * dt_frac;
}

void VacuumSim::runStep(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy)
{
  runCommonStepTasks(hierarchy);

  double dt = getDt(hierarchy);
  initVacuumStep(hierarchy);
  
  outputVacuumStep(hierarchy);
  runVacuumStep(hierarchy, cur_t, cur_t + dt);
  cur_t += dt;
}


void VacuumSim::addBSSNExtras(
  const boost::shared_ptr<hier::PatchLevel> & level)
{
#if USE_CCZ4
  bssnSim->initZ(level);
#endif
  return;
}

void VacuumSim::addBSSNExtras(
  const boost::shared_ptr<hier::Patch> & patch)
{
#if USE_CCZ4
  bssnSim->initZ(patch);
#endif
  return;
}

  
void VacuumSim::RKEvolveLevel(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy,
  idx_t ln,
  double from_t,
  double to_t)
{
  const boost::shared_ptr<hier::PatchLevel> level(
    hierarchy->getPatchLevel(ln));

  const boost::shared_ptr<hier::PatchLevel> coarser_level(
    ((ln>0)?(hierarchy->getPatchLevel(ln-1)):NULL));
  
  
  xfer::RefineAlgorithm refiner;
  boost::shared_ptr<xfer::RefineSchedule> refine_schedule;


  
  bssnSim->registerRKRefiner(refiner, space_refine_op);

  bssnSim->prepairForK1(coarser_level, to_t);


  
  //if not the coarsest level, should 
  if(coarser_level!=NULL)
  {
    boost::shared_ptr<xfer::PatchLevelBorderFillPattern> border_fill_pattern (
      new xfer::PatchLevelBorderFillPattern());
    
    refine_schedule = refiner.createSchedule(
      //border_fill_pattern,
      level,
      level,
      coarser_level->getLevelNumber(),
      hierarchy,
      (cosmoPS->is_time_dependent)?NULL:cosmoPS);
  }
  else
    refine_schedule = refiner.createSchedule(level, NULL);
  
  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;

    //Evolve inner grids
    bssnSim->RKEvolvePatch(patch, to_t - from_t);
    // Evolve physical boundary
    // would not do anything if boundary is time independent

    bssnSim->RKEvolvePatchBD(patch, to_t - from_t);  
  }

  

  level->getBoxLevel()->getMPI().Barrier();
  refine_schedule->fillData(to_t);

  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;
    bssnSim->K1FinalizePatch(patch);
    addBSSNExtras(patch);
  }

  
  /**************Starting K2 *********************************/
  bssnSim->prepairForK2(coarser_level, to_t);
  
  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;

        //Evolve inner grids
    bssnSim->RKEvolvePatch(patch, to_t - from_t);
    //Evolve physical boundary
    // would not do anything if boundary is time dependent
    bssnSim->RKEvolvePatchBD(patch, to_t - from_t);  
  }

  level->getBoxLevel()->getMPI().Barrier();
  refine_schedule->fillData(to_t);

  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;
    bssnSim->K2FinalizePatch(patch);
    addBSSNExtras(patch);
  }

  /**************Starting K3 *********************************/

  bssnSim->prepairForK3(coarser_level, to_t);
    

  
  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;

  

    bssnSim->RKEvolvePatch(patch, to_t - from_t);
    //Evolve physical boundary
    // would not do anything if boundary is time dependent
    bssnSim->RKEvolvePatchBD(patch, to_t - from_t);  
  }

  level->getBoxLevel()->getMPI().Barrier();
  refine_schedule->fillData(to_t);

  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;
    bssnSim->K3FinalizePatch(patch);
    addBSSNExtras(patch);
  }

  /**************Starting K4 *********************************/

  bssnSim->prepairForK4(coarser_level, to_t);

  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;

    bssnSim->RKEvolvePatch(patch, to_t - from_t);
    //Evolve physical boundary
    // would not do anything if boundary is time dependent
    bssnSim->RKEvolvePatchBD(patch, to_t - from_t);  
  }

  level->getBoxLevel()->getMPI().Barrier();
  refine_schedule->fillData(to_t);

  for( hier::PatchLevel::iterator pit(level->begin());
       pit != level->end(); ++pit)
  {
    const boost::shared_ptr<hier::Patch> & patch = *pit;
    bssnSim->K4FinalizePatch(patch);
    addBSSNExtras(patch);
  }

}


void VacuumSim::advanceLevel(
  const boost::shared_ptr<hier::PatchHierarchy>& hierarchy,
  int ln,
  double from_t,
  double to_t)
{
  if( ln >= hierarchy->getNumberOfLevels())
    return;
  
  //double dt = to_t - from_t;
  const boost::shared_ptr<hier::PatchLevel> level(
    hierarchy->getPatchLevel(ln));

  //updating extra fields like (Z^i) before advance any level
  addBSSNExtras(level);

  bssnSim->setLevelTime(level, from_t, to_t);
  //RK advance interior(including innner ghost cells) of level


  RKEvolveLevel(
    hierarchy, ln, from_t, to_t);


  level->getBoxLevel()->getMPI().Barrier();
     
  advanceLevel(hierarchy, ln+1, from_t, from_t + (to_t - from_t)/2.0);

  level->getBoxLevel()->getMPI().Barrier();
  
  advanceLevel(hierarchy, ln+1, from_t + (to_t - from_t)/2.0, to_t);

   

  if(ln < hierarchy->getNumberOfLevels() -1 )
  {
    xfer::CoarsenAlgorithm coarsener(dim);
  

    boost::shared_ptr<xfer::CoarsenSchedule> coarsen_schedule;

    bssnSim->registerCoarsenActive(coarsener,space_coarsen_op);
      
    coarsen_schedule = coarsener.createSchedule(level, hierarchy->getPatchLevel(ln+1));
    level->getBoxLevel()->getMPI().Barrier();
    coarsen_schedule->coarsenData();

    xfer::RefineAlgorithm post_refiner;

    boost::shared_ptr<xfer::RefineSchedule> refine_schedule;

    

    bssnSim->registerSameLevelRefinerActive(post_refiner, space_refine_op);
    
    refine_schedule = post_refiner.createSchedule(level, NULL);

    level->getBoxLevel()->getMPI().Barrier();
    refine_schedule->fillData(to_t);
  }

  // copy _a to _p and set _p time to next timestamp1

  math::HierarchyCellDataOpsReal<real_t> hcellmath(hierarchy,ln,ln);

  //bssnSim->swapPF(hcellmath);
  bssnSim->copyAToP(hcellmath);

  bssnSim->setLevelTime(level, to_t, to_t);

  level->getBoxLevel()->getMPI().Barrier();
}
void VacuumSim::resetHierarchyConfiguration(
  /*! New hierarchy */
  const boost::shared_ptr<hier::PatchHierarchy>& new_hierarchy,
  /*! Coarsest level */ int coarsest_level,
  /*! Finest level */ int finest_level)
{
  return;
}
  


} /* namespace cosmo */