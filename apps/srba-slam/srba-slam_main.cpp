/* +---------------------------------------------------------------------------+
   |                 The Mobile Robot Programming Toolkit (MRPT)               |
   |                                                                           |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2013, Individual contributors, see AUTHORS file        |
   | Copyright (c) 2005-2013, MAPIR group, University of Malaga                |
   | Copyright (c) 2012-2013, University of Almeria                            |
   | All rights reserved.                                                      |
   |                                                                           |
   | Redistribution and use in source and binary forms, with or without        |
   | modification, are permitted provided that the following conditions are    |
   | met:                                                                      |
   |    * Redistributions of source code must retain the above copyright       |
   |      notice, this list of conditions and the following disclaimer.        |
   |    * Redistributions in binary form must reproduce the above copyright    |
   |      notice, this list of conditions and the following disclaimer in the  |
   |      documentation and/or other materials provided with the distribution. |
   |    * Neither the name of the copyright holders nor the                    |
   |      names of its contributors may be used to endorse or promote products |
   |      derived from this software without specific prior written permission.|
   |                                                                           |
   | THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       |
   | 'AS IS' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED |
   | TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR|
   | PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE |
   | FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL|
   | DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR|
   |  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)       |
   | HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,       |
   | STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN  |
   | ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           |
   | POSSIBILITY OF SUCH DAMAGE.                                               |
   +---------------------------------------------------------------------------+ */

/*
App : srba-slam
Info: A front-end for Relative Bundle Adjustment (RBA). The program is
      flexible to cope with several types of sensors and problem parameterizations.
	  Input datasets are plain text, in the format generated by RWT [1].
	  More docs on SRBA in [2].

	  [1] http://www.mrpt.org/Application:srba-slam
	  [2] http://www.mrpt.org/srba
*/

#include <mrpt/srba.h>

#include <mrpt/utils.h> // We use a lot of classes from here
#include <mrpt/system/filesystem.h>  // for ASSERT_FILE_EXISTS_
#include <mrpt/system/threads.h>  // for sleep()
#include <mrpt/random.h>
#include <mrpt/math/CMatrixTemplateNumeric.h>  // For CMatrixDouble
#include <mrpt/math/CMatrixD.h>  // For the serializable version of matrices
//#include <mrpt/vision/CVideoFileWriter.h>

#include <mrpt/gui/CDisplayWindow3D.h>

#include <mrpt/otherlibs/tclap/CmdLine.h>

#include <sstream>  // For stringstream
#include <memory>  // For auto_ptr
//#include <iostream>
//#include <locale>

using namespace mrpt;
using namespace mrpt::srba;
using namespace std;


// ---------------- All the parameters of this app: --------------
struct RBASLAM_Params
{
	// Declare the supported options.
	TCLAP::CmdLine cmd;
	TCLAP::ValueArg<string> arg_dataset;
	TCLAP::ValueArg<string> arg_gt_map;
	TCLAP::ValueArg<string> arg_gt_path;
	TCLAP::ValueArg<unsigned int> arg_max_known_feats_per_frame;
	TCLAP::SwitchArg  arg_se2,arg_se3;
	TCLAP::SwitchArg  arg_lm2d,arg_lm3d;
	TCLAP::ValueArg<string> arg_obs;
	TCLAP::ValueArg<string> arg_sensor_params;
	TCLAP::SwitchArg  arg_no_gui;
	TCLAP::SwitchArg  arg_gui_step_by_step;
	TCLAP::ValueArg<string>  arg_profile_stats;
	TCLAP::ValueArg<unsigned int> arg_profile_stats_length;
	TCLAP::ValueArg<double> arg_pixel_noise;
	TCLAP::ValueArg<unsigned int> arg_max_tree_depth;
	TCLAP::ValueArg<unsigned int> arg_max_opt_depth;
	TCLAP::ValueArg<unsigned int> arg_max_iters;
	TCLAP::ValueArg<string>       arg_edge_policy;
	TCLAP::ValueArg<unsigned int> arg_submap_size;
	TCLAP::ValueArg<unsigned int> arg_verbose;
	TCLAP::ValueArg<unsigned int> arg_random_seed;
	TCLAP::ValueArg<string> arg_rba_params_cfg_file;
	TCLAP::ValueArg<string> arg_write_rba_params_cfg_file;
	TCLAP::ValueArg<string> arg_video;
	TCLAP::ValueArg<double> arg_video_fps;
	TCLAP::SwitchArg   arg_debug_dump_cur_spantree;
	TCLAP::ValueArg<string> arg_save_final_graph;
	TCLAP::ValueArg<string> arg_save_final_graph_landmarks;
	TCLAP::SwitchArg   arg_eval_overall_sqr_error;
	TCLAP::SwitchArg   arg_eval_overall_se3_error;
	TCLAP::SwitchArg   arg_eval_connectivity;

