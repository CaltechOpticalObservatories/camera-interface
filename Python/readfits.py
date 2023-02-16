import os
from astropy.io import fits
import numpy as np

def ReadNIRC2Fits (filename):
    nirc2_hdul = fits.open(filename) #HDU list
    nirc2_hdr = nirc2_hdul[0].header # Header of Primary HDU
    
    try:
        nirc2_hdr['SAMPMODE']
    except:
        frames = 0
        print("There is no SAMPMODE Keyword in this .fits file")
    else:   
        n_ext = len(nirc2_hdul)-1 # gets the number of extension in the fits file
        frames = np.zeros((n_ext,nirc2_hdul[1].header['NAXIS3'],nirc2_hdul[1].header['NAXIS2'],nirc2_hdul[1].header['NAXIS1']))
        print(frames.shape)
        for e in np.arange(n_ext):
            print(e)
            frames[e,:,:,:] = np.array(nirc2_hdul[e+1].data, dtype=np.int32)
    return frames
    
