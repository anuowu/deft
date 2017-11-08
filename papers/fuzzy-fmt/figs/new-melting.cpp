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
#include <popt.h>
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

struct data {
  double diff_free_energy_per_atom;
  double cfree_energy_per_atom;
  double hfree_energy_per_vol;
  double cfree_energy_per_vol;
  char *dataoutfile_name;
};

double find_lattice_constant(double reduced_density, double fv) {
  return pow(4*(1-fv)/reduced_density, 1.0/3);
}

data find_energy(double temp, double reduced_density, double fv, double gwidth, char *data_dir, double dx, bool verbose=false) {
  double reduced_num_spheres = 4*(1-fv); // number of spheres in one cell based on input vacancy fraction fv
  double vacancy = 4*fv;                 //there are 4 spheres in one cell when there are no vacancies (fv=1)
  double lattice_constant = find_lattice_constant(reduced_density, fv);
  double dV = pow(dx,3);  //volume element dV

  HomogeneousSFMTFluid hf;
  hf.sigma() = 1;
  hf.epsilon() = 1;   //energy constant in the WCA fluid
  hf.kT() = temp;
  hf.n() = reduced_density;
  hf.mu() = 0;
  //Note: hf.energy() returns energy/volume

  if (verbose) {
    //printf("Reduced homogeneous density= %g, fraction of vacancies= %g, Gaussian width= %g, temp= %g\n",
    //       reduced_density, fv, gwidth, temp);

    printf("Reduced number of spheres in one fluid cell is %g, vacancy is %g spheres.\n",
           reduced_num_spheres, vacancy);
    printf("lattice constant = %g\n", lattice_constant);
  }

  const double homogeneous_free_energy = hf.energy()/reduced_density; // energy per sphere
  if (verbose) {
    printf("Bulk energy per volume is %g\n", hf.energy());
    printf("Homogeneous free energy per sphere is %g\n", homogeneous_free_energy);
  }

  SFMTFluid f(lattice_constant, lattice_constant, lattice_constant, dx);
  f.sigma() = hf.sigma();
  f.epsilon() = hf.epsilon();
  f.kT() = hf.kT();
  f.mu() = hf.mu();
  f.Vext() = 0;
  f.n() = hf.n();
  //Note: f.energy() returns energy (not energy/volume like hf.energy()!)

  double N_crystal = 0;

  {
    // This is where we set up the inhomogeneous n(r) for a Face Centered Cubic (FCC)
    const int Ntot = f.Nx()*f.Ny()*f.Nz();  //Ntot is the total number of position vectors at which the density will be calculated
    const Vector rrx = f.get_rx();          //Nx is the total number of values for rx etc...
    const Vector rry = f.get_ry();
    const Vector rrz = f.get_rz();
    const double norm = (1-fv)/pow(sqrt(2*M_PI)*gwidth,3); // Normalized Gaussians correspond to 4 spheres/atoms for no vacancies
    // multiply 4 by (1-fv) to get the reduced number of spheres.
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
    if (verbose) {
      printf("Integrated number of spheres in one crystal cell is %g but we want %g\n",
             N_crystal, reduced_num_spheres);
    }
    setn = setn*(reduced_num_spheres/N_crystal);  //Normalizes setn
    if (verbose) {
      double checking_normalized_num_spheres = 0;
      for (int i=0; i<Ntot; i++) {
        checking_normalized_num_spheres += setn[i]*dV;
      }
      printf("Integrated number of spheres in one crystal cell is NOW %.16g and we want %.16g\n",
             checking_normalized_num_spheres, reduced_num_spheres);
    }
  }

  //printf("crystal free energy is %g\n", f.energy());
  double crystal_free_energy = f.energy()/reduced_num_spheres; // free energy per atom
  data data_out;
  data_out.diff_free_energy_per_atom=crystal_free_energy - homogeneous_free_energy;
  data_out.cfree_energy_per_atom=crystal_free_energy;
  data_out.hfree_energy_per_vol=hf.energy();
  data_out.cfree_energy_per_vol=f.energy()/pow(lattice_constant,3);

  if (verbose) {
    printf("Crystal free energy is %g\n", crystal_free_energy);

    f.printme("Crystal stuff!");
    if (f.energy() != f.energy()) {
      printf("FAIL!  nan for initial energy is bad!\n");
    }

    if (crystal_free_energy < homogeneous_free_energy) {
      printf("Crystal Free Energy is LOWER than the Liquid Cell Free Energy!!!\n\n");
    } else printf("TRY AGAIN!\n\n");
  }

  // Create all output data filename
  char *alldat_filename = new char[1024];
  char *alldat_filedescriptor = new char[1024];
  sprintf(alldat_filedescriptor, "kT%5.3f_n%05.3f_fv%04.2f_gw%04.3f",
          temp, reduced_density, fv, gwidth);
  sprintf(alldat_filename, "%s/%s-alldat.dat", data_dir, alldat_filedescriptor);
  //sprintf(alldat_filename, "%s/kT%5.3f_n%05.3f_fv%04.2f_gw%04.3f-alldat.dat",
  //        data_dir, temp, reduced_density, fv, gwidth);
  printf("Create data file: %s\n", alldat_filename);
  data_out.dataoutfile_name=alldat_filedescriptor;

  //Create dataout file
  FILE *newmeltoutfile = fopen(alldat_filename, "w");
  if (newmeltoutfile) {
    fprintf(newmeltoutfile, "# git  version: %s\n", version_identifier());
    fprintf(newmeltoutfile, "#T\tn\tfv\tgwidth\thFreeEnergy/atom\tcFreeEnergy/atom\tFEdiff/atom\tlattice_constant\tNsph\n");
    fprintf(newmeltoutfile, "%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\n",
            temp, reduced_density, fv, gwidth, homogeneous_free_energy,
            crystal_free_energy, crystal_free_energy-homogeneous_free_energy,
            lattice_constant, reduced_num_spheres);
    fclose(newmeltoutfile);
  } else {
    printf("Unable to open file %s!\n", alldat_filename);
  }
  return data_out;
}

