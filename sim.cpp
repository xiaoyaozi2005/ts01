#include "data.h"
#include "sim.h"

#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <ktl.h>

//#define DEBUG_PRINT printf
#define DEBUG_PRINT



class TS01Simulator::Impl{
public:
  TS01InputData*   input;
  TS01OutputData* output;

  
  Impl(int id,
       TS01InputData*   input,
       TS01OutputData* output  );
  
  int create_command_server();
  void join_command_server();
  int create_autosampling_server();
  void join_autosampling_server();
  
 private:
  std::thread* task_c_server; //command server
  std::thread* task_a_server; //as server

  Ktl::RTTask task_as; //autosampling task
  bool as_connected;//オートサンプリングサーバに接続があるか
  int  as_socket; //for sampling data

  int period; //オートサンプリング周期[100*us]
  int vender;
  int id; //複数使用のための識別子

  struct sockaddr_in as_client;
  socklen_t clientlen;// = sizeof(as_client);

  bool flag_clock[TS01_DO_CH_NUM]; //パルス出力として使用するか
  
  int  command_server();
  int  autosampling_server();
  void autosampling();
  
};

TS01Simulator::Impl::Impl(int num,
			  TS01InputData*   pinput,
			  TS01OutputData* poutput ){
  id = num;
  vender = 1234;

  input = pinput;
  output= poutput;
  
  input->init();
  output->init();
}



int TS01Simulator::Impl::create_command_server(){
  task_c_server = new std::thread( [this]{ command_server(); } );
  return 0;
}
void TS01Simulator::Impl::join_command_server(){
  task_c_server->join();

}
  
int TS01Simulator::Impl::create_autosampling_server(){
  task_as.create( [this]{autosampling();}); //autosampling
  task_a_server = new std::thread( [this]{ autosampling_server(); } );
  
  return 0;
}
void TS01Simulator::Impl::join_autosampling_server(){
  task_a_server->join();
}


inline void align_data2(unsigned char* des, unsigned char* data){
  des[0] = data[1];
  des[1] = data[0];
}
inline void align_data4(unsigned char* des, unsigned char* data){
  des[0] = data[3];
  des[1] = data[2];
  des[2] = data[1];
  des[3] = data[0];
}


