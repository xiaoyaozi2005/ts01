#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ktl.h>
#include <ktl/rttask.h>

#include<libts01.h>


int main(){
  TS01Simulator ts01;
  ts01.create_command_server();
  ts01.create_autosampling_server();

  RTTask task;

  task.init("main");
  int period = 10000;
  task.start(period);

  int count=0;
  double dt = period*1e-6;
  
  while(1){
    double t = dt*count;
    for(int i=0;i<TS01_AI_CH_NUM;i++){
      ts01.input.v[i] = sin(2*PI*0.5*t) + 0.5*i;
    }

    for(int i=0;i<TS01_DI_CH_NUM;i++){
      ts01.input.din[i] = count%1000 > 500 ? true:false;
    }
    for(int i=0;i<TS01_CNT_CH_NUM;i++){
      ts01.input.count[i] = count%(100*(i+1));
    }
    //printf("v=%f\n",ts01.input.v[1]);
    
    task.wait();
    count++;
  }

  
  ts01.join_command_server();  
  return 0;
}
