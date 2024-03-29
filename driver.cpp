#include "data.h"
#include "driver.h"
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>

//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT printf
#else
#define DEBUG_PRINT
#endif


class TS01::Impl{
  
 public:
  Impl();
	  
  int open(std::string hostname, int sid=0);
  
  void close();

  ssize_t write(int addr, int data); //汎用書き込み 4バイト
  ssize_t read(int addr, void* data);//汎用読み込み 4バイト

  int write_data(TS01OutputData* data); //一括書き込み

  int read_autosampling_data(TS01InputData* data);
  
  int start_sampling(int period_us);//, in_port_t port = 11100);
  void stop_sampling();

  void set_count_mode(CountMode mode);
  void start_count();
  void stop_count();
  void set_count(int value);
  
  void setup_ssi(int ch,short clock,short length,short timeout);
  void set_dout_mode(int ch,bool flag_clock);

  //void apply_config( const TS01Config& config );

  
 private:

  class SendPacket{
  public:
    size_t size;
    int point;
    unsigned char* address;
  };

  int sid; //シミュレータ複数使用時の識別用

  int sock;  //レジスタ読み書き用ソケット
  int socka; //オートサンプリングデータ受信用ソケット
  
  int period;
  unsigned int count;// LEDサイクル用
  
  int dmode_status; //状態を一々読み出さなくてよいようにアプリ側で保持する

  unsigned char pooling_buf[512]; //一つのUDPパケットにまとめるためのバッファ
  int indicator;//

  SendPacket p_aout;
  SendPacket p_load;
  SendPacket p_dout;
  SendPacket p_freq;
  SendPacket p_led;

  struct sockaddr_in server; //コマンドパケット送信用
  std::string hostname; //TS-01のホスト名

  bool flag_sim = false;
  
#define PACKET_SIZE 133
  unsigned char rbuf[PACKET_SIZE];//オートサンプリングデータ受信用バッファ
  
  SendPacket register_packet(int addr, size_t size);
  void update_CRC(const SendPacket& packet);
  
};


// RMAP_CalculateCRC //  CRCの計算
extern unsigned char RMAP_CalculateCRC(unsigned char *buf,
				       unsigned int len, int crcType);

//-----------------------------------------------------------------------
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


ssize_t TS01::Impl::write(int addr, int val){ //4 byte
  static const size_t size = 4;

  unsigned char sendSpW[33] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,size+16+1,
    0xFE,0x01,0x74,0x02, //12,13,14,15
    0xFE,0x00,0x00,0x00, //16,17,18,19
    0x00,0x00,0x00,0x00, //20,21,22,23 addr
    0x00,0x00,size,0x00  //24,25,26,27
  };

  align_data4( &sendSpW[20], (unsigned char*)&addr );
  align_data4( &sendSpW[28], (unsigned char*)&val );
  
  sendSpW[27] = RMAP_CalculateCRC(&sendSpW[12],15,0x01);
  sendSpW[32] = RMAP_CalculateCRC(&sendSpW[28],size,0x01);

  ssize_t wsize = sendto( sock, sendSpW, 33, 0, //12+16+4+1
			  (struct sockaddr*)&server, sizeof(server) );

  if( wsize != 33 ){
    fprintf(stderr,"TS01::write::rsize = %ld, writing_size=33\n", wsize);
    if( wsize <= 0 ){
      perror("TS01::write: send");
    }
  }
  
  return wsize;
}

