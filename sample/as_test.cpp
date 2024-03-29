#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <sched.h>
//<sys/time.h><sys/resource.h>
       
#include <ktl.h>
#include "libts01.h"

#define DEBUG_PRINT printf


int main(int ac,char* av[]){
  if( ac<2 ){
    fprintf(stderr,"arg : [hostname]\n");
    return -1;
  }

  TS01 ts01;

  const char* server_hostname = av[1];
  if( ts01.open( server_hostname ) < 0 ){
    fprintf(stderr,"ts:cannot open TS-01\n");
    return -1;
  }


  TS01InputData input;
  TS01OutputData output;
  double t0;
  Ktl::Timer timer;
  timer.reset();

  nice(-20);
   //setpriority(PRIO_PROCESS, getpid(), -20); 

  struct sched_param sp;
  sp.sched_priority=99;  
  int ret = sched_setscheduler(0,SCHED_FIFO,&sp);
  
  
  int period = 100000;
  ts01.start_sampling(period);
  
  for(int count=0;count<10;count++){
    
    //time = count * dt ; // 時刻の更新

    double ti = timer.get_time();
    //t0 = timer.get_time();
    timer.reset();
    double t0 = timer.get_time();
    ssize_t rsize = ts01.read_autosampling_data(&input);
    double t1 = timer.get_time();

    /*
    for(int j=0;j<TS01_SSI_CH_NUM;j++){
      //ssi[j] = input.ssi[j] >> 1; //for MTL enc

      //ssix[j] = (double)ssi[j] / 0x1ffff; //[0-1]
    }
    */
    //output.u[10] = 2*ssix[0];
    output.u[11] = count % 2 ? 5.0 : 0.0;

    double t2 = timer.get_time();
    ts01.write_data( &output );

    double t3 = timer.get_time();

    if( ti > 0.0015 || count%500 == 0 )
      printf("%6d %.3f,  %.3f %.3f %.3f\n",
	     count, ti*1e3,t1*1e3,t2*1e3,t3*1e3);
  }
  

  ts01.stop_sampling();
  usleep(100000);//この間が必要
  ts01.close();


  DEBUG_PRINT("ts:end of main\n");
  return 0;
}