	// Parse all cmd-line arguments at construction
	// ---------------------------------------------------
	RBASLAM_Params(int argc, char**argv) :
		cmd (argv[0], ' ', mrpt::system::MRPT_getVersion().c_str()),
		arg_dataset("d","dataset","Dataset file (e.g. 'dataset1_SENSOR.txt', etc.)",false,"","",cmd),
		arg_gt_map("","gt-map","Ground-truth landmark map file (e.g. 'dataset1_GT_MAP.txt', etc.)",false,"","",cmd),
		arg_gt_path("","gt-path","Ground-truth robot path file (e.g. 'dataset1_GT_PATH.txt', etc.)",false,"","",cmd),
		arg_max_known_feats_per_frame("","max-fixed-feats-per-kf","Create fixed & known-location features",false,0,"",cmd),
		arg_se2("","se2","Relative poses are SE(2)",cmd, false),
		arg_se3("","se3","Relative poses are SE(3)",cmd, false),
		arg_lm2d("","lm-2d","Relative landmarks are Euclidean 2D points",cmd, false),
		arg_lm3d("","lm-3d","Relative landmarks are Euclidean 2D points",cmd, false),
		arg_obs("","obs","Type of observations in the dataset (use --list-obs to see available types)",false,"","",cmd),
		arg_sensor_params("","sensor-params-cfg-file","Config file from where to load the sensor parameters",false,"","",cmd),
		arg_no_gui("","no-gui","Don't show the live gui",cmd, false),
		arg_gui_step_by_step("","step-by-step","If showing the gui, go step by step",cmd, false),
		arg_profile_stats("","profile-stats","Generate profile stats to CSV files, with the given prefix",false,"","stats",cmd),
		arg_profile_stats_length("","profile-stats-length","Length in KFs of each saved profiled segment",false,10,"",cmd),
		arg_pixel_noise("","pixel-noise","Std of the AWGN of feature coordinates (px).\n If a SRBA config is provided, it will override this value.",false,0.05,"noise_std",cmd),
		arg_max_tree_depth("","max-spanning-tree-depth","Overrides this parameter in config files",false,4,"depth",cmd),
		arg_max_opt_depth("","max-optimize-depth","Overrides this parameter in config files",false,4,"depth",cmd),
		arg_max_iters("","max-iters","Max. number of optimization iterations.",false,20,"",cmd),
		arg_edge_policy("","edge-policy","Policy for edge creation, as textual names of the enum TEdgeCreationPolicy",false,"ecpICRA2013","ecpOptimizeICRA2013",cmd),
		arg_submap_size("","submap-size","Number of KFs in each 'submap' of the arc-creation policy.",false,20,"20",cmd),
		arg_verbose("v","verbose-level","0:quiet, 1:informative, 2:tons of info",false,1,"",cmd),
		arg_random_seed("","random-seed","<0: randomize; >=0, use this random seed.",false,-1,"",cmd),
		arg_rba_params_cfg_file("","cfg-file-rba","Config file (*.cfg) for the RBA parameters",false,"","rba.cfg",cmd),
		arg_write_rba_params_cfg_file("","cfg-file-rba-bootstrap","Writes an empty config file (*.cfg) for the RBA parameters and exit.",false,"","rba.cfg",cmd),
		arg_video("","create-video","Creates a video with the animated GUI output (*.avi).",false,"","out.avi",cmd),
		arg_video_fps("","video-fps","If creating a video, its FPS (Hz).",false,30.0,"",cmd),
		arg_debug_dump_cur_spantree("","debug-dump-cur-spantree","Dump to files the current spanning tree",cmd, false),
		arg_save_final_graph("","save-final-graph","Save the final graph-map of KFs to a .dot file",false,"","final-map.dot",cmd),
		arg_save_final_graph_landmarks("","save-final-graph-landmarks","Save the final graph-map (all KFs and all Landmarks) to a .dot file",false,"","final-map.dot",cmd),
		arg_eval_overall_sqr_error("","eval-overall-sqr-error","At end, evaluate the overall square error for all the observations with the final estimated model",cmd, false),
		arg_eval_overall_se3_error("","eval-overall-se3-error","At end, evaluate the overall SE3 error for all relative poses",cmd, false),
		arg_eval_connectivity("","eval-connectivity","At end, make stats on the graph connectivity",cmd, false)
	{
		// Parse arguments:
		if (!cmd.parse( argc, argv ))
			throw std::runtime_error(""); // should exit, but without any error msg (should have been dumped to cerr)

		if (!arg_obs.isSet())
			throw std::runtime_error("Error: argument --obs is mandatory to select the type of observations.\nRun with --help to see all the options or visit http://www.mrpt.org/srba for docs and examples.\n");

		if ( (arg_se2.isSet() && arg_se3.isSet()) ||
			 (!arg_se2.isSet() && !arg_se3.isSet()) )
			 throw std::runtime_error("Exactly one of --se2 or --se3 flags must be set.");

		if ( (arg_lm2d.isSet() && arg_lm3d.isSet()) ||
			 (!arg_lm2d.isSet() && !arg_lm3d.isSet()) )
			 throw std::runtime_error("Exactly one of --lm-2d or --lm-3d flags must be set.");
		
	}
};

// ---------------- INCLUDE DATASET PARSERS -------------------------
#include "CDatasetParserBase.h"
#include "CDatasetParser_RangeBearing2D.h"
#include "CDatasetParser_Stereo.h"
// ------------------------------------------------------------------


// ---------------- The main RBA-Runner class -----------------------
struct RBA_Run_Base
{
	virtual int run(RBASLAM_Params &params) = 0;

	RBA_Run_Base() {}
	virtual ~RBA_Run_Base() {}
};


// -------------------- InitializerSensorParams -------------------------
template <class OBS_TYPE> 
struct InitializerSensorParams
{
	template <class RBA>
	static void init(RBA &rba, RBASLAM_Params &config)
	{
		// Nothing to do by default:
	}
};

template <> 
struct InitializerSensorParams<mrpt::srba::observations::StereoCamera>
{
	template <class RBA>
	static void init(RBA &rba, RBASLAM_Params &config)
	{
		// Load params from file: 

		if (!config.arg_sensor_params.isSet())
			throw std::runtime_error("Error: --sensor-params-cfg-file is mandatory for this type of observations.");

		const std::string sCfgFile = config.arg_sensor_params.getValue();
		rba.parameters.sensor.camera_calib.loadFromConfigFile("CAMERA",mrpt::utils::CConfigFile(sCfgFile) );
		const double baseline = rba.parameters.sensor.camera_calib.rightCameraPose.x();
		ASSERT_(baseline!=0)
	}
};

