#include "ReciprocalGrid.h"

complex ReciprocalGrid::operator()(const RelativeReciprocal &r) const {
  double rx = r(0)*gd.Nx, ry = r(1)*gd.Ny, rz = r(2)*gd.Nz;
  int ix = floor(rx), iy = floor(ry), iz = floor(rz);
  double wx = rx-ix, wy = ry-iy, wz = rz-iz;
  while (ix < 0) ix += gd.Nx;
  while (iy < 0) iy += gd.Ny;
  while (iz < 0) iz += gd.Nz;
  while (ix >= gd.Nx) ix -= gd.Nx;
  while (iy >= gd.Ny) iy -= gd.Ny;
  while (iz >= gd.Nz) iz -= gd.Nz;
  int ixp1 = (ix+1)%gd.Nx, iyp1 = (iy+1)%gd.Ny, izp1 = (iz+1)%gd.Nz;
  while (ixp1 < 0) ixp1 += gd.Nx;
  while (iyp1 < 0) iyp1 += gd.Ny;
  while (izp1 < 0) izp1 += gd.Nz;
  while (ixp1 >= gd.Nx) ixp1 -= gd.Nx;
  while (iyp1 >= gd.Ny) iyp1 -= gd.Ny;
  while (izp1 >= gd.Nz) izp1 -= gd.Nz;
  if (iz > gd.NzOver2) iz = gd.Nz - iz;
  if (izp1 > gd.NzOver2) izp1 = gd.Nz - izp1;
  assert(wx>=0);
  assert(wy>=0);
  assert(wz>=0);
  return (1-wx)*(1-wy)*(1-wz)*(*this)(ix,iy,iz)
    + wx*(1-wy)*(1-wz)*(*this)(ixp1,iy,iz)
    + (1-wx)*wy*(1-wz)*(*this)(ix,iyp1,iz)
    + (1-wx)*(1-wy)*wz*(*this)(ix,iy,izp1)
    + wx*(1-wy)*wz*(*this)(ixp1,iy,izp1)
    + (1-wx)*wy*wz*(*this)(ix,iyp1,izp1)
    + wx*wy*(1-wz)*(*this)(ixp1,iyp1,iz)
    + wx*wy*wz*(*this)(ixp1,iyp1,izp1);
}

static double cartSqr(Reciprocal r) {
  return r.squaredNorm();
}
static double xfunc(Reciprocal r) {
  return r(0);
}
static double yfunc(Reciprocal r) {
  return r(1);
}
static double zfunc(Reciprocal r) {
  return r(2);
}

ReciprocalGrid::ReciprocalGrid(const GridDescription &gdin)
  : VectorXcd(gdin.NxNyNzOver2), gd(gdin),
    g2_op(gd, cartSqr), gx_op(gd, xfunc), gy_op(gd, yfunc), gz_op(gd, zfunc) {
}

ReciprocalGrid::ReciprocalGrid(const ReciprocalGrid &x)
  : VectorXcd(x), gd(x.gd),
    g2_op(gd, cartSqr), gx_op(gd, xfunc), gy_op(gd, yfunc), gz_op(gd, zfunc) {
}