int  TS01Simulator::Impl::command_server(){
  vender = 1234;
  int ret = 0;
  
  in_port_t port = 10030 + 10 * id;
  struct sockaddr_in me;
  memset( &me, 0, sizeof(me) );
  me.sin_family = AF_INET;
  me.sin_addr.s_addr = htonl( INADDR_ANY );
  me.sin_port = htons(port);
  
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  ret = bind(sock, (struct sockaddr *)&me, sizeof(me) );
  if( ret<0 ){
    perror("bind");
    return -1;
  }
  
  //----------------------------------------------

  const size_t READING_SIZE = 1024;
  
  unsigned char cbuf[READING_SIZE]; //command packet buffer

  struct sockaddr_in from;
  int fromlen = sizeof(from);
  
  while(1){
    //DEBUG_PRINT("\nCommandServer : waitiing for a command\n");
    ssize_t rsize = recvfrom( sock, cbuf, READING_SIZE, 0,
			      (struct sockaddr*)&from, (socklen_t*)&fromlen);
    
    if( rsize == 0 ){
      DEBUG_PRINT("CommandServer : disconnected by the client\n\n");
      break;
    }
    else if( rsize < 0 ){
      //クライアント側のsockオプションにより発生
      DEBUG_PRINT("CommandServer:rsize = %ld\n",rsize);
      perror("recv");
      continue;
    }
    //DEBUG_PRINT("received a packet rsize = %ld\n",rsize);
    
    int current_head = 0;
    
    while( current_head < rsize ){
      int addr;
      align_data4( (unsigned char*)&addr, &cbuf[current_head+20] );
      size_t data_size   = cbuf[current_head+26];
      size_t packet_size = 12+16+data_size+1;
      //DEBUG_PRINT(" addr = 0x%x, size = %d\n",addr, packet_size);
      
      
      if( cbuf[current_head+14] == 0x74 ){ //write command
	//DEBUG_PRINT("received a write command %d\n",current_head);
	
	switch(addr){
	case 0x2004: //Auto Sampling
	  align_data4( (unsigned char*)&period,  &cbuf[current_head+28] );
	  DEBUG_PRINT("period = %d [us]\n",period*100);
	  
	  if( period > 0 )
	    task_as.start(period*100);
	  else
	    task_as.stop();
	  break;
	case 0x10064: //COUNT
	  align_data4( (unsigned char*)&input->count[0],
		       &cbuf[current_head+28] );
	  break;
	case 0x10068: //COUNT
	  align_data4( (unsigned char*)&input->count[1],
		       &cbuf[current_head+28] );
	  break;
	case 0x1006c: //COUNT
	  align_data4( (unsigned char*)&input->count[2],
		       &cbuf[current_head+28] );
	  break;
	case 0x10070: //COUNT
	  align_data4( (unsigned char*)&input->count[3],
		       &cbuf[current_head+28] );
	  break;
	case 0x10074: //COUNT
	  align_data4( (unsigned char*)&input->count[4],
		       &cbuf[current_head+28] );
	  break;
	case 0x10078: //COUNT
	  align_data4( (unsigned char*)&input->count[5],
		       &cbuf[current_head+28] );
	  break;
	case 0x1007C: //COUNT
	  align_data4( (unsigned char*)&input->count[6],
		       &cbuf[current_head+28] );
	  break;
	case 0x10080: //COUNT
	  align_data4( (unsigned char*)&input->count[7],
		       &cbuf[current_head+28] );
	  break;
	  
	case 0x10084: //AOUT
	  for(int j=0; j<data_size/4; j++){
	    int data;
	    align_data4( (unsigned char*)&data, &cbuf[current_head+28+4*j] );
	    output->u[j] = 20.0/0xffff * data;
	    //DEBUG_PRINT("u[%d]= %f\n",j,output->u[j]);
	  }
	  break;
	case 0x100b4: //DOUT
	  {
	    unsigned char ddata = 0;
	    align_data4( &ddata, &cbuf[current_head+28] );
	    //printf("ddata=%x\n",ddata);
	    for(int j=0;j<TS01_DO_CH_NUM;j++){
	      if( !flag_clock[j] ){
		output->dout[j] = (0x1 << j) & ddata;
		//printf("TS01Sim::dout[%d] = %x\n",j,output->dout[j]);
	      }
	    }
	  } 
	  break;
	case 0x9000: //DOUT mode
	  {
	    unsigned char dmode_status = 0;
	    align_data4( &dmode_status, &cbuf[current_head+28] );
	    DEBUG_PRINT("TS01Sim::dmode = 0x%x\n",dmode_status);
	    
	    for(int j=0;j<TS01_DO_CH_NUM;j++){
	      flag_clock[j] = ((0x1 << j) & dmode_status);
	      DEBUG_PRINT("TS01Sim::dmode[%d] = %x\n",j,flag_clock[j]);
		     
	    }
	  } 
	  
	  break;
	case 0x9010: //freq
	  {
	    int data = 0; //分周値
	    
	    for(int j=0;j<TS01_DO_CH_NUM;j++){
	      if( flag_clock[j] ){
		align_data4( (unsigned char*)&data, &cbuf[current_head+28+4*j] );

		output->f[j] = (data == 0) ? 0.0 : 10e6 / data;

		//DEBUG_PRINT("TS01Sim::freq[%d] = %d, %.3f[Hz]\n",j,data,output->f[j]);
		
	      }
	    }
	  } 
	  
	  break;
	} //switch
      } //if
      else if( cbuf[current_head+14] == 0x4C ){ //read  command
	//size_t packet_size = 12+16;
	
	//DEBUG_PRINT("received a read command (%ld)\n",rsize);
	//DEBUG_PRINT(" addr = %x\n",addr);
	
	static unsigned char replySpW[12+12+4+1] = {
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	  0xFE,0x01,0x0C,0x00,//12,13,14,15
	  0xFE,0x00,0x00,0x00,//16,17,18,19
	  0x00,0x00,0x04,0x00,//20,21,22,23 //size
	};
	
	switch(addr){
	case 0x0000: //Vender ID
	  align_data4( &replySpW[24],
		       (unsigned char*)&vender );
	  break;
	  
	case 0x2004: //period
	  align_data4( &replySpW[24],
		       (unsigned char*)&period );
	  DEBUG_PRINT("return period %d[us]\n",period*100);
	  break;
	}
	
	sendto( sock, replySpW, sizeof(replySpW), 0,
		(struct sockaddr*)&from, fromlen );	
	
      } //else if
      else{
	DEBUG_PRINT("invalid packet (rsize = %ld)\n",rsize);
	DEBUG_PRINT("%x %x\n",cbuf[current_head+12],cbuf[current_head+14]);
	break;
      }
      current_head += packet_size;
    } //while
    
  }//while
  
  close(sock);
  return 0;
}


