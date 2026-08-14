// Copyright Epic Games, Inc. All Rights Reserved.

//VSOP87-Multilang http://www.celestialprogramming.com/vsop87-multilang/index.html
//Greg Miller (gmiller@gregmiller.net) 2019.  Released as Public Domain

#ifndef VSOP87A_FULL
#define VSOP87A_FULL

class vsop87a_full{
   public:
   static void getEarth(double t,double temp[]);
   static void getEmb(double t,double temp[]);
   static void getMoon(double earth[], double emb[],double temp[]);

   private:
   static double earth_x(double t);
   static double earth_y(double t);
   static double earth_z(double t);
   static double emb_x(double t);
   static double emb_y(double t);
   static double emb_z(double t);
};
#endif