ssize_t TS01::Impl::read(int addr, void* data){ //4byte

  unsigned char readSpW[28] = { //リードコマンド
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
    0xFE,0x01,0x4C,0x02, //12,13,14,15
    0xFE,0x00,0x00,0x00, //16,17,18,19
    0x00,0x00,0x20,0x00, //20,21,22,23
    0x00,0x00,0x04,0x00  //24,25,26,27
  };
  
  align_data4( &readSpW[20], (unsigned char*)&addr );

  //printf("%2x %2x %2x %2x\n",readSpW[20],readSpW[21],readSpW[22],readSpW[23]);

  readSpW[27] = RMAP_CalculateCRC(&readSpW[12],15,0x01);
  //DEBUG_PRINT("TS01::read : sending a read command\n");

  ssize_t wsize = sendto( sock, readSpW, 28, 0, //12+16
			(struct sockaddr*)&server, sizeof(server) );

  if( wsize < 0 ){
    perror("TS01::read::send");
    return -1;
  }


  static const size_t size = 4;
  size_t reading_size = 12+12+size+1;
  unsigned char replybuf[reading_size];

  struct sockaddr_in from;
  int fromlen = sizeof(from);

  //DEBUG_PRINT("TS01::read : reading\n");
  
  ssize_t rsize = recvfrom( sock, replybuf, reading_size, 0,
			    (struct sockaddr*)&from, (socklen_t*)&fromlen);

  if( rsize < 0 ){
    perror("TS01::read::recv");
    return -1;
  }
  else if( rsize != reading_size ){
    fprintf(stderr,"TS01::read::rsize = %ld, reading_size=%ld\n",
	    rsize,reading_size);
  }
  
  align_data4( (unsigned char*)data, &replybuf[24] );
  return rsize;
}


int TS01::Impl::open(std::string hostname2, int num){
  sid = num; //複数使用する際にのみ必要　通常は0
  in_port_t port = 11000 + sid; //ホスト側ポート

  //---------------------------------------------------------

  hostname = hostname2;

  if( hostname == "127.0.0.1" or
      hostname == "localhost" ){
    flag_sim = true;
    DEBUG_PRINT("TS01::open : using Simulator\n");
  }
  
  struct hostent*  server_ent = gethostbyname( hostname.c_str() );
  if( !server_ent ){
    return -1;
    perror("gethostbyname");
  }

  sock = socket(AF_INET, SOCK_DGRAM, 0);


  struct sockaddr_in me;
  memset( &me, 0, sizeof(me) );
  me.sin_family = AF_INET;
  me.sin_addr.s_addr = htonl( INADDR_ANY );
  me.sin_port = htons(port);
  
  int ret = bind(sock, (struct sockaddr *)&me, sizeof(me) );
  if( ret<0 ){
    perror("TS01::open : bind");
    return -1;
  }
  
  in_port_t server_port = 10030; //実機の場合はポートを変えてはいけない
  
  if( flag_sim )  //シミュレータ複数の場合はTS01側(sim)のポートも変わる
    server_port += 10*sid; //10030, 10040, 10050
  
  memset( &server, 0, sizeof(server) );
  server.sin_family = AF_INET;
  server.sin_port = htons(server_port);
    
  memcpy(&server.sin_addr, server_ent->h_addr, server_ent->h_length );

  int x = ntohl(server.sin_addr.s_addr);
  char ip_text[32];  
  sprintf(ip_text,"%d.%d.%d.%d",
	  (x>>24)&0xFF, (x>>16)&0xFF, (x>>8)&0xFF, x&0xFF  );
  //DEBUG_PRINT("TS01:: IPaddress [%s]\n",ip_text);

  int st;
  ret = TS01::Impl::read( 0x0000, &st);

  if( ret < 0 ){
    fprintf(stderr,"TS01::open cannot read \n");
    ::close(sock);
    return -1; 
  }
  else{
    DEBUG_PRINT("TS01::open : successfully opened ! (Vender ID = %d)\n",st);
  }

  TS01::Impl::write(0x2008, 0x00 ); //0ch SSI

  TS01::Impl::read(0x9000, &dmode_status ); //最初に一度だけ読み出せばよい

  for(int j=0;j<TS01_DO_CH_NUM;j++){
    //TS-01の分周値の初期値が10なので
    //モード切り替え時にパルスが発生しないよう0で初期化しておく
    TS01::Impl::write(0x9010 + j*4, 0x0 ); //周波数 0
  }

  //カウンタのデフォルト設定
  TS01::Impl::set_count_mode(QUAD_SINGLE);//全ch4テイ倍シングルエンド
  
  DEBUG_PRINT("TS01::open : Driver a4.2\n");
  return sock;
}


void TS01::Impl::close(){
  //stop_count(); //アプリケーションに委ねる

  ::close(sock);

  DEBUG_PRINT("TS01::close::done\n");
}


static const double BPV = 0xffff / 20.0;

