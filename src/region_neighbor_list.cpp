/* ----------------------------------------------------------------------
   LIGGGHTS - LAMMPS Improved for General Granular and Granular Heat
   Transfer Simulations

   Copyright 2014-     JKU Linz

   LIGGGHTS is based on LAMMPS
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author:
   Richard Berger (JKU Linz)
------------------------------------------------------------------------- */

#include "lmptype.h"
#include "mpi.h"
#include "bounding_box.h"
#include "region_neighbor_list.h"
#include <limits>
#include "assert.h"

static const double SMALL = 1.0e-6;
static const double BIG = 1.0e20;

using namespace LAMMPS_NS;
using namespace LIGGGHTS;
using namespace std;


/**
 * @brief Default constructor which will create an empty neighbor list
 */
RegionNeighborList::RegionNeighborList() {
}


/**
 * @brief Determine if the given particle overlaps with any particle in this neighbor list
 * @param x        position of particle to check
 * @param radius   radius of particle to check
 * @return true if particle has an overlap with a particle in this neighbor list, false otherwise
 */
bool RegionNeighborList::hasOverlap(double * x, double radius) const {
  int ibin = coord2bin(x);

  for(std::vector<int>::const_iterator it = stencil.begin(); it != stencil.end(); ++it) {
    const int offset = *it;
    assert(ibin+offset >= 0 || (size_t)(ibin+offset) < bins.size());
    const ParticleBin & bin = bins[ibin+offset];

    for(ParticleBin::const_iterator pit = bin.begin(); pit != bin.end(); ++pit) {
      const Particle & p = *pit;
      double del[3];
      vectorSubtract3D(x, p.x, del);
      const double rsq = vectorMag3DSquared(del);
      const double radsum = radius + p.radius;
      if (rsq <= radsum*radsum) return true;
    }
  }

  return false;
}


/**
 * @brief Insert a new particle into neighbor list
 * @param x        position in 3D
 * @param radius   particle radius
 */
void RegionNeighborList::insert(double * x, double radius) {
  int ibin = coord2bin(x);
  assert(ibin >= 0 && ibin <= bins.size());

  bins[ibin].push_back(Particle(x, radius));
  ++ncount;
}


/**
 * @brief Clears neighbor list and brings it into initial state
 */
void RegionNeighborList::reset() {
  bins.clear();
  stencil.clear();
  ncount = 0;
}


/**
 * @brief Returns the number of particles inserted into the neighbor list
 * @return number of particles in neighbor list
 */
size_t RegionNeighborList::count() const {
  return ncount;
}


/**
 * @brief Update the region bounding box
 *
 * This will update internal data structures to ensure they can handle the new
 * region, which is defined by its bounding box.
 *
 * @param bb        bounding box of region
 * @param maxrad    largest particle radius
 * @return true if bounding box was set successfully, false bounding box could not
 * be set and neighbor list is not usable
 */
