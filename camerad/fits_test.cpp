/**
 \file fits_test.cpp
 \brief FITS file writing test software
 \details This program tests the writing capability of the FITS file writing 
 system.  The code gives an example of how to set up information for the FITS 
 file through the camera_info object, and then shows how to write both a FITS 
 file consisting of a single image and a multi-extension FITS cube.  The cube 
 loops through to create images with a delay in the loop, so performance testing
 of the FITS system can be done by changing the delay.  
 \author Dr. Reed L. Riddle  riddle@caltech.edu
 \note This is a slimmed down version of the FITS data management system from
 the Robo-AO project.  It is a method to write FITS data files from a system,
 such as an AO system, that generates files at a high frequency.  This package 
 has removed many features from the software that aren't necessary if you just
 want to write a FITS file to keep this focused on only that operation.
 */


// Global include files
# include <iostream>

// Local include files
//# include "common.h"
#include "camera.h"
# include "fits_file.h"

/**************** main ****************/
/**
 The main routine.  This launches the routine to take a frame with the CCD39.
 \param [argc] Command line argument count
 \param [argv] Array of character strings for each command line argument
 \note None.
 */
int main(int argc, char *argv[])
{
	// Allocate the FITS file objects, the camera_info object, and a base name
  FITS_file <uint32_t> fitsfile1(false);
  FITS_file <int16_t> fitsfile2(true);
  Camera::Information camera_info;
  std::string image_name("test_1.fits");
  
  // Set up some image parameters
  camera_info.naxes[0] = 128;
  camera_info.naxes[1] = 128;
//camera_info.set_observation(image_name, Camera::IMAGE_OBJECT);
  camera_info.image_name = image_name;
  camera_info.basename = image_name;
  camera_info.region_of_interest[0] = 1;
  camera_info.region_of_interest[1] = 128;
  camera_info.region_of_interest[2] = 1;
  camera_info.region_of_interest[3] = 128;
  camera_info.binning[0] = 1;
  camera_info.binning[1] = camera_info.binning[0];
  camera_info.set_axes(LONG_IMG);
  camera_info.directory = "/tmp";
  camera_info.fits_name = camera_info.directory+"/"+camera_info.image_name;

	// Always need an image time stamp
  std::string cube_time_stamp = get_system_date(); //get_current_time(FILENAME_MICROSECOND);

  // Set up the data array for the single image
  uint32_t *data1;
  data1 = new uint32_t[camera_info.image_size];
  data1[0] = 1;
  data1[1] = 2;
  data1[3] = 3;
  
  // Take a single image and write it to a single FITS file
  fitsfile1.write_image(data1, cube_time_stamp, -1, camera_info);
  delete [] data1;

  // Now, make the camera output into a multi-extension FITS file
  camera_info.iscube = true;
  
  // Set up the image parameter and data array
  image_name = "test_2.fits";
  camera_info.set_axes(SHORT_IMG);
  camera_info.image_name = image_name;
  camera_info.fits_name = camera_info.directory+"/"+camera_info.image_name;
//camera_info.set_observation(image_name, Camera::IMAGE_OBJECT);
  int16_t *data;
  data = new int16_t[camera_info.image_size];
  // Write all of the data pixles to a value
  for (uint32_t j = 0; j < camera_info.image_size; j++) {
    data[j] = j;// * (i + 1);
  }

  // Write 50,000 images to the FITS data system. It will continue to write 
  // images until it finishes, creating new FITS multi-extension files as 
  // necessary.  
  std::stringstream bob;
  for (int i = 0; i < 50000; i++){
    // Set the first frame pixel value so we can track and make sure we are 
    // writing frames properly
    data[0] = i;
  	// Set the frmae time stamp
    bob << std::setprecision(24) << get_clock_time();
    cube_time_stamp = bob.str();
    bob.str("");
    // Add the image data to the FITS image queue
    fitsfile2.write_image(data, cube_time_stamp, i-1, camera_info);
    // Pause before getting the next "image".  You can make this number 
    // larger or smaller to test your setup, if you comment this line out the
    // loop will dump all of the frames into the FITS queue at once, which
    // may cause an issue with system memory if the frames are too large.
    timeout(0.0001);
  }

  // Tell the FITS system there are no more frames to add so it finishes 
  // writing the frames in its queue and finishes.
  fitsfile2.complete();

  // Exit nicely
  exit(0);
}