TS01::Impl::SendPacket TS01::Impl::register_packet(int addr, size_t size){

  const size_t sp_size = 16+size+1; //スペースワイヤーパケット
  const size_t header_size = 12+16; //SSDTP2+SpaceWireのヘッダーサイズ

  SendPacket packet;
  packet.size = size;
  packet.point = indicator + header_size; //データの位置
  packet.address = &pooling_buf[packet.point];
  
  unsigned char header[64] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, sp_size,
    0xFE,0x01,0x74,0x02,
    0xFE,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00, //addr
    0x00,0x00,size,0x00
  };

  align_data4( &header[20], (unsigned char*)&addr );

  header[27] = RMAP_CalculateCRC(&header[12],15,0x01);

  memcpy( &pooling_buf[indicator], header, header_size );
  //ヘッダー部だけコピー
  
  const size_t ssdtp2_size = 12+sp_size; //SSDTP2パケット
  indicator += ssdtp2_size; //次の登録の位置
  //DEBUG_PRINT("indicator %d\n",indicator);
  return packet;

}


TS01::Impl::Impl(){
  indicator=0;
  p_aout = register_packet(0x10084, 4*TS01_AO_CH_NUM );
  p_load = register_packet(0x6004 , 4 );
  p_dout = register_packet(0x100b4, 4 );
  p_freq = register_packet(0x9010 , 4*TS01_DO_CH_NUM );
  p_led  = register_packet(0x7008 , 4 );
}

void TS01::Impl::update_CRC(const SendPacket& packet){
  pooling_buf[packet.point+packet.size] =
    RMAP_CalculateCRC(packet.address,packet.size,0x01);
}

int  TS01::Impl::write_data(TS01OutputData* data){
  //-------- AOUT -----------------------------------------------
  for(int j=0;j<TS01_AO_CH_NUM;j++){
    int val = BPV * data->u[j];
    align_data4( p_aout.address + j*4, (unsigned char*)&val );
    //    printf("%x%x%x%x\n",j,
    //printf("val%d = %d\n",j,val);
  }
  update_CRC(p_aout);

  int val = 1;
  align_data4( p_load.address, (unsigned char*)&val );
  update_CRC(p_load);

  //---------- DOUT ----------------------------------------------
  unsigned char ddata = 0;
  for(int j=0;j<TS01_DO_CH_NUM;j++){
    ddata |= ((data->dout[j]&0x1) << j) ; //0x1のマスク必須
  }
  
  align_data4( p_dout.address, (unsigned char*)&ddata );
  update_CRC(p_dout);
  

  //----------- CLOCK  ------------------------------------------
  for(int j=0;j<TS01_DO_CH_NUM;j++){
    int val = data->f[j] < 0.1 ? 0 : 10e6 / data->f[j]; //分周値
    align_data4( p_freq.address + j*4, (unsigned char*)&val );
  }
  update_CRC(p_freq);

  //---------- LED ------------------------------------------
  unsigned int lval = 0xff & (count++ / 20);
  align_data4( p_led.address , (unsigned char*)&lval );
  update_CRC(p_led);
  
  ssize_t wsize = sendto( sock, pooling_buf, indicator, 0,
			  (struct sockaddr*)&server, sizeof(server) );//1度だけ
  
  //if( count > 1000000 ) count = 0;
  return wsize;
}

/*
void TS01::write_pulse_frequency(int ch, double f){
  int d = f < 0.1 ? 0 : 10e6 / f; //分周値

  write( 0x9010 + ch*4, d );
}
*/


static const double VPB = 10.0/0xffff;

/***
 * データを取り出して丸ごと変換する
 ***/
void store(TS01InputData* data, unsigned char* packet){
  align_data4( (unsigned char*)&data->sequence, &packet[28] );
  //printf("%x %x %x %x\n",packet[28],packet[29],packet[30],packet[31]);

  short d;
  for(int i=0;i<TS01_AI_CH_NUM;i++){
    align_data2( (unsigned char*)&d, &packet[32+2*i] ); //16bit
    data->v[i] =  VPB * d;
  }
  
  for(int j=0;j<TS01_DI_CH_NUM;j++){
    data->din[j] = (packet[67] >> j) & 0x1;
  }
  for(int i=0;i<TS01_SSI_CH_NUM;i++){
    align_data4( (unsigned char*)&data->ssi[i], &packet[68+4*i] );
    //printf("%d : %8x\n",i, (data->ssi[i]) );//& 0x1ffff );
  }
  
  for(int i=0;i<TS01_CNT_CH_NUM;i++){
    align_data4( (unsigned char*)&data->count[i], &packet[100+4*i] );
    //printf("%d : %8x\n",i, (data->ssi[i]) );//& 0x1ffff );
  }
}

