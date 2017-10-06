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
  for (int i=0; i<n.get_size(); i++) {
    if (n[i] > maxn) maxn = n[i];
    if (n[i] < minn) minn = n[i];
  }
  return (maxn - minn)/fabs(minn);
}

double find_energy(double temp, double reduced_density, double fv, double gwidth) {
  const double cell_spheres = 4.0;  // number of spheres in one cell when there are no vacancies
  printf("A full cell contains %g sphere(s).\n",  cell_spheres);

  double reduced_num_spheres = cell_spheres*(1-fv); // number of spheres in one cell based on input vacancy fraction fv  
  double vacancy = cell_spheres*fv;
  double lattice_constant = pow(reduced_num_spheres/reduced_density, 1.0/3);  
    
  HomogeneousSFMTFluid hf;
  hf.sigma() = 1;
  hf.epsilon() = 1;   //energy constant in the WCA fluid
  hf.kT() = temp;
  hf.n() = reduced_density;
  hf.mu() = 0;

  printf("Reduced homogeneous density= %g, fraction of vacancies=%g, Gaussian width= %g, temp= %g\n", reduced_density, fv, gwidth, temp); 
    
  printf("Reduced number of spheres in one fluid cell is %g, vacancy is %g spheres.\n", reduced_num_spheres, vacancy); 
  printf("lattice constant = %g\n", lattice_constant);   
  
    // for (double gwidth=0.2; gwidth <=0.4; gwidth+=0.001) {
    //  printf("Reduced homogeneous density= %g, fraction of vacancies=%g, Gaussian width= %g, temp= %g\n", reduced_density, fv, gwidth, temp);

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

    double N_crystal = 0;

    {
      // This is where we set up the inhomogeneous n(r) for a Face Centered Cubic (FCC)
      const int Ntot = f.Nx()*f.Ny()*f.Nz();  //Ntot is the total number of position vectors at which the density will be calculated
      const Vector rrx = f.get_rx();          //Nx is the total number of values for rx etc...
      const Vector rry = f.get_ry();
      const Vector rrz = f.get_rz();
      const double norm = (1-fv)/pow(sqrt(2*M_PI)*gwidth,3); // Using normally normalized Gaussians would correspond to 4 spheres
      // so we need to multiply by (1-fv) to get the reduced number of spheres.
      Vector setn = f.n();

      for (int i=0; i<Ntot; i++) {
        const double rx = rrx[i];
        const double ry = rry[i];
        const double rz = rrz[i];
        setn[i] = 0.0*hf.n(); //sets initial density everywhere to a small value (zero)
        // The FCC cube is set up with one whole sphere in the center of the cube.
        // dist is the magnitude of vector r-vector R=square root of ((rx-Rx)^2 + (ry-Ry)^2 + (rz-Rz)^2)
        // where r is a position vector and R is a vector to the center of a sphere or Gaussian.
        // The following code calculates the contribution to the density
        // at a position vector (rrx[i],rry[i],rrz[i]) from each Guassian
        // and adds them to get the density at that position vector which
        // is then stored in setn[i].
        //NOTE! For this code to give proper results, the Gaussians must
        //have a width that is much smaller than the lattice constant so
        //that parts of the Gaussians that extend into the cube do not
        //extend out the other sides of the cube!
        {
          //R1: Gaussian centered at Rx=0,     Ry=0,    Rz=0
          double dist = sqrt(rx*rx + ry*ry+rz*rz);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);
        }
        {
          //R2: Gaussian centered at Rx=a/2,   Ry=a/2,  Rz=0
          double dist = sqrt((rx-lattice_constant/2)*(rx-lattice_constant/2) +
                             (ry-lattice_constant/2)*(ry-lattice_constant/2) +
                             rz*rz);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);

          //R3: Gaussian centered at Rx=-a/2,  Ry=a/2,  Rz=0
          dist = sqrt((rx+lattice_constant/2)*(rx+lattice_constant/2) +
                      (ry-lattice_constant/2)*(ry-lattice_constant/2) +
                      rz*rz);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);

          //R4: Gaussian centered at Rx=a/2,   Ry=-a/2, Rz=0
          dist = sqrt((rx-lattice_constant/2)*(rx-lattice_constant/2) +
                      (ry+lattice_constant/2)*(ry+lattice_constant/2) +
                      rz*rz);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);

          //R5: Gaussian centered at Rx=-a/2,  Ry=-a/2, Rz=0
          dist = sqrt((rx+lattice_constant/2)*(rx+lattice_constant/2) +
                      (ry+lattice_constant/2)*(ry+lattice_constant/2) +
                      rz*rz);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);
        }
        {
          //R6:  Gaussian centered at Rx=0,    Ry=a/2,  Rz=a/2
          double dist = sqrt((rz-lattice_constant/2)*(rz-lattice_constant/2) +
                             (ry-lattice_constant/2)*(ry-lattice_constant/2) +
                             rx*rx);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);

          //R7:  Gaussian centered at Rx=0,    Ry=a/2,  Rz=-a/2
          dist = sqrt((rz+lattice_constant/2)*(rz+lattice_constant/2) +
                      (ry-lattice_constant/2)*(ry-lattice_constant/2) +
                      rx*rx);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);

          //R8:  Gaussian centered at Rx=0,    Ry=-a/2, Rz=a/2
          dist = sqrt((rz-lattice_constant/2)*(rz-lattice_constant/2) +
                      (ry+lattice_constant/2)*(ry+lattice_constant/2) +
                      rx*rx);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);

          //R9:  Gaussian centered at Rx=0,    Ry=-a/2, Rz=-a/2
          dist = sqrt((rz+lattice_constant/2)*(rz+lattice_constant/2) +
                      (ry+lattice_constant/2)*(ry+lattice_constant/2) +
                      rx*rx);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);
        }
        {
          //R10: Gaussian centered at Rx=a/2,  Ry=0,    Rz=a/2
          double dist = sqrt((rx-lattice_constant/2)*(rx-lattice_constant/2) +
                             (rz-lattice_constant/2)*(rz-lattice_constant/2) +
                             ry*ry);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);

          //R11: Gaussian centered at Rx=-a/2, Ry=0,    Rz=a/2
          dist = sqrt((rx+lattice_constant/2)*(rx+lattice_constant/2) +
                      (rz-lattice_constant/2)*(rz-lattice_constant/2) +
                      ry*ry);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);

          //R12: Gaussian centered at Rx=a/2,  Ry=0,    Rz=-a/2
          dist = sqrt((rx-lattice_constant/2)*(rx-lattice_constant/2) +
                      (rz+lattice_constant/2)*(rz+lattice_constant/2) +
                      ry*ry);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);

          //R13: Gaussian centered at Rx=-a/2,  Ry=0,   Rz=-a/2
          dist = sqrt((rx+lattice_constant/2)*(rx+lattice_constant/2) +
                      (rz+lattice_constant/2)*(rz+lattice_constant/2) +
                      ry*ry);
          setn[i] += norm*exp(-0.5*dist*dist/gwidth/gwidth);
        }
        //Integrate n(r) computationally to check number of spheres in one cell
        N_crystal = (setn[i]*dV) + N_crystal;
      }
      printf("Integrated number of spheres in one crystal cell is %g but we want %g\n",
             N_crystal, reduced_num_spheres);
      setn = setn*(reduced_num_spheres/N_crystal);  //Normalizes setn
      double checking_normalized_num_spheres = 0;
      for (int i=0; i<Ntot; i++) {
        checking_normalized_num_spheres += setn[i]*dV;
      }
      printf("Integrated number of spheres in one crystal cell is NOW %.16g and we want %.16g\n",
             checking_normalized_num_spheres, reduced_num_spheres);
    }
    printf("Integrated number of spheres in one crystal cell is %g but we want %g\n",
           N_crystal, reduced_num_spheres);
    setn = setn*(reduced_num_spheres/N_crystal);  //Normalizes setn
    double checking_normalized_num_spheres = 0;   
    for (int i=0; i<Ntot; i++) {
      checking_normalized_num_spheres += setn[i]*dV;
    }
    //printf("Crystal free energy is %g\n", f.energy());
    double Free_Energy = f.energy();
    printf("Crystal free energy is %g\n", Free_Energy);

    f.printme("Crystal stuff!");
    if (f.energy() != f.energy()) {
      printf("FAIL!  nan for initial energy is bad!\n");
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
  //printf("Crystal free energy is %g\n", f.energy());
  double Free_Energy = f.energy();
  printf("Crystal free energy is %g\n", Free_Energy);
  
  f.printme("Crystal stuff!");
  if (f.energy() != f.energy()) {
    printf("FAIL!  nan for initial energy is bad!\n");
    return f.energy();
  }

  // Find the difference between the homogeneous (fluid) free energy and the crystal free energy 
  double DIFF;   
  DIFF = f.energy() - homogeneous_free_energy;
  printf("DIFF = Crystal Free Energy - Fluid Cell Free Energy = %g \n", DIFF);
  if (f.energy() < homogeneous_free_energy) {
    printf("Crystal Free Energy is LOWER than the Liquid Cell Free Energy!!!\n\n");
  }
    else printf("TRY AGAIN!\n\n");
    
  //Create dataout file - or open file in append mode
  FILE *newmeltoutfile;
  newmeltoutfile = fopen("newmeltdataout.dat", "a");
  //fprintf(newmeltoutfile, "#temp  redensity fv  kT     CryFreeEnergy\n");
  //fprintf(1.3 0.1 0.32 2   3.6   1.40428 -9.17801   -48.192newmeltoutfile, "# %g  %g  %g  %g  %g\n", reduced_density, fv, gwidth,temp, f.energy());
  //fprintf(newmeltoutfile, "#redensity   CrystalFreeEnergy\n");
  //fprintf(newmeltoutfile, "%g %g\n", gwidth, f.energy());
 // fprintf(newmeltoutfile, "%g %g\n", fv, Free_Energy);  // running fv loop
 //  fprintf(newmeltoutfile, "%g %g %g %g   %g   %g %g   %g\n", reduced_density, fv, gwidth, temp, checking_normalized_num_spheres, lattice_constant, homogeneous_free_energy, Free_Energy);  //running gwidth loop
   fprintf(newmeltoutfile, "%g %g %g %g   %g   %g %g   %g   %g\n", reduced_density, fv, gwidth,
          temp, reduced_num_spheres, lattice_constant, homogeneous_free_energy, Free_Energy, Free_Energy/reduced_num_spheres);  //running gwidth loop
  return Free_Energy/reduced_num_spheres;
}