//+++++++++++++++++++++++++++++++Downhill Simplex+++++++++++++++++++++++++++++++

struct point_fe {
  double fv;
  double gw;
  double fe;
};

struct points_fe {
  point_fe in;
  point_fe out;
};

void display_simplex(double simplex_fe[3][3]) {
  printf("\n");
  for (int k=0; k<3; k++) {
    for(int l=0; l<3; l++) {
      printf("%g\t", simplex_fe[k][l]);
    }
    printf("\n");
  }
  printf("\n");
}

void evaluate_simplex(double temp, double reduced_density, double simplex_fe[3][3], char *data_dir, double dx, bool verbose) {
  for (int k=0; k<3; k++) {
    data dhill_data=find_energy(temp, reduced_density, simplex_fe[k][0], simplex_fe[k][1], data_dir, dx, verbose);
    simplex_fe[k][2]=dhill_data.cfree_energy_per_atom;
    printf("simplex_fe[%i][2]=%g\n", k, simplex_fe[k][2]);
  }
}

void sort_simplex(double simplex_fe[3][3]) {
  double holdfe[3];
  for (int i =0; i < 2; ++i) {    //standard sorting algorithm
    for (int j=i+1; j < 3; j++) {
      if (simplex_fe[i][2] > simplex_fe[j][2]) {
        for (int m=0; m < 3; m++) {
          holdfe[m]=simplex_fe[i][m];
          simplex_fe[i][m]=simplex_fe[j][m];
          simplex_fe[j][m]=holdfe[m];
        }
      }
    }
  }
}

point_fe reflect_simplex(double temp, double reduced_density, double simplex_fe[3][3], char *data_dir, double dx, bool verbose) {
  point_fe reflected;
  reflected.fv=simplex_fe[0][0]+simplex_fe[1][0]-simplex_fe[2][0];
  reflected.gw=simplex_fe[0][1]+simplex_fe[1][1]-simplex_fe[2][1];
  data reflect=find_energy(temp, reduced_density, reflected.fv, reflected.gw, data_dir, dx, verbose);
  reflected.fe=reflect.cfree_energy_per_atom;
  return reflected;
}

point_fe extend_simplex(double temp, double reduced_density, double simplex_fe[3][3], char *data_dir, double dx, bool verbose) {
  point_fe extended;
  extended.fv=(3/2.0)*(simplex_fe[0][0]+simplex_fe[1][0])-2.0*simplex_fe[2][0];
  extended.gw=(3/2.0)*(simplex_fe[0][1]+simplex_fe[1][1])-2.0*simplex_fe[2][1];
  data extend=find_energy(temp, reduced_density, extended.fv, extended.gw, data_dir, dx, verbose);
  extended.fe=extend.cfree_energy_per_atom;
  return extended;
}

