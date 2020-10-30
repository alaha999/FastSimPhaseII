import FWCore.ParameterSet.Config as cms

from FastSimulation.Configuration.Geometries_cff import *

# Apply Tracker and Muon misalignment
misalignedTrackerGeometry.applyAlignment = False
misalignedDTGeometry.applyAlignment = True
misalignedCSCGeometry.applyAlignment = True