// -------------------- InitializerSensorPoseParams  -------------------------
template <class SENSOR_POSE_OPTION> 
struct InitializerSensorPoseParams;

template <> 
struct InitializerSensorPoseParams<sensor_pose_on_robot_none>
{
	template <class RBA>
	static void init(RBA &rba, RBASLAM_Params &config)
	{
		// Nothing to do.
	}
};

template <> 
struct InitializerSensorPoseParams<sensor_pose_on_robot_se3>
{
	template <class RBA>
	static void init(RBA &rba, RBASLAM_Params &config)
	{
		// Sensor pose on the robot parameters:
		rba.parameters.sensor_pose.relative_pose = mrpt::poses::CPose3D(0,0,0,DEG2RAD(-90),DEG2RAD(0),DEG2RAD(-90) );
	}
};


template <class KF2KF_POSE_TYPE,class LM_TYPE,class OBS_TYPE, class RBA_OPTIONS>
struct RBA_Run : public RBA_Run_Base
{
	typedef RBA_Problem<
		KF2KF_POSE_TYPE, // Parameterization  KF-to-KF poses
		LM_TYPE,         // Parameterization of landmark positions
		OBS_TYPE,        // Type of observations
		RBA_OPTIONS      // Other parameters
		>
		my_srba_t;

