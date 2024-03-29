#include <stdlib.h>
//#include <netdb.h>
//#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <ktl.h>
#include "libts01.h"

//#define USE_FIFO
#ifdef USE_FIFO
# include <rtai_fifos.h>
#endif

#define DEBUG_PRINT printf

bool flag_terminate=false;

pthread_t task_tx;
int fd_tx;

pthread_t task_rx;
int fd_rx;

//Ktl::Timer timer;

TS01 ts01;

void sig_handler(int sig){
  DEBUG_PRINT("ts:sig_handler catch %d \n",sig);
  flag_terminate = true;
  ts01.stop_sampling();
  fcntl(fd_tx, F_SETFL, O_NONBLOCK);
  fcntl(fd_rx, F_SETFL, O_NONBLOCK);
  DEBUG_PRINT("ts:handler done\n");
}



void* test_thread(void* arg){
  ts01.write( 0x4000, 0x0010 );
  usleep( 1000 );
  ts01.write( 0x4020, 0x0012 );

  DEBUG_PRINT("setup done\n");

  unsigned char data[4];
  for(int i=0;i<255;i++){
    //write_data( 0x7008, i);
    //timer.reset();
    ts01.read( 0x10044, data );//, 4 );
    //timer.get_time();
    //double tint = timer.get_interval();
    //DEBUG_PRINT("t = %.3f [ms]\n",tint*1e3);

    int x = 
      (data[0] << 23) +
      (data[1] << 15) +
      (data[2] <<  7) +
      (data[3] >>  1) ;

    DEBUG_PRINT("%d\n",x);
    double t = x/131072.0;
    ts01.write( 0x7008, (int)(255.0*t) );
    //double v = 20*t -10;
    ts01.write( 0x10084, 0xffff*t );
    usleep(20000);
  } 


  ts01.write( 0x7008, 0x00);


  return 0;
}

/***************************************
 * アプリケーションからTS-01へ指令値を転送
 ****************************************/
void* transmitter(void* arg){

  TS01OutputData output;

  while(1){
    ssize_t rsize = read(fd_tx,&output,sizeof(output) );
    //if( rsize < 0 ) break;

    ts01.write_data( &output );

    if( flag_terminate ) break;  
  } 

  DEBUG_PRINT("ts:end of transmitter\n");
  return 0;
}




static short align_data2(unsigned char* data){
  return (data[0] << 8) +  (data[1]);
}
/***************************************
 * TS-01からデータを取得
 ****************************************/
void* receiver(void* arg){

  //Ktl::Timer timer;
  //timer.reset();


  DEBUG_PRINT("ts:start_sampling\n");
  ts01.start_sampling(10000);
  DEBUG_PRINT("ts:start_sampling done\n");

  usleep(1000);

  TS01InputData input;
  unsigned char buf[4];


  while(1){
    ssize_t rsize = ts01.read_autosampling_data(&input);
    /*
    for(int j=0;j<2;j++){
      ssize_t rsize = ts01.read( 0x10000+j*4, buf );
      short d = align_data2( &buf[2] ); //16bit
      input.v[j] =  10.0/0xffff * d;

    //timer.get_time();
    //double t = timer.get_interval();
    //DEBUG_PRINT("t = %.2f [ms]\n",t*1e3);

    if( rsize == 0 ){
      DEBUG_PRINT("ts:cannot receive data. breaking\n");
      break;
    }
    }
    */
    ssize_t wsize = write( fd_rx, &input, sizeof(input) );
    if( wsize <= 0 ){
      DEBUG_PRINT("write error\n");
      perror("write");
    }

    if( flag_terminate ) break;
  } 


  ts01.stop_sampling();
  DEBUG_PRINT("ts:end of receiver\n");
  return 0;
}


int main(int ac,char* av[]){
  signal(SIGTERM,sig_handler);
  signal(SIGINT,sig_handler);
  if( ac<4 ){
    fprintf(stderr,"arg : [hostname] [tfifo] [rfifo]\n");
    return -1;
  }


  const char* server_hostname = av[1];
  if( ts01.open( server_hostname ) < 0 ){
    fprintf(stderr,"ts:cannot open TS-01\n");
    return -1;
  }


  short ssi_clock[TS01_SSI_CH_NUM];
  short ssi_length[TS01_SSI_CH_NUM];

  for(int j=0;j<TS01_SSI_CH_NUM;j++){
    ssi_clock[j]  = 16; // 16 * 100 ns
    ssi_length[j] = 18;  //18bit = 17+1
    ts01.write( 0x4000 + 4*j, ssi_clock[j] );
    ts01.write( 0x4020 + 4*j, ssi_length[j] );
  }



  int fifo;
  char rtf[128];
  //-- tx --
  sscanf(av[2],"%d",&fifo);
  sprintf( rtf, "/dev/rtf%d", fifo );

  DEBUG_PRINT("ts:tx rtf = %s\n",rtf);
  fd_tx  = open(rtf,O_RDONLY);//, O_NONBLOCK);

  sscanf(av[3],"%d",&fifo);
  sprintf(rtf, "/dev/rtf%d", fifo );
  DEBUG_PRINT("ts:rx rtf = %s\n",rtf);
  fd_rx  = open(rtf,O_WRONLY);//, O_NONBLOCK);

  DEBUG_PRINT("ts:fifo opened\n");
  

  pthread_create( &task_rx, NULL, receiver, NULL);
  pthread_create( &task_tx, NULL, transmitter, NULL);

  DEBUG_PRINT("ts:waiting for join\n");

  pthread_join(task_rx, NULL);
  DEBUG_PRINT("ts:joined rx\n");

  pthread_join(task_tx, NULL);
  //DEBUG_PRINT("ts:joined tx\n");

  close(fd_tx);
  close(fd_rx);
  
  ts01.close();


  DEBUG_PRINT("ts:end of main\n");
  return 0;
}



