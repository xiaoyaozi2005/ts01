/******************************************************************
 * Copyright (c) 2021 Kotaro Tadano
 ******************************************************************/

#ifndef __TS01_DATA__
#define __TS01_DATA__

#define TS01_AI_CH_NUM 16
#define TS01_AO_CH_NUM 12
#define TS01_CNT_CH_NUM 8
#define TS01_SSI_CH_NUM 8
#define TS01_DI_CH_NUM 8
#define TS01_DO_CH_NUM 8


class TS01InputData{
 public:

  double v[TS01_AI_CH_NUM];
  int count[TS01_CNT_CH_NUM];
  int ssi[TS01_SSI_CH_NUM];
  bool din[TS01_DI_CH_NUM];
  unsigned int sequence;

  void init();

  TS01InputData(){
    init();
  }
  void print()const;
};

class TS01OutputData{
 public:
  
  double u[TS01_AO_CH_NUM];
  bool dout[TS01_DO_CH_NUM];
  double f[TS01_DO_CH_NUM];
  
  void init();
  void print()const;
};


#endif