	// Constructor:
	virtual int run(RBASLAM_Params &cfg)
	{
		// Open & load dataset:
		// ------------------------------------------
		CDatasetParserTempl<OBS_TYPE>  dataset(cfg);


		// Create an empty RBA problem:
		// ------------------------------------------
		my_srba_t rba;

		// Init/load sensor parameters:
		InitializerSensorParams<OBS_TYPE>::init(rba,cfg);

		// Init sensor-to-robot relative pose parameters:
		InitializerSensorPoseParams<typename RBA_OPTIONS::sensor_pose_on_robot_t>::init(rba, cfg);

		// Process cmd-line flags:
		// ------------------------------------------
		if (cfg.arg_random_seed.getValue()<0)
				mrpt::random::randomGenerator.randomize();
		else 	mrpt::random::randomGenerator.randomize( cfg.arg_random_seed.getValue() );

		//const double PIXEL_NOISE_STD =cfg.arg_pixel_noise.getValue();  // pixels
		const bool DEBUG_DUMP_ALL_SPANNING_TREES =cfg.arg_debug_dump_cur_spantree.getValue();

		rba.setVerbosityLevel(cfg.arg_verbose.getValue() );

		rba.parameters.srba.use_robust_kernel = false;
		rba.parameters.srba.std_noise_observations = 0.02; // PIXEL_NOISE_STD;

		rba.parameters.srba.compute_condition_number = false;

		rba.parameters.srba.max_error_per_obs_to_stop = 1e-8;
		//rba.parameters.srba.feedback_user_iteration = &optimization_feedback;

		// =========== Topology parameters ===========
	//	rba.parameters.edge_creation_policy = mrpt::srba::ecpICRA2013;
	//	rba.parameters.max_tree_depth       = 4;
		// ===========================================

		if (cfg.arg_rba_params_cfg_file.isSet() && mrpt::system::fileExists(cfg.arg_rba_params_cfg_file.getValue()))
			rba.parameters.srba.loadFromConfigFileName(cfg.arg_rba_params_cfg_file.getValue(), "srba");

		if (cfg.arg_write_rba_params_cfg_file.isSet())
		{
			rba.parameters.srba.saveToConfigFileName(cfg.arg_write_rba_params_cfg_file.getValue(), "srba");
			return 0; // end program
		}

		// Override max ST depth:
		if (cfg.arg_max_tree_depth.isSet())
		{
			rba.parameters.srba.max_tree_depth =cfg.arg_max_tree_depth.getValue();
			cout << "Overriding max_tree_depth to value: " << rba.parameters.srba.max_tree_depth << endl;
		}

		// Override policy:
		if (cfg.arg_edge_policy.isSet())
		{
			rba.parameters.srba.edge_creation_policy = mrpt::utils::TEnumType<srba::TEdgeCreationPolicy>::name2value(cfg.arg_edge_policy.getValue());
			cout << "Overriding edge_creation_policy to value: " <<cfg.arg_edge_policy.getValue() << endl;
		}

		// Override max optimize depth:
		if (cfg.arg_max_opt_depth.isSet())
		{
			rba.parameters.srba.max_optimize_depth =cfg.arg_max_opt_depth.getValue();
			cout << "Overriding max_optimize_depth to value: " << rba.parameters.srba.max_optimize_depth << endl;
		}

		// Override max optimize depth:
		if (cfg.arg_max_iters.isSet())
		{
			rba.parameters.srba.max_iters =cfg.arg_max_iters.getValue();
			cout << "Overriding max_iters to value: " << rba.parameters.srba.max_iters << endl;
		}

		// Override submap
		if (cfg.arg_submap_size.isSet())
		{
			rba.parameters.srba.submap_size =cfg.arg_submap_size.getValue();
			cout << "Overriding submap_size to value: " << rba.parameters.srba.submap_size << endl;
		}

		if (cfg.arg_verbose.getValue()>=1)
		{
			cout << "RBA parameters:\n"
					"-----------------\n";
			rba.parameters.srba.dumpToConsole();
			cout << endl;
		}

		const unsigned int	INCREMENTAL_FRAMES_AT_ONCE  = 1;
		const unsigned int	MAX_KNOWN_FEATS_PER_FRAME   =cfg.arg_max_known_feats_per_frame.getValue();

		const double REL_POS_NOISE_STD_KNOWN    = 0.0001;  // m
		const double REL_POS_NOISE_STD_UNKNOWN  = 0.3;  // m

		const bool DEMO_SIMUL_SHOW_GUI = !cfg.arg_no_gui.getValue();
		bool DEMO_SIMUL_SHOW_STEPBYSTEP =cfg.arg_gui_step_by_step.getValue();

		const bool   SAVE_TIMING_STATS =cfg.arg_profile_stats.isSet();
		const size_t SAVE_TIMING_SEGMENT_LENGTH =cfg.arg_profile_stats_length.getValue();

		// Load dataset:
		const size_t nTotalObs = dataset.obs().getRowCount();

		// Set camera calib from dataset:
		//rba.parameters.sensor.camera_calib = CAM_CALIB;

		// Sensor pose on the robot parameters:
		//rba.parameters.sensor_pose.relative_pose = mrpt::poses::CPose3D(0,0,0,DEG2RAD(-90),DEG2RAD(0),DEG2RAD(-90) ); // Set camera pointing forwards (camera's +Z is robot +X)

		// ------------------------------------------------------------------------
		// Process the dataset sequentially, add observations to the RBA problem
		// and solve it:
		// ------------------------------------------------------------------------

		mrpt::gui::CDisplayWindow3DPtr win;
		mrpt::opengl::COpenGLViewportPtr gl_view2D, gl_view_cur_tree;
		mrpt::opengl::CSetOfObjectsPtr gl_rba_scene, gl_cur_kf_tree;

		// Arbitrary unique IDs for text labels in the 3D gui.
		const size_t TXTID_KF = 1000;

		if (DEMO_SIMUL_SHOW_GUI)
		{
			win = mrpt::gui::CDisplayWindow3D::Create("Simulation state",1000,800);
			win->setCameraPointingToPoint(8,8,0);

			//win->grabImagesStart("capture_");

			gl_rba_scene   = mrpt::opengl::CSetOfObjects::Create();
			gl_cur_kf_tree = mrpt::opengl::CSetOfObjects::Create();

			{
				mrpt::opengl::COpenGLScenePtr scene = win->get3DSceneAndLock();

				//gl_view2D = scene->createViewport("view2D");
				//gl_view2D->setViewportPosition(0,0,640/2,480/2);

				gl_view_cur_tree = scene->createViewport("view_tree");
				gl_view_cur_tree->setViewportPosition(-600,0, 600,200);
				gl_view_cur_tree->setBorderSize(1);
				gl_view_cur_tree->setTransparent(true);
				mrpt::opengl::CCamera & cam = gl_view_cur_tree->getCamera();
				cam.setPointingAt(0,-10,0);
				cam.setElevationDegrees(90);
				cam.setAzimuthDegrees(-90);
				cam.setOrthogonal();
				cam.setZoomDistance(25);

				scene->insert(gl_rba_scene);
				gl_view_cur_tree->insert(gl_cur_kf_tree);

				win->unlockAccess3DScene();
			}
		}

		// Create video?
		//mrpt::vision::CVideoFileWriter  outVideo;
		//const string sOutVideo =cfg.arg_video.getValue();
		//if (cfg.arg_video.isSet() && win)
		//	win->captureImagesStart();

		//const double VIDEO_FPS =cfg.arg_video_fps.getValue();


		// All LMs with known, fixed relative coordinates: this will be automatically filled-in
		//  the first time a LM is seen, so the current frame  ID becomes its base frame.
		typename my_srba_t::TRelativeLandmarkPosMap  all_fixed_LMs;
		typename my_srba_t::TRelativeLandmarkPosMap  all_unknown_LMs_GT; // Ground truth of relative position for feats with unknown rel.pos.

		// Timming stats:
		map<string,vector<double> >  timming_section_mean_times;
		map<string,vector<size_t> >  timming_section_call_count;
		vector<size_t> timming_section_ts; // The time indices for the above.

		// Simul counters
		srba::TKeyFrameID  frameIdx;
		srba::TKeyFrameID  next_rba_keyframe_ID = 0;
		size_t obsIdx;

		size_t last_obs_idx_reported = 0;
		const size_t REPORT_PROGRESS_OBS_COUNT = 200;

		mrpt::utils::CTicTac  mytimer;
		mytimer.Tic();


		for (frameIdx=0,obsIdx=0; obsIdx<nTotalObs ; )
		{
			const unsigned int nFramesAtOnce =  frameIdx==0  ? INCREMENTAL_FRAMES_AT_ONCE+1 : INCREMENTAL_FRAMES_AT_ONCE;

			// Collect all the observations for the present time step:
			srba::TKeyFrameID curFrameIdx=frameIdx;

			const srba::TKeyFrameID frameIdxMax = frameIdx+nFramesAtOnce;
			frameIdx+=nFramesAtOnce; // for the next round

			//mrpt::vision::TSequenceFeatureObservations  feats_to_draw; // For the GUI

			srba::TNewKeyFrameInfo new_kf_info;

			while (obsIdx<nTotalObs && curFrameIdx<frameIdxMax)
			{
				size_t  nKnownFeatsInThisFrame = 0;
				typename my_srba_t::new_kf_observations_t  new_obs_in_this_frame;

				if (obsIdx-last_obs_idx_reported>REPORT_PROGRESS_OBS_COUNT)
				{
					last_obs_idx_reported= obsIdx;
					const double dataset_percent_done = (100.*obsIdx)/nTotalObs;

					const double elapsed_tim = mytimer.Tac();
					const double estimated_rem_tim = (dataset_percent_done<100) ? elapsed_tim*(1.0-1.0/(0.01*dataset_percent_done)) : 0;

					cout << "Progress in dataset: obs " << obsIdx << " / " << nTotalObs<< mrpt::format(" (%.4f%%)  Remaining: %s \r",dataset_percent_done, mrpt::system::formatTimeInterval(estimated_rem_tim).c_str() );  // '\r' so if verbose=0 only 1 line is refreshed.
					cout.flush();
				}

				while (obsIdx<nTotalObs && (curFrameIdx=dataset.obs().coeff(obsIdx,0))==next_rba_keyframe_ID)
				{
					// Is this observation of a LM with known (i.e. FIXED) relative position?
					bool this_feat_has_known_rel_pos = (nKnownFeatsInThisFrame<MAX_KNOWN_FEATS_PER_FRAME);

					// Get observation from the specific dataset parser:
					typename my_srba_t::new_kf_observation_t new_obs;
					dataset.getObs(obsIdx,new_obs.obs);

					// Already has a base ID?
#if 0
					const size_t LMidx = new_obs.obs.feat_id;
					typename my_srba_t::TRelativeLandmarkPosMap::const_iterator itRelFeat = all_fixed_LMs.find(LMidx);
					if (itRelFeat == all_fixed_LMs.end())
					{
						nKnownFeatsInThisFrame++;

						// Nope, it's the first observation:
						// Get global location of LM:
						mrpt::math::TPoint3D globalPos;
						GT_MAP.getPoint(LMidx,globalPos);

						// Convert to relative:
						mrpt::math::TPoint3D relPos;
						GT_path[curFrameIdx].inverseComposePoint(
							globalPos.x,globalPos.y,globalPos.z,
							relPos.x,relPos.y,relPos.z);

						const my_srba_t::TRelativeLandmarkPos rfp_GT= my_srba_t::TRelativeLandmarkPos( curFrameIdx, relPos );

						// Add random noise to "measured" relative positions:
						if (this_feat_has_known_rel_pos)
						{
							relPos.x += mrpt::random::randomGenerator.drawGaussian1D(0, REL_POS_NOISE_STD_KNOWN);
							relPos.y += mrpt::random::randomGenerator.drawGaussian1D(0, REL_POS_NOISE_STD_KNOWN);
							relPos.z += mrpt::random::randomGenerator.drawGaussian1D(0, REL_POS_NOISE_STD_KNOWN);
						}
						else
						{
							relPos.x += mrpt::random::randomGenerator.drawGaussian1D(0, REL_POS_NOISE_STD_UNKNOWN);
							relPos.y += mrpt::random::randomGenerator.drawGaussian1D(0, REL_POS_NOISE_STD_UNKNOWN);
							relPos.z += mrpt::random::randomGenerator.drawGaussian1D(0, REL_POS_NOISE_STD_UNKNOWN);
						}

						const my_srba_t::TRelativeLandmarkPos rfp= my_srba_t::TRelativeLandmarkPos( curFrameIdx, relPos );

						// Append to list of feats with Known positions:
						new_obs.setRelPos(relPos);
						new_obs.is_fixed                 =  this_feat_has_known_rel_pos;
						new_obs.is_unknown_with_init_val = !this_feat_has_known_rel_pos;

						// Add also to our simulation list, so future observations of this landmark do not contain a known relative pos.
						all_fixed_LMs[LMidx] = rfp;
						all_unknown_LMs_GT[LMidx] = rfp_GT;
					}
					else
					{
						// Already added to the known relative position,
						//  now we are observing the feature as a 2D pixel coords:
						//rba.add_observation(new_obs);

						//if (DEMO_SIMUL_SHOW_GUI) feats_to_draw.push_back( mrpt::vision::TFeatureObservation(LMidx,curFrameIdx,px) );
					}

#endif
					// Append to the list of observations in the current KF:
					new_obs_in_this_frame.push_back( new_obs );

					++obsIdx; // next observation
				} // end while


				ASSERT_(!new_obs_in_this_frame.empty())

				// Append new key_frame to the RBA state:

				//  ======================================
				//  ==          Here is the beef        ==
				//  ======================================
				rba.define_new_keyframe(
					new_obs_in_this_frame,
					new_kf_info,
					true // Optimize?
					);
				//  ======================================

				// Append optimization stat as new entries in the time logger:
				{
					mrpt::utils::CTimeLogger &tl = rba.get_time_profiler();
					tl.registerUserMeasure("num_jacobians", new_kf_info.optimize_results.num_jacobians );
					tl.registerUserMeasure("num_kf2kf_edges_optimized", new_kf_info.optimize_results.num_kf2kf_edges_optimized );
					tl.registerUserMeasure("num_kf2lm_edges_optimized", new_kf_info.optimize_results.num_kf2lm_edges_optimized );
				}

				if (DEBUG_DUMP_ALL_SPANNING_TREES)
				{
					static int i=0;
					if (i==0) {
						if (::system("del spantree_*.*")) {;}
						if (::system("rm spantree_*.*")) {;}
					}

					//const string sFil = mrpt::format("spantree_%05i.txt",i);
					//rba.get_rba_state().spanning_tree.dump_as_text_to_file(sFil);

					vector<srba::TKeyFrameID> roots_to_save;  // leave empty: save all
					roots_to_save.push_back( new_kf_info.kf_id );

					const string sFilDot = mrpt::format("spantree_%05i.dot",i);
					const string sFilPng = mrpt::format("spantree_%05i.png",i);
					rba.get_rba_state().spanning_tree.save_as_dot_file(sFilDot, roots_to_save);

					// Render DOT -> PNG
					if (::system(mrpt::format("dot %s -o %s -Tpng",sFilDot.c_str(), sFilPng.c_str() ).c_str()))
						std::cout << "Error running external dot command...\n";

					i++;
				}

				next_rba_keyframe_ID = 1 + new_kf_info.kf_id;  // Update last KF ID:

				if (obsIdx<nTotalObs)
				{
					ASSERT_EQUAL_(next_rba_keyframe_ID, curFrameIdx)  // This should occur if key_frames in simulation are ordered
				}

			} // end while


			// Draw simulated "camera" image:
			//if (DEMO_SIMUL_SHOW_GUI && win && win->isOpen())
			//{
			//	mrpt::utils::CImage img(rba.parameters.sensor.camera_calib.ncols/2,rba.parameters.sensor.camera_calib.nrows/2,CH_RGB);
			//	img.filledRectangle(0,0,img.getWidth()-1,img.getHeight()-1, mrpt::utils::TColor(0,0,0) );
			//	img.selectTextFont("6x13");

			//	for (mrpt::vision::TSequenceFeatureObservations::const_iterator it=feats_to_draw.begin();it!=feats_to_draw.end();++it)
			//	{
			//		img.cross(it->px.x/2,it->px.y/2, mrpt::utils::TColor::red,'+');
			//		img.textOut(it->px.x/2+2,it->px.y/2+2,mrpt::format("%i",static_cast<int>(it->id_feature)), mrpt::utils::TColor::white);
			//	}

			//	{
			//		win->get3DSceneAndLock();
			//		gl_view2D->setImageView(img);
			//		win->unlockAccess3DScene();
			//	}
			//}

			// Draw 3D scene of the SRBA map:
			if (DEMO_SIMUL_SHOW_GUI && win && win->isOpen())
			{
				// Update text labels:
				win->addTextMessage(
					5,-15,
					mrpt::format("Current KF=%u",static_cast<unsigned int>(new_kf_info.kf_id)),
					mrpt::utils::TColorf(1,1,1), "mono",10, mrpt::opengl::NICE, TXTID_KF,1.5,0.1,
					true /* draw shadow */ );

				// Update 3D objects:
				const srba::TKeyFrameID root_kf = next_rba_keyframe_ID-1;
				typename my_srba_t::TOpenGLRepresentationOptions opengl_params;
				opengl_params.span_tree_max_depth = rba.parameters.srba.max_tree_depth; // Render these past keyframes at most.
				opengl_params.draw_unknown_feats_ellipses_quantiles = 3;

				mrpt::opengl::CSetOfObjectsPtr gl_obj = mrpt::opengl::CSetOfObjects::Create();
				mrpt::opengl::CSetOfObjectsPtr gl_obj_tree = mrpt::opengl::CSetOfObjects::Create();

				rba.build_opengl_representation(
					root_kf,
					opengl_params,
					gl_obj,
					gl_obj_tree
					);


				win->get3DSceneAndLock();

				gl_rba_scene->clear();
				gl_rba_scene->insert( mrpt::opengl::CGridPlaneXY::Create(-50,50,-50,50,0,1) );
				gl_rba_scene->insert(gl_obj);

				gl_view_cur_tree->clear();
				gl_view_cur_tree->insert(gl_obj_tree);

				mrpt::math::TPoint3D bbmin,bbmax;
				gl_obj_tree->getBoundingBox(bbmin,bbmax);
				const double cur_tree_width = 1+std::min(bbmax.x-bbmin.x, bbmax.y-bbmin.y);
				gl_view_cur_tree->getCamera().setZoomDistance(1.5*cur_tree_width);

				win->unlockAccess3DScene();
				win->repaint();
			}

			// Write images to video?
			//if (cfg.arg_video.isSet() && win && win->isOpen())
			//{
			//	// Screenshot:
			//	const mrpt::utils::CImagePtr pImg = win->getLastWindowImagePtr();
			//	if (pImg)
			//	{
			//		// Create video upon first pass:
			//		if (!outVideo.isOpen())
			//		{
			//			outVideo.open(sOutVideo,VIDEO_FPS, pImg->getSize(), "PIM1" );

			//			if (!outVideo.isOpen())
			//				cerr << "Error opening output video file!" << endl;
			//		}

			//		// Write image to video:
			//		outVideo << *pImg;
			//	}
			//}


	#if 0
			cout << "Optimized edges:\n";
			for (srba::RBA_Problem::k2k_edges_t::const_iterator itEdge=rba.get_k2k_edges().begin();itEdge!=rba.get_k2k_edges().end();++itEdge)
			{
				const mrpt::utils::TNodeID nodeFrom = itEdge->from;
				const mrpt::utils::TNodeID nodeTo   = itEdge->to;
				cout << nodeFrom << " -> " << nodeTo << ": " << itEdge->inv_pose << endl;
			}
	#endif
	#if 0
			cout << "Optimized feats:\n";
			const mrpt::srba::TRelativeLandmarkPosMap &unkn_feats = rba.get_unknown_feats();
			// vs: all_unknown_LMs_GT
			for (mrpt::srba::TRelativeLandmarkPosMap::const_iterator it=unkn_feats.begin();it!=unkn_feats.end();++it)
			{
				mrpt::srba::TRelativeLandmarkPosMap::const_iterator it_gt = all_unknown_LMs_GT.find(it->first);
				ASSERT_(it_gt != all_unknown_LMs_GT.end())
				cout << "Feat: " << it->first << " -> " << it->second.pos << " vs GT= " << it_gt->second.pos << endl;
			}
	#endif

			// Profile stats:
			if (SAVE_TIMING_STATS)
			{
				if ((curFrameIdx % SAVE_TIMING_SEGMENT_LENGTH) == 0  )
				{
					// Append time index:
					timming_section_ts.push_back( curFrameIdx );

					// Append times:
					map<string,mrpt::utils::CTimeLogger::TCallStats> cur_stats;
					rba.get_time_profiler().getStats(cur_stats);

					for (map<string,mrpt::utils::CTimeLogger::TCallStats>::const_iterator it=cur_stats.begin();it!=cur_stats.end();++it)
					{
						timming_section_mean_times[ it->first ].push_back( it->second.mean_t );
						timming_section_call_count[ it->first ].push_back( it->second.n_calls );
					}

					// Other useful stats (although not being times):
					timming_section_mean_times[ "total_num_kfs" ].push_back( rba.get_rba_state().keyframes.size() );
					timming_section_mean_times[ "total_num_lms" ].push_back( rba.get_known_feats().size() + rba.get_unknown_feats().size()  );
					timming_section_mean_times[ "total_num_obs" ].push_back( rba.get_rba_state().all_observations.size() );

					rba.get_time_profiler().clear(); // Reset timers stats
				}
			}



			// Process user keys?
			if (win && win->isOpen() && (DEMO_SIMUL_SHOW_STEPBYSTEP || win->keyHit()))
			{
				const int char_code = win->waitForKey();

				switch (char_code)
				{
				case 'r': // Resume:
					DEMO_SIMUL_SHOW_STEPBYSTEP=!DEMO_SIMUL_SHOW_STEPBYSTEP;
					break;
				case 's': // save 3d scene
					{
						static int cnt=0;
						mrpt::opengl::COpenGLScene scene;
						scene.insert(gl_rba_scene);
						const string sFil = mrpt::format("rba-simul-%05i.3Dscene",++cnt);
						cout << "** SAVING 3D SCENE TO: " << sFil << " **\n";
						// This file can be visualized with "SceneViewer3D"
						mrpt::utils::CFileGZOutputStream(sFil) << scene;
					}
					break;
				case 'd': // save DOT graph
					{
						static int cnt=0;
						const string sDotFil = mrpt::format("rba-simul-%05i-full.dot",cnt);
						const string sPngFil = mrpt::format("rba-simul-%05i-full.png",cnt);
						cnt++;

						cout << "** SAVING FULL DOT SCENE TO: " << sDotFil << " **\n";
						rba.save_graph_as_dot(sDotFil, false);

						cout << "Converting to PNG...";
						const string sCmd = mrpt::format("dot -Tpng -o %s %s",sPngFil.c_str(), sDotFil.c_str());
						if (0!=::system(sCmd.c_str()))
						{
							cerr << "> ERROR Running: " << sCmd << endl;
						}
						else
							cout << "Done! " << sPngFil << endl;
					}
					break;

				case 27:  // "ESC" key
					{
						obsIdx=nTotalObs+1; // End simulation
						cout << "Quitting by user command.\n";
					}
					break;
				default:
					break;
				}
			}

			if ((!win || !win->isOpen()) && mrpt::system::os::kbhit())
			{
				const int c = mrpt::system::os::getch();
				switch (c)
				{
				case 27:  // "ESC" key
					{
						obsIdx=nTotalObs+1; // End simulation
						cout << "Quitting by user command.\n";
					}
					break;
				default:
					break;
				}
			}

		} // end for-each frame ID in the dataset


		// Evaluate errors vs. ground truth:
		// ---------------------------------------
#if 0
		if (cfg.arg_eval_overall_se3_error.isSet())
		{
			double total_edge_sqr_err=0;
			for (my_srba_t::k2k_edges_deque_t::const_iterator itEdge=rba.get_k2k_edges().begin();itEdge!=rba.get_k2k_edges().end();++itEdge)
			{
				const mrpt::utils::TNodeID nodeFrom = itEdge->from;
				const mrpt::utils::TNodeID nodeTo   = itEdge->to;

				const mrpt::poses::CPose3D GT_edge = mrpt::poses::CPose3D( GT_path[nodeFrom] - GT_path[nodeTo] ); // (edges store *inverse* poses)
				const mrpt::poses::CPose3D edgeErr = itEdge->inv_pose - GT_edge;
				total_edge_sqr_err+= edgeErr.ln().squaredNorm();
			}
			const double mean_edge_err = sqrt(total_edge_sqr_err/rba.get_k2k_edges().size());
			cout << "Average edge se(3) error: " << mean_edge_err << endl;
		}

		// Evaluate overall observation reprojection errors:
		// ------------------------------------------------------
		if (cfg.arg_eval_overall_sqr_error.isSet())
		{
			cout << "Evaluating overall observation errors...\n"; cout.flush();
			double total_obs_sqr_err = rba.eval_overall_squared_error();
			cout << "total_obs_sqr_err = " << total_obs_sqr_err << endl;

			const size_t nObs = rba.get_rba_state().all_observations.size();
			if (nObs) cout << "RMSE per observation = " << sqrt(total_obs_sqr_err / nObs ) << endl;
		}

		if (cfg.arg_eval_connectivity.isSet())
		{
			{
				double deg_mean, deg_std, deg_max;
				rba.get_rba_state().compute_all_node_degrees(deg_mean, deg_std, deg_max);
				cout << "Keyframe connectivity (degree) stats:\n"
				<< " mean = " << deg_mean << endl
				<< " std  = " << deg_std << endl
				<< " max  = " << deg_max << endl;
			}
			{
				size_t num_min, num_max;
				double num_mean,num_std;
				rba.get_rba_state().spanning_tree.get_stats(num_min, num_max,num_mean,num_std);
				cout << "Spanning tree sizes stats:\n"
				<< " mean = " << num_mean << endl
				<< " std  = " << num_std << endl
				<< " max  = " << num_max << endl
				<< " min  = " << num_min << endl;
			}
		}
#endif

		// Save profile stats to disk
		if (SAVE_TIMING_STATS)
		{
			const string sStatsPrefix = mrpt::format("%s_%s", mrpt::system::fileNameStripInvalidChars(cfg.arg_profile_stats.getValue()).c_str(), mrpt::system::fileNameStripInvalidChars( mrpt::system::dateTimeToString( mrpt::system::getCurrentLocalTime() ) ).c_str() );

			const string sStatsMean = sStatsPrefix + string("_means.csv");
			const string sStatsCalls = sStatsPrefix + string("_calls.csv");

			cout << "Saving time stats to:\n" << sStatsMean << endl << sStatsCalls << endl;

			std::stringstream s_means, s_calls;

			const size_t N = timming_section_ts.size();

			// Header line:
			s_means << "timestep";
			s_calls << "timestep";
			for (map<string,vector<double> >::const_iterator it=timming_section_mean_times.begin();it!=timming_section_mean_times.end();++it)
			{
				if (it->second.size()!=N) continue;

				s_means << "\t \"" << it->first << "\"";
				s_calls << "\t \"" << it->first << "\"";
			}
			s_means << endl;
			s_calls << endl;

			// The rest of lines:
			for (size_t i=0;i<N;i++)
			{
				s_means << timming_section_ts[i];
				for (map<string,vector<double> >::const_iterator it=timming_section_mean_times.begin();it!=timming_section_mean_times.end();++it)
				{
					if (it->second.size()!=N) continue;
					s_means << "\t" << it->second[i];
				}
				s_means << endl;

				s_calls << timming_section_ts[i];
				for (map<string,vector<size_t> >::const_iterator it=timming_section_call_count.begin();it!=timming_section_call_count.end();++it)
				{
					if (it->second.size()!=N) continue;
					s_calls << "\t" << it->second[i];
				}
				s_calls << endl;
			}

			mrpt::utils::CFileOutputStream f_means(sStatsMean), f_calls(sStatsCalls);
			f_means.printf("%s", s_means.str().c_str() );
			f_calls.printf("%s", s_calls.str().c_str() );

			cout << "Done.\n";
		}


		if (cfg.arg_save_final_graph.isSet())
		{
			const string sFil =cfg.arg_save_final_graph.getValue();
			const string sFilPng = mrpt::system::fileNameChangeExtension(sFil,"png");

			cout << "Saving final graph of KFs to: " << sFil << endl;
			rba.save_graph_as_dot(sFil, false /* LMs=don't save */);
			cout << "Done. Converting to PNG...\n";

			// Render DOT -> PNG
			if (!::system(mrpt::format("dot %s -o %s -Tpng",sFil.c_str(), sFilPng.c_str() ).c_str()))
					std::cout << "Done.\n";
			else 	std::cout << "Error running external dot command...\n";
		}

		if (cfg.arg_save_final_graph_landmarks.isSet())
		{
			const string sFil =cfg.arg_save_final_graph_landmarks.getValue();
			cout << "Saving final graph of KFs and LMs to: " << sFil << endl;
			rba.save_graph_as_dot(sFil, true /* LMs=save */);
			cout << "Done.\n";
		}

		if (DEMO_SIMUL_SHOW_GUI && win && win->isOpen())
		{
			cout << "Press any key or close the window to end.\n";
			win->waitForKey();
		}




		return 0; // No error.
	} // end of run()

}; // end of RBA_Run


