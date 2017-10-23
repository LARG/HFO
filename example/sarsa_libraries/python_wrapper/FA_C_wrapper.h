#ifndef __FA_C_WRAPPER_H__
#define __FA_C_WRAPPER_H__

#include "CMAC.h"
#include<iostream>

extern "C" {
  void* CMAC_new(int numF, int numA, double r[], double m[], double res[]) 
  {
//    std::cout<<"FA_C_WRAPPER: CMAC_new"<<std::endl;
    CMAC *ca = new CMAC(numF, numA, r, m, res);
    void *ptr = reinterpret_cast<void *>(ca);
    return ptr;
  }
}

#endif