points_fe contract_simplex(double temp, double reduced_density, double simplex_fe[3][3], char *data_dir, double dx, bool verbose) {
  points_fe contracted;
  
  printf("working with simplex:\n"); //debug
  display_simplex(   simplex_fe);   //debug

  contracted.out.fv=((3/4.0)*(simplex_fe[0][0]+simplex_fe[1][0]))-((1/2.0)*(simplex_fe[2][0]));
  contracted.out.gw=((3/4.0)*(simplex_fe[0][1]+simplex_fe[1][1]))-((1/2.0)*(simplex_fe[2][1]));
  printf("contracted.out.fv=%g, contracted.out.gw=%g\n", contracted.out.fv, contracted.out.gw);   //debug
  data contract_out=find_energy(temp, reduced_density, contracted.out.fv, contracted.out.gw, data_dir, dx, verbose);
  contracted.in.fe=contract_out.cfree_energy_per_atom;

  contracted.in.fv=((1/4.0)*(simplex_fe[0][0]+simplex_fe[1][0]))-((1/2.0)*(simplex_fe[2][0]));
  contracted.in.gw=((1/4.0)*(simplex_fe[0][1]+simplex_fe[1][1]))-((1/2.0)*(simplex_fe[2][1]));
  printf("contracted.in.fv=%g, contracted.in.gw=%g\n", contracted.in.fv, contracted.in.gw);   //debug
  data contract_in=find_energy(temp, reduced_density, contracted.in.fv, contracted.in.gw, data_dir, dx, verbose);
  contracted.in.fe=contract_in.cfree_energy_per_atom;
  return contracted;
}

points_fe shrink_simplex(double temp, double reduced_density, double simplex_fe[3][3], char *data_dir, double dx, bool verbose) {
  points_fe shrunken;

  shrunken.out.fv=(1/2.0)*(simplex_fe[0][0] + simplex_fe[1][0]);   //using in/out so don't have to make another structure
  shrunken.out.gw=(1/2.0)*(simplex_fe[0][1] + simplex_fe[1][1]);
  shrunken.out.fe=find_energy(temp, reduced_density, shrunken.out.fv, shrunken.out.gw,
                              data_dir, dx, verbose).cfree_energy_per_atom;

  shrunken.in.fv=(1/2.0)*(simplex_fe[0][0] + simplex_fe[2][0]);
  shrunken.in.gw=(1/2.0)*(simplex_fe[0][1] + simplex_fe[2][1]);
  shrunken.in.fe=find_energy(temp, reduced_density, shrunken.in.fv, shrunken.in.gw,
                             data_dir, dx, verbose).cfree_energy_per_atom;
  return shrunken;
}

//check_convergence_simplex() {
//}

