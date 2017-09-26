// Deft is a density functional package developed by the research
// group of Professor David Roundy
//
// Copyright 2010 The Deft Authors
//
// Deft is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// You should have received a copy of the GNU General Public License
// along with deft; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
// Please see the file AUTHORS for a list of authors.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include "new/SFMTFluidFast.h"
#include "new/HomogeneousSFMTFluidFast.h"
#include "new/Minimize.h"
#include "version-identifier.h"

static void took(const char *name) {
  static clock_t last_time = clock();
  clock_t t = clock();
  assert(name); // so it'll count as being used...
  double peak = peak_memory()/1024.0/1024;
  printf("\t\t%s took %g seconds and %g M memory\n", name, (t-last_time)/double(CLOCKS_PER_SEC), peak);
  fflush(stdout);
  last_time = t;
}

double inhomogeneity(Vector n) {
  double maxn = n[0];
  double minn = n[0];
  for (int i=0;i<n.get_size();i++) {
    if (n[i] > maxn) maxn = n[i];
    if (n[i] < minn) minn = n[i];
  }
  return (maxn - minn)/fabs(minn);
}


int main(int argc, char **argv) {
  double lattice_constant; 
  double reduced_density, gwidth, fv, temp;   // reduced density is the homogeneous (flat) density
  double N_crystal, crystal_density;  //number of spheres and density computed by integrating n(r)
  double cell_spheres;  // number of spheres that FILL one cell (no vacancies)
  double reduced_num_spheres;  // number of spheres in one cell based on input vacancy fraction fv  
  
  //Get inputs from command line
  if (argc != 5) {
    printf("ENTER: %s homogeneous(reduced) density, fraction of vacancies, Gaussian width, kT\n", argv[0]);
    return 1;
  }
  // printf("git version: %s\n", version_identifier());
  assert(sscanf(argv[1], "%lg", &reduced_density) == 1);
  assert(sscanf(argv[2], "%lg", &fv) == 1);
  assert(sscanf(argv[3], "%lg", &gwidth) == 1);
  assert(sscanf(argv[4], "%lg", &temp) == 1);
  printf("Homogeneous(reduced) Density= %g, fraction of vacancies= %g, Gaussian width= %g, temp= %g\n", reduced_density, fv, gwidth, temp);
  
  cell_spheres = 1.0;   // the number of spheres that fill a fluid cell -ASK (should be 4 for FCC! not 1!)
  reduced_num_spheres = cell_spheres*(1-fv);   //the reduced number of spheres in a fluid cell
  printf("A full cell contains %g sphere(s).\n",  cell_spheres);
  printf("Reduced number of spheres in one fluid cell is %g, vacancy is %g spheres.\n", reduced_num_spheres, cell_spheres*fv); 
  
  lattice_constant = pow(reduced_num_spheres/reduced_density, 1.0/3);      
  printf("lattice constant = %g\n", lattice_constant);    
  
  HomogeneousSFMTFluid hf;
  hf.sigma() = 1;
  hf.epsilon() = 1;   //energy constant in the WCA fluid
  hf.kT() = temp;
  hf.n() = reduced_density;
  hf.mu() = 0;
  // hf.mu() = hf.d_by_dn(); // we will set mu based on the derivative of hf

  const double homogeneous_free_energy = hf.energy()*lattice_constant*lattice_constant*lattice_constant;  
  printf("Bulk energy is %g\n", hf.energy());
  printf("Fluid cell free energy should be %g\n", homogeneous_free_energy);

  const double dx = 0.05;         //grid point spacing dx=dy=dz=0.05
  const double dV = pow(0.05,3);  //volume element dV  
  SFMTFluid f(lattice_constant, lattice_constant, lattice_constant, dx);   
  f.sigma() = hf.sigma();
  f.epsilon() = hf.epsilon();
  f.kT() = hf.kT();
  f.mu() = hf.mu();
  f.Vext() = 0;
  f.n() = hf.n();

  {
    // This is where we set up the inhomogeneous n(r) for a Face Centered Cubic (FCC)
    const int Ntot = f.Nx()*f.Ny()*f.Nz();  //Ntot is the total number of position vectors at which the density will be calculated
    const Vector rrx = f.get_rx();          //Nx is the total number of values for rx  etc...
    const Vector rry = f.get_ry();
    const Vector rrz = f.get_rz();
    const double norm = (1/reduced_num_spheres)*pow(sqrt(2*M_PI)*gwidth, 3); 

    Vector setn = f.n();
    N_crystal = 0.0000001;  //sets the initial value for computing the number of spheres in the crystal to a small value other than zero
    
    for (int i=0; i<Ntot; i++) {
      const double rx = rrx[i];
      const double ry = rry[i];
      const double rz = rrz[i]; 
      setn[i] = 0.0000001*hf.n();  //sets the initial value for the density to be everywhere a small value other than zero
      // The FCC cube is set up with one whole sphere in the center of the cube
      // dist is the magnitude of vector r - vector R = square root of ((rx-Rx)^2 + (ry-Ry)^2 + (rz-Rz)^2)  
      // where r is the position vector and R is a vector to the center of a sphere
      // The following code calculates the contribution to the density at a position vector (rx,ry,rz) from each Guassian
      // and adds them together to get the density at position vector (rx, ry, rz) which is then stored in setn[i]
      {                            
        double dist = sqrt(rx*rx + ry*ry+rz*rz);                           
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R1: Gaussian centered at the origin  Rx=0, Ry=0, Rz=0 
      }
      {
        double dist = sqrt((rx-lattice_constant/2)*(rx-lattice_constant/2) +
                           (ry-lattice_constant/2)*(ry-lattice_constant/2) +
                           rz*rz);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R2: Gaussian centered at Rx=a/2, Ry=a/2, Rz=0

        dist = sqrt((rx+lattice_constant/2)*(rx+lattice_constant/2) +
                    (ry-lattice_constant/2)*(ry-lattice_constant/2) +
                    rz*rz);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R3: Gaussian centered at Rx=-a/2, Ry=a/2,  Rz=0 

        dist = sqrt((rx-lattice_constant/2)*(rx-lattice_constant/2) +
                    (ry+lattice_constant/2)*(ry+lattice_constant/2) +
                    rz*rz);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R4: Gaussian centered at Rx=a/2,  Ry=-a/2,  Rz=0

        dist = sqrt((rx+lattice_constant/2)*(rx+lattice_constant/2) +
                    (ry+lattice_constant/2)*(ry+lattice_constant/2) +
                    rz*rz);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R5: Gaussian centered at Rx=-a/2, Ry=-a/2,  Rz=0
      }
      {
        double dist = sqrt((rz-lattice_constant/2)*(rz-lattice_constant/2) +
                           (ry-lattice_constant/2)*(ry-lattice_constant/2) +
                           rx*rx);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R6: Gaussian centered at Rx=0, Ry=a/2,  Rz=a/2

        dist = sqrt((rz+lattice_constant/2)*(rz+lattice_constant/2) +
                    (ry-lattice_constant/2)*(ry-lattice_constant/2) +
                    rx*rx);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R7: Gaussian centered at Rx=0, Ry=a/2, Rz=-a/2 

        dist = sqrt((rz-lattice_constant/2)*(rz-lattice_constant/2) +
                    (ry+lattice_constant/2)*(ry+lattice_constant/2) +
                    rx*rx);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R8: Gaussian centered at Rx=0, Ry=-a/2, Rz=a/2 

        dist = sqrt((rz+lattice_constant/2)*(rz+lattice_constant/2) +
                    (ry+lattice_constant/2)*(ry+lattice_constant/2) +
                    rx*rx);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R9: Gaussian centered at Rx=0, Ry=-a/2, Rz=-a/2
      }
      {
        double dist = sqrt((rx-lattice_constant/2)*(rx-lattice_constant/2) +
                           (rz-lattice_constant/2)*(rz-lattice_constant/2) +
                           ry*ry);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R10: Gaussian centered at Rx=a/2, Ry=0, Rz=a/2

        dist = sqrt((rx+lattice_constant/2)*(rx+lattice_constant/2) +
                    (rz-lattice_constant/2)*(rz-lattice_constant/2) +
                    ry*ry);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R11: Gaussian centered at Rx=-a/2,  Ry=0, Rz=a/2 

        dist = sqrt((rx-lattice_constant/2)*(rx-lattice_constant/2) +
                    (rz+lattice_constant/2)*(rz+lattice_constant/2) +
                    ry*ry);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R12: Gaussian centered at Rx=a/2,  Ry=0, Rz=-a/2 

        dist = sqrt((rx+lattice_constant/2)*(rx+lattice_constant/2) +
                    (rz+lattice_constant/2)*(rz+lattice_constant/2) +
                    ry*ry);
        setn[i] += exp(-0.5*dist*dist/gwidth/gwidth)/norm;                  //R13: Gaussian centered at Rx=-a/2, Ry=0, Rz=-a/2 
        
        N_crystal = (setn[i]*dV) + N_crystal;  //Calculates the number of spheres in one crystal cell
      }
     }
    }


  crystal_density = N_crystal/pow(lattice_constant,3);

  if (false) {
    char *fname = new char[5000];
    mkdir("papers/fuzzy-fmt/figs/new-data", 0777); // make sure the directory exists
    snprintf(fname, 5000, "papers/fuzzy-fmt/figs/new-data/initial-melting-%04.2f-%04.2f-%04.2f.dat",
             lattice_constant, reduced_density, temp);
    FILE *o = fopen(fname, "w");
    if (!o) {
      fprintf(stderr, "error creating file %s\n", fname);
      exit(1);
    }
    delete[] fname;
    const int Nz = f.Nz();
    Vector rz = f.get_rz();
    Vector n = f.n();
    for (int i=0;i<Nz/2;i++) {
      fprintf(o, "%g\t%g\n", rz[i], n[i]);
    }
    fclose(o);
  }

  printf("Crystal free energy is %g\n", f.energy());
  printf("Number of spheres in one crystal cell is %g\n", N_crystal);
  printf("corresponding to a homogeneous density of %g\n", crystal_density);
  
   hf.n() = crystal_density;
  
  printf("And now the Bulk energy is %g\n", hf.energy());
 // homogeneous_free_energy = hf.energy()*lattice_constant*lattice_constant*lattice_constant;
  printf("And now the Fluid cell free energy should be %g\n", hf.energy()*lattice_constant*lattice_constant*lattice_constant);
  
  f.printme("Crystal stuff!");
  if (f.energy() != f.energy()) {
    printf("FAIL!  nan for initial energy is bad!\n");
    exit(1);
  }
 
  // Find the difference between the homogeneous (fluid) free energy and the crystal free energy 
  double DIFF;   
  DIFF = f.energy() - (hf.energy()*lattice_constant*lattice_constant*lattice_constant);
  printf("DIFF = Crystal Free Energy - Fluid Cell Free Energy = %g \n", DIFF);
//  if (f.energy() < homogeneous_free_energy) {
  if (f.energy() < hf.energy()*lattice_constant*lattice_constant*lattice_constant) {
    printf("Crystal Free Energy is LOWER than the Fluid Cell Free Energy!!!\n");
  }
    else printf("TRY AGAIN!\n");

  return 0;
}