bool check_packet(unsigned char* packet){
  static unsigned char AS_PACKET[133] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x68,//size+16+1,
    0xFE,0x01,0x74,0x02, //12,13,14,15
    0xFE,0x00,0x00,0x00, //16,17,18,19 
    0x00,0x00,0x00,0x00, //20,21,22,23 addr
    0x00,0x00,0x64,0xAA, //24,25,26,27
  };
  AS_PACKET[132] = 0xAA;

  bool flag_error = false;
  for(int i=0;i<133;i++){
    if( i > 27 && i <132) continue;
    //printf("%d :  %x == %x\n",i, packet[i], AS_PACKET[i]);
    if(packet[i] != AS_PACKET[i] ){
      printf("error, %d  %x != %x\n",i, packet[i], AS_PACKET[i]);
      flag_error = true;
    }
  }
  
  return flag_error;
}


int TS01::Impl::start_sampling(int period_us){
  DEBUG_PRINT("TS01::start_sampling\n");
  period = period_us; //不具合発生時再始動のための記憶しておく

  socka = socket(AF_INET, SOCK_DGRAM, 0);

  in_port_t port = 11100 + sid;
  
  struct sockaddr_in me;
  memset( &me, 0, sizeof(me) );
  me.sin_family = AF_INET;
  me.sin_addr.s_addr = htonl( INADDR_ANY );
  me.sin_port = htons(port);  // receiving port 

  DEBUG_PRINT("TS01::start_sampling : port = %d\n",port);

  int ret = bind(socka, (struct sockaddr *)&me, sizeof(me) );
  if( ret<0 ){
    perror("TS01::start_sampling : bind");
    return -1;
  }

  // receiving port の指定
  // UDPパケットの送信元ポートを使って
  // オートサンプリングデータ送信先のポートを指定するためパケットを送る
  struct hostent* server_ent = gethostbyname( hostname.c_str() ); //参照なので使い回しに注意
  if( !server_ent ){
    return -1;
    perror("gethostbyname");
  }

  in_port_t server_port = 10031; //実機の場合はポートを変えてはいけない
  
  if( flag_sim )  //シミュレータ複数の場合はTS01側(sim)のポートも変わる
    server_port += 10*sid; //10031, 10041, 10051
  
  struct sockaddr_in server;
  memset( &server, 0, sizeof(server) );
  server.sin_family = AF_INET;
  server.sin_port = htons(server_port);

  memcpy(&server.sin_addr, server_ent->h_addr, server_ent->h_length );

  unsigned char dummy = 1;
  ssize_t wsize = sendto( socka, &dummy, 1, 0, 
			  (struct sockaddr*)&server, sizeof(server) );
  if( wsize <= 0 ){
    fprintf(stderr,"TS01::start_sampling\n");
    perror("sendto");
  }
  
  usleep(100000);

  write(0x2000, 0x01 ); //オートモードにセット
  
  write(0x2004, period/100 ); //周期設定，スタート

  int st;  
  TS01::Impl::read( 0x2004, &st );//, 4); 

  if( st == period/100 ){
    DEBUG_PRINT("TS01::start_sampling : confirmed period of %d [us]\n",period);
  }
  else{
    fprintf(stderr,"TS01::start_sampling : Period Error %d\n",st);
  }
  DEBUG_PRINT("TS01::start_sampling:done\n");
  return 1;
}

int TS01::Impl::read_autosampling_data(TS01InputData* data){
  ssize_t rsize = recvfrom( socka, rbuf, PACKET_SIZE, 0,
			    NULL, NULL);

  if( rsize == PACKET_SIZE ){
#ifdef DEBUG
    check_packet(rbuf); //デバッグのときだけでよいとする
#endif
    store(data, rbuf); //dataに格納
  }
  else if( rsize < 0 ){
    perror("TS01::read_autosampling_data:recvfrom");
  }
  else{
    fprintf(stderr,"TS01::read_autosampling_data:error : read size = %ld, but expected size = %ld\n",
	    rsize,
	    PACKET_SIZE
	    );
  }
  return rsize;
}