// ----------- RBA Problem factories ----------------------------
template <class KF2KF_POSE_TYPE,class LM_TYPE,class OBS_TYPE, class SRBA_OPTIONS>
struct RBA_Run_Factory
{
	static auto_ptr<RBA_Run_Base> create()
	{
		return auto_ptr<RBA_Run_Base>(new RBA_Run<KF2KF_POSE_TYPE,LM_TYPE,OBS_TYPE,SRBA_OPTIONS>());
	}
};

// Camera sensors have a different coordinate system wrt the robot (rotated yaw=-90, pitch=0, roll=-90)
struct my_srba_options_cameras
{
	typedef sensor_pose_on_robot_se3 sensor_pose_on_robot_t;
};


int main(int argc, char**argv)
{
	try
	{
		// Parse command-line arguments:
		// ---------------------------------
		RBASLAM_Params  config(argc,argv);

		// Construct the RBA problem object:
		// -----------------------------------
		auto_ptr<RBA_Run_Base> rba;

		if (config.arg_se2.isSet() && config.arg_lm2d.isSet() && config.arg_obs.getValue()=="RangeBearing_2D")
			rba = RBA_Run_Factory<kf2kf_poses::SE2,landmarks::Euclidean2D,observations::RangeBearing_2D,RBA_OPTIONS_DEFAULT>::create();
		else 
		if (config.arg_se3.isSet() && config.arg_lm3d.isSet() && config.arg_obs.getValue()=="StereoCamera")
			rba = RBA_Run_Factory<kf2kf_poses::SE3,landmarks::Euclidean3D,observations::StereoCamera,my_srba_options_cameras>::create();
		else
		{
			throw std::runtime_error("Sorry: the given combination of pose, point and sensor wasn't precompiled in this program!");
		}

		// Run:
		// ------
		return rba->run(config);
	}
	catch (std::exception &e)
	{
		const std::string sErr = std::string(e.what());
		if (!sErr.empty())
			std::cerr << "*EXCEPTION*:\n" << sErr << std::endl;
		return 1;
	}
}

//void optimization_feedback(unsigned int iter, const double total_sq_err, const double mean_sqroot_error)
//{
//	//cout << "iter: " << iter << " msqe=" << mean_sqroot_error << endl;
//}
