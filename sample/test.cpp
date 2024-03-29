#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
//#include <ktl.h>
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

  ts01.write( 0x4000, 0x0010 );
  usleep( 1000 );
  ts01.write( 0x4020, 0x0012 );

 
  DEBUG_PRINT("setup done\n");

  unsigned char data[4];
  int x[8];
  TS01OutputData output;
  /*
  for(int i=0;i<100;i++){
    //write_data( 0x7008, i);
    //timer.reset();
    //ts01.read( 0x10044, data );//, 4 );

    for(int j=0; j<8; j++){
      //ts01.read( 0x10084 + 4*j, data );//, 4 );
      ts01.read( 0x10044+4*j, data );//, 4 );
      //ts01.read( 0x10000+4*j, data );//, 4 );
      //timer.get_time();
      //double tint = timer.get_interval();
      //DEBUG_PRINT("t = %.3f [ms]\n",tint*1e3);
      
      x[j] = 
	(data[0] << 24) +
	(data[1] << 16) +
	(data[2] <<  8) +
	(data[3] >>  0) ;
      
      //printf("%2x %2x %2x %2x\n",data[0],data[1],data[2],data[3]);
    }
    printf("%4x %4x %4x %4x ",x[0],x[1],x[2],x[3]);
    printf("%4x %4x %4x %4x\n",x[4],x[5],x[6],x[7]);


    for(int j=0;j<8;j++){
      //usleep(100);
      double u = i % 20 > 10 ? 1.0 * (j+1) : 0.0;
      short d = 0xffff * u / 20.0;
      //ts01.write( 0x10084 + j*4, d );
      output.u[j] = u;
    }

    //ts01.write_data(&output);
    
    usleep(10000);
  } 
  */

  ts01.write( 0x6000, 0x0 );  
  //ts01.write( 0x10084, 0x0000);
  ts01.write( 0x10094, 0x8f00);
  //usleep(1000);
  //ts01.write( 0x6004, 0x1);


  //ts01.write( 0x10094, 0xffff);
  //ts01.write( 0x7008, 0xf4);

  
  ts01.close();


  DEBUG_PRINT("ts:end of main\n");
  return 0;
}