void TS01Simulator::Impl::autosampling(){
  task_as.init();

  unsigned int count = 0;
  unsigned char rbuf[133] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x68,//size+16+1,
    0xFE,0x01,0x74,0x02, //12,13,14,15
    0xFE,0x00,0x00,0x00, //16,17,18,19 
    0x00,0x00,0x00,0x00, //20,21,22,23 addr
    0x00,0x00,0x64,0xAA, //24,25,26,27
  };
  rbuf[132] = 0xAA;
  
  while(1){
    //DEBUG_PRINT("autosampling:waiting for a clock at %d\n",count);
    task_as.wait();
    //DEBUG_PRINT("autosampling:go at %d\n",count);

    align_data4( &rbuf[28],
		 (unsigned char*)&count );
    
    short d;  
    for(int i=0;i<TS01_AI_CH_NUM;i++){
      //input->v[i] =  VPB * d;
      d = 0xffff/10.0 * input->v[i];
      //DEBUG_PRINT("%d\n",d);
      align_data2( &rbuf[32+2*i],
		   (unsigned char*)&d  ); 
    }
    
    for(int j=0;j<TS01_DI_CH_NUM;j++){
      //rbuf[95] =	    data->din[j];
    }
    for(int i=0;i<TS01_SSI_CH_NUM;i++){
      align_data4( &rbuf[68+4*i],
		   (unsigned char*)&input->ssi[i] );
    }
    
    for(int i=0;i<TS01_CNT_CH_NUM;i++){
      align_data4( &rbuf[100+4*i],
		   (unsigned char*)&input->count[i] );
      //DEBUG_PRINT("%d : %8x\n",i, (data->ssi[i]) );//& 0x1ffff );
    }
    
    if( as_connected ){
      ssize_t ret = sendto( as_socket, rbuf, sizeof(rbuf), 0,
			(struct sockaddr*)&as_client, sizeof(as_client));
      if( ret < 0 ){
	perror("sendto");
      }
    }
    
    
    count++;
  }//while
  
  DEBUG_PRINT("autosampling stopped\n");
}


int TS01Simulator::Impl::autosampling_server(){
  int ret = 0;
  
  in_port_t port = 10031 + 10*id;

  struct sockaddr_in me;
  memset( &me, 0, sizeof(me) );
  me.sin_family = AF_INET;
  me.sin_addr.s_addr = htonl( INADDR_ANY );
  me.sin_port = htons(port);
  
  as_socket = socket(AF_INET, SOCK_DGRAM, 0);
  ret = bind(as_socket, (struct sockaddr *)&me, sizeof(me) );
  if( ret<0 ){
    perror("bind");
    return -1;
  }
  //DEBUG_PRINT("tcp : init sock\n");

  clientlen = sizeof(as_client);

  unsigned char rbuf[128];
  
  while(1){
    ssize_t rsize = recvfrom( as_socket, rbuf, 128, 0,
			      (struct sockaddr*)&as_client,
			      (socklen_t*)&clientlen);
    //クライアントのポートを知るために任意のパケットを受け取る
    
    if( rsize == 0 ){
      fprintf(stderr,"AutoSamplingServer:disconnected by the client\n\n");
      as_connected = false;
      break;
    }
    else if( rsize < 0 ){
      fprintf(stderr,"AutoSamplingServer:rsize = %ld\n",rsize);
      perror("recv");
      continue;
    }

    //DEBUG_PRINT("AutoSamplingServer:set client %d %d\n",
    //as_client.sin_addr.s_addr,
    //as_client.sin_port);
    as_connected = true;
    
  }//while
    
  close(as_socket);
  return 0;
}



TS01Simulator::TS01Simulator(int num):
  impl(new TS01Simulator::Impl(num, &input, &output) ){
}

TS01Simulator::~TS01Simulator(){
  delete impl;
}

int TS01Simulator::create_command_server(){
  return impl->create_command_server();
}

void TS01Simulator::join_command_server(){
  impl->join_command_server();
}
  
int TS01Simulator::create_autosampling_server(){
  return impl->create_autosampling_server();
}

void TS01Simulator::join_autosampling_server(){
  impl->join_autosampling_server();
}