void TS01::Impl::stop_sampling(){
  TS01::Impl::write(0x2004, 0x00 ); //0ms stop
  TS01::Impl::write(0x2000, 0x0  ); //マニュアルモード

  ::close(socka); //auto sampling po
  
  DEBUG_PRINT("TS01::stop_sampling : done\n");
}


void TS01::Impl::set_count_mode(CountMode mode){
  TS01::Impl::write(0x5000, mode);
}

void TS01::Impl::start_count(){
  TS01::Impl::write( 0x5004, 0xff ); // count start (all ch)
}

void TS01::Impl::stop_count(){
  TS01::Impl::write( 0x5008, 0xff ); // count start (all ch)
}

void TS01::Impl::set_count(int value){
  for(int i=0;i<TS01_CNT_CH_NUM;i++)
    TS01::Impl::write( 0x10064 + 4*i, value  ); //set value
}

/*
 * timeout : [ns]
 */
void TS01::Impl::setup_ssi(int ch,
		     short clock,short length,short timeout){
  timeout = timeout / 8; // T ns / 8ns

  TS01::Impl::write( 0x4000 + 4*ch, clock );
  TS01::Impl::write( 0x4020 + 4*ch, length );
  TS01::Impl::write( 0x4040 + 4*ch, timeout );

  // for autosampling mode
  int current_status=0;
  TS01::Impl::read(0x2008, &current_status );

  //DEBUG_PRINT("SSI ch%d %2x -> %2x\n",
  //ch,current_status, current_status | (0x01 << ch) );
  TS01::Impl::write(0x2008, current_status | (0x01 << ch) );
}

void TS01::Impl::set_dout_mode(int ch,bool flag_clock){
  //int current_status=0;
  //TS01::read(0x9000, &current_status );

  if( (dmode_status& (0x1<<ch)) != (flag_clock << ch) ){
    //変更がある場合のみ書き込む
    //printf("%d %d\n", (current_status& (0x1<<ch)),	 (flag_clock << ch) );
    dmode_status = (dmode_status& ~(0x1<<ch) )| (flag_clock << ch);
    //状態を一々読み出さなくてよいようにアプリ側で保持する
    
    TS01::Impl::write(0x9000, dmode_status );
    //TS01::write(0x9000, (dmode_status& ~(0x1<<ch) )| (flag_clock << ch) );
  }
}
/*
void TS01::Impl::apply_config( const TS01Config& config ){
  for(int j=0;j<TS01_SSI_CH_NUM;j++){
    setup_ssi( j, config.ssi_clock[j],
	       config.ssi_length[j], config.ssi_timeout[j] );
  } 

  for(int j=0;j<TS01_DO_CH_NUM;j++){
    set_dout_mode( j, config.dout_mode[j] );
  }
}
*/
//---

ssize_t TS01::write(int addr, int val){ //4 byte
  return impl->write(addr,val);
}

ssize_t TS01::read(int addr, void* data){ //4byte
  return impl->read(addr,data);
}


int TS01::open(std::string hostname2, int num){
  return impl->open(hostname2,num);
}


void TS01::close(){
  impl->close();
}


TS01::TS01() :
  impl( new TS01::Impl ){
}

TS01::~TS01(){
  delete impl;
}


int  TS01::write_data(TS01OutputData* data){
  return impl->write_data(data);
}


int TS01::start_sampling(int period_us){
  return impl->start_sampling(period_us);
}

int TS01::read_autosampling_data(TS01InputData* data){
  return impl->read_autosampling_data(data);
}

void TS01::stop_sampling(){
  impl->stop_sampling();
}

void TS01::set_count_mode(CountMode mode){
  impl->set_count_mode(mode);
}

void TS01::start_count(){
  impl->start_count();
}

void TS01::stop_count(){
  impl->stop_count();
}

void TS01::set_count(int value){
  impl->set_count(value);
}

/*
 * timeout : [ns]
 */
void TS01::setup_ssi(int ch,
		     short clock,short length,short timeout){
  impl->setup_ssi(ch, clock, length, timeout);
}

void TS01::set_dout_mode(int ch,bool flag_clock){
  impl->set_dout_mode(ch, flag_clock);
}
/*
void TS01::apply_config( const TS01Config& config ){
  impl->apply_config( config );
}
*/