int main(int argc, char **argv) {
  double reduced_density, gwidth, fv, temp; //reduced density is the homogeneous (flat) density accounting for sphere vacancies
  
  //Get inputs from command line
  if (argc != 5) {
    printf("ENTER: %s homogeneous(reduced) density, fraction of vacancies, Gaussian width, kT\n", argv[0]);
    return 1;
  }
  // printf("git version: %s\n", version_identifier());
  assert(sscanf(argv[1], "%lg", &temp) == 1);
  assert(sscanf(argv[2], "%lg", &reduced_density) == 1);
  assert(sscanf(argv[3], "%lg", &fv) == 1);
  assert(sscanf(argv[4], "%lg", &gwidth) == 1);
  printf("Reduced homogeneous density= %g, fraction of vacancies= %g, Gaussian width= %g, temp= %g\n", reduced_density, fv, gwidth, temp);

  if (fv == -1) {
    double best_energy = 1e100;
    double best_fv, best_gwidth;
    for (double fv=0; fv <=1; fv+=0.01) {
      for (double gwidth=0.01; gwidth <= 1; gwidth+=0.01) {
         double e = find_energy(temp, reduced_density, fv, gwidth);
         if (e < best_energy) {
           best_energy = e;
           best_fv = fv;
           best_gwidth = gwidth;
         }
       }
     }
     printf("best fv %g gwidth %g E %g\n", best_fv, best_gwidth, best_energy);
   } else if (gwidth < 0) {
      for (double gwidth=0.001; gwidth <= 5; gwidth+=0.01) {
         find_energy(temp, reduced_density, fv, gwidth);
       }     
   } else {
     find_energy(temp, reduced_density, fv, gwidth);
   }

  return 0;
}
