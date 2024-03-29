/******************************************************************
 * Copyright (c) 2021 Kotaro Tadano
 ******************************************************************/

#ifndef __TS01_DRIVER__
#define __TS01_DRIVER__

#include <string>

class TS01{
  
 public:

  enum CountMode{
		 QUAD_DIFF   = 0xffaaaa, //全ch差動入力
		 QUAD_SINGLE = 0x00aaaa, //全chシングルエンド
  };

  
  TS01();
  virtual ~TS01();
	  
  int open(std::string hostname, int sid=0);
  
  void close();

  ssize_t write(int addr, int data); //汎用書き込み 4バイト
  ssize_t read(int addr, void* data);//汎用読み込み 4バイト

  int write_data(TS01OutputData* data); //一括書き込み

  int read_autosampling_data(TS01InputData* data);
  
  int start_sampling(int period_us);
  void stop_sampling();

  void set_count_mode(CountMode mode);
  void start_count();
  void stop_count();
  void set_count(int value);
  
  void setup_ssi(int ch,short clock,short length,short timeout);
  void set_dout_mode(int ch,bool flag_clock);

  
 private:
  class Impl;
  Impl* impl;
};


#endif
