#include <stdio.h>
#include "Monte-Carlo/monte-carlo.h"

void run(){
  const int N = 13;
  const double R = 1;
  const double rad = 3;
  Vector3d *spheres = new Vector3d[N];
  spheres[0] = Vector3d(0,0,0);
  spheres[1] = Vector3d(2*R,0,0);
  spheres[2] = Vector3d(R,R*sqrt(3),0);
  spheres[3] = Vector3d(-R,R*sqrt(3),0);
  spheres[4] = Vector3d(-2*R,0,0);
  spheres[5] = Vector3d(-R,-R*sqrt(3),0);
  spheres[6] = Vector3d(R,-R*sqrt(3),0);
  spheres[7] = Vector3d(R,-R*.5,R*sqrt(3));
  spheres[8] = Vector3d(0,sqrt(3)-.5*R,R*sqrt(3));
  spheres[9] = Vector3d(-R,-.5*R,R*sqrt(3));
  spheres[10] = Vector3d(R,-.5*R,-R*sqrt(3));
  spheres[11] = Vector3d(0,sqrt(3)-.5*R,-R*sqrt(3));
  spheres[12] = Vector3d(-R,-.5*R,-R*sqrt(3)); 
  for(int i = 0; i<N; i++){
    printf("%g  %g  %g\n", spheres[i][0], spheres[i][1], spheres[i][2]);
    printf("Distance from origin = %g\n", distance(spheres[i], Vector3d(0,0,0)));
  }
  FILE *o = fopen("Spheres.dat", "w");
  writeSpheres(spheres, N, o);
  int i = 0;
  int j = 0;
  int count = 0;

  // int layer [5];

  while(j<10000){
    count++;
    //printf("Count = %d\n",count);
    // printf("j = %d\n",j);
     i++;
     //Vector3d temp = move(spheres[i%N], 3, 3, 3);
     // Vector3d temp = move(spheres[i%N],3);
     Vector3d temp = move(spheres[i%N]);
     // printf("Past move\n");
     //if(overlap(spheres, temp, N, R, i%N)){
     //  continue;
     // 
     //}
     if(overlap(spheres, temp, N, R, rad, i%N)){
       continue;
     }
     //printf("Past overlap\n");
     spheres[i%N] = temp;
     
     //layer[shell(spheres[i%N],5,R)]++;

     writeSpheres(spheres, N, o);
     j++;
  }
  printf("Total number of attempted moves = %d\n",count);
  fclose(o);
  delete[] spheres;
}

//To be deleted... cvh
bool overlap(Vector3d *spheres, Vector3d v, int n, double R, int s){
  for(int i = 0; i < n; i++){
    if(i==s){
      continue;
    }
    if(distance(spheres[i],v)<2*R){
      return true;
    }
  }
  return false;
}

bool overlap(Vector3d *spheres, Vector3d v, int n, double R, double rad, int s){
  if(distance(v,Vector3d(0,0,0))>rad){
      return true;
  }
  for(int i = 0; i < n; i++){
    if(i==s){
      continue;
    }
    if(distance(spheres[i],v)<2*R){
      return true;
    }
  }
  return false;
}

bool overlap(Vector3d *spheres, Vector3d v, int n, double R, int s, double x, double y, double z){
  if((fabs((v)[0]) > x) || (fabs((v)[1]) > y) || (fabs((v)[2]) > z)){
      return true;
  }
  for(int i = 0; i < n; i++){
    if(i==s){
     continue;
    }
    if(distance(spheres[i],v)<2*R){
      return true;
    }
  }
  return false;
}


Vector3d move(Vector3d v){
  double scale = .5;
  return v+scale*ran3();
}
 
//To be deleted... cvh 
Vector3d move(Vector3d v, double x, double y, double z){
  const double scale =.5;
  Vector3d temp;
  while(true){
    temp = ran3()*scale;
    if((fabs((v+temp)[0]) <= x) && (fabs((v+temp)[1]) <= y) && (fabs((v+temp)[2]) <= z)){
	break;
   }
  }
 return temp + v;
}

//To be deleted... cvh
Vector3d move(Vector3d v, double R){
  const double scale = .05;
  Vector3d temp;
  while(true){
    temp = ran3()*scale;
    if(distance(v+temp,Vector3d(0,0,0))<=R){
      break;
    }
  }
  return temp + v;
}

bool touch(Vector3d *spheres, Vector3d v, int n, double R, double delta, int s){
  for(int i = 0; i < n; i++){
    if(i==s){
      continue;
    }
    if(distance(spheres[i],v)>=R && distance(spheres[n],v)<=R+delta){
      return true;
    }
  }
  return false;
}

int shell(Vector3d v, int div, double R){
  double temp = distance(v,Vector3d(0,0,0));
  int count = 1;
  for(int i = R/div; i <= div; i+=R/div){
    if(count==1){
      if(temp <= i){
	return count;
      }
      count++;
      continue;
    }
    if(temp > i && temp <= (i+R/div)){
      return count;
    }
    count++;
  }
  return 0;
}