void advance_simplex(double temp, double reduced_density, double simplex_fe[3][3], char *data_dir, double dx, bool verbose) {
      printf("working with simplex:\n"); //debug
      display_simplex(   simplex_fe);   //debug
      printf("   reflect simplex\n");  //debug
  point_fe reflected_point=reflect_simplex(temp, reduced_density, simplex_fe, data_dir, dx, verbose);
      printf("   reflected_point.fv=%g, reflected_point.gw=%g, reflected_point.fe=%g\n", reflected_point.fv, reflected_point.gw, reflected_point.fe);  //debug
  if (simplex_fe[0][2]  < reflected_point.fe && reflected_point.fe < simplex_fe[1][2]) {
    simplex_fe[2][0]=reflected_point.fv;
    simplex_fe[2][1]=reflected_point.gw;
    simplex_fe[2][2]=reflected_point.fe;
    printf("   **simplex reflected A\n");  //debug
    display_simplex(   simplex_fe);   //debug
    return;
  }
  if (reflected_point.fe < simplex_fe[0][2]) {
    point_fe extended_point=extend_simplex(temp, reduced_density, simplex_fe, data_dir, dx, verbose);
    printf("   extended_point.fv=%g, extended_point.gw=%g, extended_point.fe=%g\n", extended_point.fv, extended_point.gw, extended_point.fe);  //debug
    if (extended_point.fe < reflected_point.fe) {
      simplex_fe[2][0]=extended_point.fv;
      simplex_fe[2][1]=extended_point.gw;
      simplex_fe[2][2]=extended_point.fe;
      printf("   **simplex reflected B and extended\n");  //debug
      display_simplex(   simplex_fe);   //debug
    } else {
      simplex_fe[2][0]=reflected_point.fv;
      simplex_fe[2][1]=reflected_point.gw;
      simplex_fe[2][2]=reflected_point.fe;
      printf("   **simplex reflected C but not extended\n");  //debug
      display_simplex(   simplex_fe);  //debug
    }
    return;
  }
  printf("   contract simplex\n");  //debug
  printf("working with simplex:\n"); //debug
  display_simplex(   simplex_fe);   //debug
  points_fe contracted_points=contract_simplex(temp, reduced_density, simplex_fe, data_dir, dx, verbose);
  printf("   contracted_points.in.fv=%g, contracted_points.in.gw=%g, contracted_points.in.fe=%g\n", contracted_points.in.fv, contracted_points.in.gw, contracted_points.in.fe);  //debug
  printf("   contracted_points.out.fv=%g, contracted_points.out.gw=%g, contracted_points.out.fe=%g\n", contracted_points.out.fv, contracted_points.out.gw, contracted_points.out.fe);  //debug
  point_fe better_contracted_point;
  if (contracted_points.out.fe < contracted_points.in.fe) {
    better_contracted_point=contracted_points.out;  //there's probably a problem with this!
    printf("better_contracted_point.fv=%g, better_contracted_point.gw=%g, better_contracted_point.fe=%g\n", better_contracted_point.fv, better_contracted_point.gw, better_contracted_point.fe); //debug
  }  else {
    better_contracted_point=contracted_points.in;
  }
  printf("   better_contracted_point.fv=%g, better_contracted_point.gw=%g, better_contracted_point.fe=%g\n", better_contracted_point.fv, better_contracted_point.gw, better_contracted_point.fe);  //debug
  if (better_contracted_point.fe < simplex_fe[1][2]) {
    simplex_fe[2][0]=better_contracted_point.fv;
    simplex_fe[2][1]=better_contracted_point.gw;
    simplex_fe[2][2]=better_contracted_point.fe;
    printf("   **simplex contracted\n");  //debug
    display_simplex(   simplex_fe);  //debug
    return;
  }
  printf("   shrink simplex\n");  //debug
  points_fe shrunken_points=shrink_simplex(temp, reduced_density, simplex_fe, data_dir, dx, verbose);
  printf("   shrunken_points.in.fv=%g, shrunken_points.in.gw=%g, shrunken_points.in.fe=%g\n", shrunken_points.in.fv, shrunken_points.in.gw, shrunken_points.in.fe);  //debug
  printf("   shrunken_points.out.fv=%g, shrunken_points.out.gw=%g, shrunken_points.out.fe=%g\n", shrunken_points.out.fv, shrunken_points.out.gw, shrunken_points.out.fe);  //debug
  simplex_fe[1][0]=shrunken_points.out.fv;
  simplex_fe[1][1]=shrunken_points.out.gw;
  simplex_fe[1][2]=shrunken_points.out.fe;

  simplex_fe[2][0]=shrunken_points.in.fv;
  simplex_fe[2][1]=shrunken_points.in.gw;
  simplex_fe[2][2]=shrunken_points.in.fe;
  
  printf("   **simplex shrunken\n");  //debug
  display_simplex(   simplex_fe);  //debug
}


//+++++++++++++++++++++++++++++++END Downhill Simplex+++++++++++++++++++++++++++



