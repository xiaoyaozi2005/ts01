/******************************************************************
 * Copyright (c) 2021 Kotaro Tadano
 ******************************************************************/

#ifndef __TS01_SIM__
#define __TS01_SIM__

class TS01Simulator{
public:

  TS01InputData input;
  TS01OutputData output;
  
  TS01Simulator(int id = 0);
  virtual ~TS01Simulator();

  int create_command_server();
  void join_command_server();
  int create_autosampling_server();
  void join_autosampling_server();
  
 private:
  class Impl;
  Impl* impl;
};


#endif
