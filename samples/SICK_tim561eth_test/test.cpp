/**-----------------------------------------------
  * @Copyright (C) 2017 All rights reserved.
  * @filename: test.cpp
  * @author: aliben.develop@gmail.com
  * @created_date: 2017-12-04 15:59:55
  * @last_modified_date: NO_LAST_MODIFIED_DATE
  * @description: TODO
  *-----------------------------------------------*/

/* +------------------------------------------------------------------------+
    |                     Mobile Robot Programming Toolkit (MRPT)            |
    |                          http://www.mrpt.org/                          |
    |                                                                        |
    | Copyright (c) 2005-2018, Individual contributors, see AUTHORS file     |
    | See: http://www.mrpt.org/Authors - All rights reserved.                |
    | Released under BSD License. See details in http://www.mrpt.org/License |
    +------------------------------------------------------------------------+ */

/*
    This example was contributed by Adrien Barral - Robopec (France)
*/

#include <mrpt/gui.h>
#include <mrpt/opengl/CPlanarLaserScan.h>  // [mrpt-maps]
#include <mrpt/hwdrivers/CSICKTim561Eth_2050101.h>

using namespace mrpt;
using namespace mrpt::utils;
using namespace mrpt::obs;
using namespace mrpt::opengl;
using namespace mrpt::hwdrivers;
using namespace mrpt::gui;
using namespace std;

int main(int argc, char* argv[])
{
  /** A process for testing Lidar with MRPT
   * 1. New a Lidar class with <IP> <Port>
   * 2. Turn on the Lidar just created
   * 3. Get Lidar data into outObs
   */
  if (argc < 3)
  {
   cout << "Usage : " << argv[0] << " <IP> <port> " << endl;
   return 0;
  }

  CSICKTim561Eth laser(string(argv[1]), atoi(argv[2]));
  laser.turnOn();

  bool isOutObs, hardwareError;
  CObservation2DRangeScan outObs;
  laser.doProcessSimple(isOutObs, outObs, hardwareError);

  CDisplayWindow3D win3D("Scan", 200, 200);

  COpenGLScene::Ptr ptr_scene = win3D.get3DSceneAndLock();

  opengl::CPlanarLaserScan::Ptr obj =
   mrpt::make_aligned_shared<opengl::CPlanarLaserScan>();
  obj->clear();
  obj->setColor(0, 0, 1);
  obj->setName("scan_TIM561");
  obj->setScan(outObs);
  ptr_scene->insert(obj);

  win3D.unlockAccess3DScene();
  win3D.forceRepaint();

  while (win3D.isOpen())
  {
   laser.doProcessSimple(isOutObs, outObs, hardwareError);

   ptr_scene = win3D.get3DSceneAndLock();
   opengl::CPlanarLaserScan::Ptr obj =
    std::dynamic_pointer_cast<opengl::CPlanarLaserScan>(
     ptr_scene->getByName("scan_TIM561"));
   obj->clear();
   obj->setScan(outObs);
   win3D.unlockAccess3DScene();
   win3D.forceRepaint();
   std::this_thread::sleep_for(20ms);
  }
  win3D.waitForKey();
  return 0;
}