int main(int argc, char **argv) {
  double reduced_density=1.0, gw=-1, fv=-1, temp=1.0; //reduced density is the homogeneous (flat) density accounting for sphere vacancies
  double fv_start=0.0, fv_end=.99, fv_step=0.01, gw_start=0.01, gw_end=1.5, gw_step=0.1, gw_lend=0.5, gw_lstep=0.1;
  double gwidth, gwend, gwstep;
  double dx=0.01;        //grid point spacing dx=dy=dz=0.01
  int verbose = false;
  int downhill = false;

//+++++++++++++++++++++++++++++++Downhill Simplex+++++++++++++++++++++++++++++++
  double simplex_fe[3][3] = {{0.8, 0.2, 0},  //best when ordered
                             {0.4, 0.3, 0},   //mid when ordered
                             {0.2, 0.1, 0}    //worst when ordered
  };  

//+++++++++++++++++++++++++++++++END Downhill Simplex+++++++++++++++++++++++++++


  char *data_dir = new char[1024];
  sprintf(data_dir,"none");
  char *default_data_dir = new char[1024];
//  sprintf(default_data_dir, "crystallization/data");
  sprintf(default_data_dir, "crystallization");


  //********************Setup POPT to get inputs from command line*******************

  poptContext optCon;

  // ----------------------------------------------------------------------------
  // Parse input options
  // ----------------------------------------------------------------------------

  poptOption optionsTable[] = {

    {
      "verbose", '\0', POPT_ARG_NONE, &verbose, 0,
      "Print lots of good stuff!", "BOOLEAN"
    },

    /*** FLUID PARAMETERS ***/
    {"kT", '\0', POPT_ARG_DOUBLE, &temp, 0, "temperature", "DOUBLE"},
    {"n", '\0', POPT_ARG_DOUBLE, &reduced_density, 0, "reduced density", "DOUBLE"},
    {"fv", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &fv, 0, "fraction of vacancies", "DOUBLE or -1 for loop"},
    {"gw", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &gw, 0, "width of Gaussian", "DOUBLE or -1 for loop (without lattice ref) or -2 loop (with lattice ref)"},

    /*** LOOPING OPTIONS ***/
    {"fvstart", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &fv_start, 0, "start fv loop at", "DOUBLE"},
    {"fvend", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &fv_end, 0, "end fv loop at", "DOUBLE"},
    {"fvstep", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &fv_step, 0, "fv loop step", "DOUBLE"},

    {"gwstart", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &gw_start, 0, "start gwidth loop at", "DOUBLE"},
    {"gwlend", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &gw_lend, 0, "end gwidth loop at lattice_constant*gw_lend", "DOUBLE"},
    {"gwlstep", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &gw_lstep, 0, "step by lattice_constant*gw_lstep", "DOUBLE"},
    {"gwend", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &gw_end, 0, "end gwidth loop at", "DOUBLE"},
    {"gwstep", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &gw_step, 0, "gwidth loop step", "DOUBLE"},
    
    /*** Downhill Simplex OPTIONS ***/
    {"dh", '\0', POPT_ARG_NONE | POPT_ARGFLAG_SHOW_DEFAULT, &downhill, 0, "Do a Downhill Simplex", "BOOLEAN"},
    
    /*** GRID OPTIONS ***/
    {"dx", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &dx, 0, "grid spacing dx", "DOUBLE"},

    /*** PARAMETERS DETERMINING OUTPUT FILE DIRECTORY AND NAMES ***/
    {
      "d", '\0', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &data_dir, 0,
      "Directory in which to save data", "DIRNAME"
    },

    POPT_AUTOHELP
    POPT_TABLEEND
  };

  optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
  poptSetOtherOptionHelp(optCon, "[OPTION...]\nRequired arguments: temperature (kT), "
                         "reduced density (n)");

  int c = 0;
  // go through arguments, set them based on optionsTable
  while((c = poptGetNextOpt(optCon)) >= 0);
  if (c < -1) {
    fprintf(stderr, "\n%s: %s\n", poptBadOption(optCon, 0), poptStrerror(c));
    return 1;
  }
  poptFreeContext(optCon);

  printf("------------------------------------------------------------------\n");
  printf("Running %s with parameters:\n", argv[0]);
  for (int i = 1; i < argc; i++) {
    if (argv[i][1] == '-') printf("\n");
    printf("%s ", argv[i]);
  }
  printf("\n");
  printf("------------------------------------------------------------------\n\n");

//*****************************End POPT Setup**************************************

  printf("git version: %s\n", version_identifier());
  printf("\nTemperature=%g, Reduced homogeneous density=%g, Fraction of vacancies=%g, Gaussian width=%g\n", temp, reduced_density, fv, gw);

  // Create directory for data files
  if (strcmp(data_dir,"none") == 0) {
    sprintf(data_dir,"%s",default_data_dir);
    printf("\nUsing default data directory: [deft/papers/fuzzy-fmt]/%s\n", data_dir);
  } else {
    printf("\nUsing given data directory: [deft/papers/fuzzy-fmt]/%s\n", data_dir);
  }
  mkdir(data_dir, 0777);

//+++++++++++++++++++++++++++++++Downhill Simplex+++++++++++++++++++++++++++++++

// simplex_fe[3][3] = {{0.05, 0.2, 0},   //best when sorted
//                     {0.2, 0.3, 0},   //mid when sorted
//                     {0.4, 0.1, 0}};  //worst when sorted

if (downhill) {

  display_simplex(simplex_fe);
  printf("Calculating free energies (takes a bit- 14sec x 3 )...\n");
  evaluate_simplex(temp, reduced_density, simplex_fe, data_dir, dx, bool(verbose));
  display_simplex(simplex_fe);
  printf("starting downhill simplex loop...\n");

  for (int i=0;i<50;i++) {
    printf("\nLoop %i of 50 \n", i);  //for debug
    printf("sort simplex\n");
    sort_simplex(simplex_fe);
    printf("simplex sorted\n");
    display_simplex(simplex_fe);  //for debug
    printf("advance simplex\n");
    advance_simplex(temp, reduced_density, simplex_fe, data_dir, dx, bool(verbose));
    printf("simplex advanced\n");
    display_simplex(simplex_fe);   //for debug
//check_convergence_simplex();
  }
  sort_simplex(simplex_fe);    //delete when have loop
  display_simplex(simplex_fe);  //delete when have loop
  exit(1);

//printf("best=%g  ", simplex_fe[0][2]);
//printf("mid=%g  ", simplex_fe[1][2]);
//printf("worst=%g\n", simplex_fe[2][2]);

}

//+++++++++++++++++++++++++++++++END Downhill Simplex+++++++++++++++++++++++++++


//return 0;  //for debug

  if (fv == -1) {
    printf("fv loop variables: fv start=%g, fv_end=%g, fv step=%g\n", fv_start, fv_end, fv_step);
  }

  if (gw == -1) {
    printf("gw loop variables: gwidth start=%g, gwidth end=%g, step=%g\n", gw_start, gw_end, gw_step);
  } else if (gw == -2) {
    printf("gw loop variables: gwidth start=%g, gwidth end=lattice constant*%g, step=lattice constant*%g\n", gw_start, gw_lend, gw_lstep);
  }

  if (fv == -1) {
    double best_energy_diff = 1e100;
    double best_fv, best_gwidth, best_lattice_constant, best_cfree_energy;
    char *best_alldatfile;  //Identify best all.dat file
    double hfree_energy_pervol, cfree_energy_pervol;
    const int num_to_compute = int(0.3/0.05*1/0.01);
    int num_computed = 0;
    for (double fv=fv_start; fv<fv_end+fv_step; fv+=fv_step) {
      double lattice_constant = find_lattice_constant(reduced_density, fv);
      printf("lattice_constant is %g\n", lattice_constant);
      if (gw == -1) {
        gwend=gw_end;
        gwstep=gw_step;
      } else if (gw == -2) {
        gwend=lattice_constant*gw_lend;
        gwstep=lattice_constant*gw_lstep;
      }
      printf ("gw is %g\n", gw);
      printf ("gwend=%g, gwstep=%g   \n\n", gwend, gwstep);

      for (double gwidth=gw_start; gwidth <= gwend +gwstep; gwidth+=gwstep) {
        data e_data =find_energy(temp, reduced_density, fv, gwidth, data_dir, dx, bool(verbose));
        num_computed += 1;
        if (num_computed % (num_to_compute/100) == 0) {
          //printf("We are %.0f%% done, best_energy_diff == %g\n", 100*num_computed/double(num_to_compute),
          //       best_energy_diff);
        }
        if (e_data.diff_free_energy_per_atom < best_energy_diff) {
          //printf("better free energy with fv %g gwidth %g and E %g\n",
          //       fv, gwidth, e_data.diff_free_energy_per_atom);
          best_energy_diff = e_data.diff_free_energy_per_atom;
          best_cfree_energy = e_data.cfree_energy_per_atom;
          best_fv = fv;
          best_gwidth = gwidth;
          best_lattice_constant=lattice_constant;
          hfree_energy_pervol=e_data.hfree_energy_per_vol;
          cfree_energy_pervol=e_data.cfree_energy_per_vol;
          best_alldatfile=e_data.dataoutfile_name;  //file descriptor (ASK! want to copy each best alldat file to save it?)
        }
      }
    }
    printf("Best: fv %g  gwidth %g  Energy Difference %g\n", best_fv, best_gwidth, best_energy_diff);

    //Create bestdataout filename (to be used if we are looping)
    char *bestdat_filename = new char[1024];
    sprintf(bestdat_filename, "%s/%s_best.dat", data_dir, best_alldatfile);

    //Create bestdataout file
    printf("Create best data file: %s\n", bestdat_filename);
    FILE *newmeltbest = fopen(bestdat_filename, "w");
    if (newmeltbest) {
      fprintf(newmeltbest, "# git version: %s\n", version_identifier());
      fprintf(newmeltbest, "#kT\tn\tvacancy_fraction\tGaussian_width\thomogeneous_energy/atom\t\tbest_crystal_free_energy/atom\tbest_energy_difference/atom\tbest_lattice_constant\thomogeneous_energy/volume\tbest_crystal_energy/volume\n");
      fprintf(newmeltbest, "%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t\t%g\t%g\n", temp, reduced_density, best_fv, best_gwidth,
              best_cfree_energy-best_energy_diff, best_cfree_energy, best_energy_diff,
              best_lattice_constant, hfree_energy_pervol, cfree_energy_pervol);
      fclose(newmeltbest);
    } else {
      printf("Unable to open file %s!\n", bestdat_filename);
    }

  } else if (gw < 0) {
    double best_energy_diff = 1e100;
    double best_fv, best_gwidth, best_lattice_constant, best_cfree_energy;
    char *best_alldatfile;  //Identify best all.dat file
    double hfree_energy_pervol, cfree_energy_pervol;
    double lattice_constant = find_lattice_constant(reduced_density, fv);
    printf("lattice_constant is %g\n", lattice_constant);
    if (gw == -1) {
      gwend=gw_end;
      gwstep=gw_step;
    } else if (gw == -2) {
      gwend=lattice_constant*gw_lend;
      gwstep=lattice_constant*gw_lstep;
    }
    printf("gw is %g\n", gw);
    printf ("gwend=%g, gwstep=%g   \n\n", gwend, gwstep);
    for (double gwidth=gw_start; gwidth <= gwend + gwstep; gwidth+=gwstep) {
      data e_data =find_energy(temp, reduced_density, fv, gwidth, data_dir, dx, bool(verbose));
      if (e_data.diff_free_energy_per_atom < best_energy_diff) {
        best_energy_diff = e_data.diff_free_energy_per_atom;
        best_cfree_energy = e_data.cfree_energy_per_atom;
        best_fv = fv;
        best_gwidth = gwidth;
        best_lattice_constant=lattice_constant;
        hfree_energy_pervol=e_data.hfree_energy_per_vol;
        cfree_energy_pervol=e_data.cfree_energy_per_vol;
        best_alldatfile=e_data.dataoutfile_name;  //file descriptor (ASK! want to copy the best alldat file to save it?)
      }
    }
    printf("For fv %g, Best: gwidth %g  energy Difference %g\n", best_fv, best_gwidth, best_energy_diff);

    //Create bestdataout filename (to be used if we are looping)
    char *bestdat_filename = new char[1024];
    sprintf(bestdat_filename, "%s/%s_best.dat", data_dir, best_alldatfile);

    //Create bestdataout file
    printf("Create best data file: %s\n", bestdat_filename);
    FILE *newmeltbest = fopen(bestdat_filename, "w");
    if (newmeltbest) {
      fprintf(newmeltbest, "# git version: %s\n", version_identifier());
      fprintf(newmeltbest, "#kT\tn\tvacancy_fraction\tGaussian_width\thomogeneous_energy/atom\t\tbest_crystal_free_energy/atom\tbest_energy_difference/atom\tbest_lattice_constant\thomogeneous_energy/volume\tbest_crystal_energy/volume\n");
      fprintf(newmeltbest, "%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t\t%g\t%g\n",
              temp, reduced_density, best_fv, best_gwidth, best_cfree_energy-best_energy_diff, best_cfree_energy, best_energy_diff, best_lattice_constant, hfree_energy_pervol, cfree_energy_pervol);
      fclose(newmeltbest);
    } else {
      printf("Unable to open file %s!\n", bestdat_filename);
    }

  } else {
    gwidth=gw;
    find_energy(temp, reduced_density, fv, gwidth, data_dir, dx, true);
  }

  return 0;
}
