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
#include <time.h>
#include "OptimizedFunctionals.h"
#include "equation-of-state.h"
#include "LineMinimizer.h"
#include "ContactDensity.h"
#include "utilities.h"
#include "handymath.h"

Functional SoftFluid(double radius, double V0, double mu);

//const double nm = 18.8972613;
// Here we set up the lattice.
double zmax = 16;
double ymax = zmax;
double xmax = zmax;
double dx = 0.1;
double V0 = 1;
const double radius = 1;
double temperature = 0.01; //default temp 
// double kT = 0.01; //default temp 

double soft_sphere_potential(Cartesian r) {
  const double z = r.z();
  const double y = r.y();
  const double x = r.x();
  const double V0overKTcutoff = 100;
  const double Rcutoff = 2*radius*(1-sqrt(V0overKTcutoff/(V0/temperature)));
  const double distance = sqrt(x*x + y*y + z*z);
  if (distance >= radius*2) { return 0; }
  else if (distance > Rcutoff) {
      return V0*pow((1-distance/(2*radius)),2);
  }
  return V0overKTcutoff*temperature;
}

static void took(const char *name) {
  static clock_t last_time = clock();
  clock_t t = clock();
  assert(name); // so it'll count as being used...
  double peak = peak_memory()/1024.0/1024;
  printf("\t\t%s took %g seconds and %g M memory\n", name, (t-last_time)/double(CLOCKS_PER_SEC), peak);
  fflush(stdout);
  last_time = t;
}

const int numiters = 25;

void z_plot(const char *fname, const Grid &a) {
  FILE *out = fopen(fname, "w");
  if (!out) {
    fprintf(stderr, "Unable to create file %s!\n", fname);
    // don't just abort?
    return;
  }
  const GridDescription gd = a.description();
  const int x = 0;
  const int y = 0;
  for (int z=0; z<gd.Nz/2; z++) {
    Cartesian here = gd.fineLat.toCartesian(Relative(x,y,z));
    double ahere = a(x,y,z);
    fprintf(out, "%g\t%g\n", here[2], ahere);
  }
  fclose(out);
}

double run_soft_sphere(double eta, double temp) {
  Functional f = SoftFluid(radius, 1, 0);
  const double mu = find_chemical_potential(OfEffectivePotential(f), temp, eta/(4*M_PI/3));
  printf("mu is %g for eta = %g at temperature %g\n", mu, eta, temp);

  //printf("Filling fraction is %g with functional %s at temperature %g\n", eta, teff);
  //fflush(stdout);
  temperature = temp;
  //if (kT == 0) kT = ;1

  Lattice lat(Cartesian(xmax,0,0), Cartesian(0,ymax,0), Cartesian(0,0,zmax));
  GridDescription gd(lat, dx);

  Grid softspherepotential(gd);
  softspherepotential.Set(soft_sphere_potential);

  f = OfEffectivePotential(SoftFluid(radius, 1, mu) + ExternalPotential(softspherepotential));

  static Grid *potential = 0;
  potential = new Grid(gd);
  *potential = softspherepotential - temperature*log(eta/(4*M_PI/3*radius*radius*radius))*VectorXd::Ones(gd.NxNyNz); // Bad starting guess 
  const double approx_energy = f(temperature, eta/(4*M_PI/3))*xmax*ymax*zmax;
  const double precision = fabs(approx_energy*1e-9);
  printf("\tMinimizing to %g absolute precision from %g from %g...\n", precision, approx_energy, temperature);
  fflush(stdout);

  Minimizer min = Precision(precision,
                            PreconditionedConjugateGradient(f, gd, temperature,
                                                            potential,
                                                            QuadraticLineMinimizer));
  took("Setting up the variables");
  for (int i=0;min.improve_energy(true) && i<100;i++) {
  }

  took("Doing the minimization");
  min.print_info();

  Grid density(gd, EffectivePotentialToDensity()(temperature, gd, *potential));
  //printf("# per area is %g at filling fraction %g\n", density.sum()*gd.dvolume/dw/dw, eta);

  char *plotname = (char *)malloc(1024);

  sprintf(plotname, "papers/fuzzy-fmt/figs/soft-sphere%06.4f-%04.2f.dat", temp, eta);
  z_plot(plotname, Grid(gd, 4*M_PI*density/3));
  free(plotname);

  {
    //double peak = peak_memory()/1024.0/1024;
    //double current = current_memory()/1024.0/1024;
    //printf("Peak memory use is %g M (current is %g M)\n", peak, current);

  }

  took("Plotting stuff");
  printf("density %g gives ff %g for eta = %g and T = %g\n", density(0,0,gd.Nz/2),
         density(0,0,gd.Nz/2)*4*M_PI/3, eta, temp);
  return density(0, 0, gd.Nz/2)*4*M_PI/3; // return bulk filling fraction
}

int main(int argc, char *argv[]) {
  printf("dx = %g, xmax = %g\n", dx, xmax);
  if (argc != 3) {
    fprintf(stderr, "Usage: %s FILLINGFRACTION kT\n", argv[0]);
    exit(1);
  }
  double temp = 0;
  double eta = 0;
  sscanf(argv[2], " %lg", &temp);
  sscanf(argv[1], " %lg", &eta);

  run_soft_sphere(eta, temp);
  return 0;
}