bool RegionNeighborList::setBoundingBox(BoundingBox & bb, double maxrad) {
  double extent[3];
  bb.getExtent(extent);

  if(extent[0] <= 0.0 || extent[1] <= 0.0 || extent[2] <= 0.0) {
    // empty or invalid region
    bins.clear();
    stencil.clear();
    return false;
  }

  bb.getBoxBounds(bboxlo, bboxhi);

  // testing code
  double binsize_optimal = 4*maxrad;
  double binsizeinv = 1.0/binsize_optimal;

  // test for too many global bins in any dimension due to huge global domain
  const int max_small_int = std::numeric_limits<int>::max();

  if (extent[0]*binsizeinv > max_small_int || extent[1]*binsizeinv > max_small_int ||
      extent[2]*binsizeinv > max_small_int) {
    printf("ERROR: too many bins for this domain\n");
    return false;
  }

  // create actual bins
  nbinx = static_cast<int>(extent[0]*binsizeinv);
  nbiny = static_cast<int>(extent[1]*binsizeinv);
  nbinz = static_cast<int>(extent[2]*binsizeinv);

  if (nbinx == 0) nbinx = 1;
  if (nbiny == 0) nbiny = 1;
  if (nbinz == 0) nbinz = 1;

  binsizex = extent[0]/nbinx;
  binsizey = extent[1]/nbiny;
  binsizez = extent[2]/nbinz;

  bininvx = 1.0 / binsizex;
  bininvy = 1.0 / binsizey;
  bininvz = 1.0 / binsizez;

  // mbinlo/hi = lowest and highest global bins my ghost atoms could be in
  // coord = lowest and highest values of coords for my ghost atoms
  // static_cast(-1.5) = -1, so subract additional -1
  // add in SMALL for round-off safety

  double bsubboxlo[3], bsubboxhi[3];
  bb.getBoxBounds(bsubboxlo, bsubboxhi);

  double coord = bsubboxlo[0] - SMALL*extent[0];
  mbinxlo = static_cast<int> ((coord-bboxlo[0])*bininvx);
  if (coord < bboxlo[0]) mbinxlo = mbinxlo - 1;
  coord = bsubboxhi[0] + SMALL*extent[0];
  int mbinxhi = static_cast<int> ((coord-bboxlo[0])*bininvx);

  coord = bsubboxlo[1] - SMALL*extent[1];
  mbinylo = static_cast<int> ((coord-bboxlo[1])*bininvy);
  if (coord < bboxlo[1]) mbinylo = mbinylo - 1;
  coord = bsubboxhi[1] + SMALL*extent[1];
  int mbinyhi = static_cast<int> ((coord-bboxlo[1])*bininvy);

  coord = bsubboxlo[2] - SMALL*extent[2];
  mbinzlo = static_cast<int> ((coord-bboxlo[2])*bininvz);
  if (coord < bboxlo[2]) mbinzlo = mbinzlo - 1;
  coord = bsubboxhi[2] + SMALL*extent[2];
  int mbinzhi = static_cast<int> ((coord-bboxlo[2])*bininvz);

  // extend bins by 1 to insure stencil extent is included

  mbinxlo = mbinxlo - 1;
  mbinxhi = mbinxhi + 1;
  mbinylo = mbinylo - 1;
  mbinyhi = mbinyhi + 1;
  mbinzlo = mbinzlo - 1;
  mbinzhi = mbinzhi + 1;

  mbinx = mbinxhi - mbinxlo + 1;
  mbiny = mbinyhi - mbinylo + 1;
  mbinz = mbinzhi - mbinzlo + 1;

#ifdef LIGGGHTS_DEBUG
  printf("setting insertion bounding box: [%g, %g] x [%g, %g] x [%g, %g]\n", bsubboxlo[0], bsubboxhi[0], bsubboxlo[1], bsubboxhi[1], bsubboxlo[2], bsubboxhi[2]);
  printf("nbinx %d nbiny %d, nbinz %d\n",nbinx,nbiny,nbinz);
  printf("mbinxlo: %d, mbinxhi: %d\n", mbinxlo, mbinxhi);
  printf("mbinylo: %d, mbinyhi: %d\n", mbinylo, mbinyhi);
  printf("mbinzlo: %d, mbinzhi: %d\n", mbinzlo, mbinzhi);
  printf("mbinx %d mbiny %d, mbinz %d\n",mbinx,mbiny,mbinz);
#endif

  // allocate bins
  bigint bbin = ((bigint) mbinx) * ((bigint) mbiny) * ((bigint) mbinz);
  if (bbin > max_small_int) {
    printf("ERROR: Too many neighbor bins\n");
    return false;
  }
  bins.resize(bbin);

  // generate stencil which will look at all bins 27 bins
  for (int k = -1; k <= 1; k++)
    for (int j = -1; j <= 1; j++)
      for (int i = -1; i <= 1; i++)
        stencil.push_back(k*mbiny*mbinx + j*mbinx + i);

  return true;
}

/**
 * @brief Compute the closest distance between central bin (0,0,0) and bin (i,j,k)
 * @param i bin coordinate along x-axis
 * @param j bin coordinate along y-axis
 * @param k bin coordinate along z-axis
 * @return closest distance between central bin (0,0,0) and bin (i,j,k)
 */
double RegionNeighborList::bin_distance(int i, int j, int k)
{
  double delx,dely,delz;

  if (i > 0) delx = (i-1)*binsizex;
  else if (i == 0) delx = 0.0;
  else delx = (i+1)*binsizex;

  if (j > 0) dely = (j-1)*binsizey;
  else if (j == 0) dely = 0.0;
  else dely = (j+1)*binsizey;

  if (k > 0) delz = (k-1)*binsizez;
  else if (k == 0) delz = 0.0;
  else delz = (k+1)*binsizez;

  return (delx*delx + dely*dely + delz*delz);
}


/**
 * @brief Determine the bin index of a given point x
 * @param x point in 3D
 * @return bin index of the given point x
 */
int RegionNeighborList::coord2bin(double *x) const
{
  int ix,iy,iz;

  if (x[0] >= bboxhi[0])
    ix = static_cast<int> ((x[0]-bboxhi[0])*bininvx) + nbinx;
  else if (x[0] >= bboxlo[0]) {
    ix = static_cast<int> ((x[0]-bboxlo[0])*bininvx);
    ix = std::min(ix,nbinx-1);
  } else
    ix = static_cast<int> ((x[0]-bboxlo[0])*bininvx) - 1;

  if (x[1] >= bboxhi[1])
    iy = static_cast<int> ((x[1]-bboxhi[1])*bininvy) + nbiny;
  else if (x[1] >= bboxlo[1]) {
    iy = static_cast<int> ((x[1]-bboxlo[1])*bininvy);
    iy = std::min(iy,nbiny-1);
  } else
    iy = static_cast<int> ((x[1]-bboxlo[1])*bininvy) - 1;

  if (x[2] >= bboxhi[2])
    iz = static_cast<int> ((x[2]-bboxhi[2])*bininvz) + nbinz;
  else if (x[2] >= bboxlo[2]) {
    iz = static_cast<int> ((x[2]-bboxlo[2])*bininvz);
    iz = std::min(iz,nbinz-1);
  } else
    iz = static_cast<int> ((x[2]-bboxlo[2])*bininvz) - 1;

  return (iz-mbinzlo)*mbiny*mbinx + (iy-mbinylo)*mbinx + (ix-mbinxlo);
}
