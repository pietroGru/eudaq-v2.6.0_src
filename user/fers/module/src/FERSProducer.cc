#include "eudaq/Producer.hh"
#include "FERS_Registers.h"
#include "FERSlib.h"
#include <iostream>
#include <fstream>
#include <ratio>
#include <chrono>
#include <thread>
#include <random>
#include <errno.h>
#include "stdlib.h"
#ifndef _WIN32
#include <sys/file.h>
#endif

#include "FERS_EUDAQ.h"
#include "configure.h"
#include "JanusC.h"
RunVars_t RunVars;
int SockConsole;	// 0: use stdio console, 1: use socket console
char ErrorMsg[250];	
//int NumBrd=2; // number of boards

Config_t WDcfg;

struct shmseg *shmp;
int shmid;

class FERSProducer : public eudaq::Producer {
	public:
		FERSProducer(const std::string & name, const std::string & runcontrol);
		void DoInitialise() override;
		void DoConfigure() override;
		void DoStartRun() override;
		void DoStopRun() override;
		void DoTerminate() override;
		void DoReset() override;
		void RunLoop() override;

		static const uint32_t m_id_factory = eudaq::cstr2hash("FERSProducer");

	private:
		bool m_flag_ts;
		bool m_flag_tg;
		uint32_t m_plane_id;
		FILE* m_file_lock;
		std::chrono::milliseconds m_ms_busy;
		bool m_exit_of_run;
		std::string fers_final_filename;
		std::string fers_ip_address;  // IP address of the board
		std::string fers_id;
		int handle =-1;		 	// Board handle
		float fers_hv_vbias;
		float fers_hv_imax;
		int fers_acq_mode;
		int vhandle[FERSLIB_MAX_NBRD];
		// staircase params
		uint8_t stair_do;
		uint16_t stair_start, stair_stop, stair_step, stair_shapingt;
		uint32_t stair_dwell_time;
		int m_HG_Gain, m_LG_Gain, m_hold_delay;
		std::string connections_string;
		int m_fers_add_events;
		int brd; // current board

};


namespace{
	auto dummy0 = eudaq::Factory<eudaq::Producer>::
		Register<FERSProducer, const std::string&, const std::string&>(FERSProducer::m_id_factory);
}


FERSProducer::FERSProducer(const std::string & name, const std::string & runcontrol)
	:eudaq::Producer(name, runcontrol), m_file_lock(0), m_exit_of_run(false){

	}


void FERSProducer::DoInitialise(){
	EUDAQ_INFO("Getting init file");
	auto ini = GetInitConfiguration();
	std::string lock_path = ini->Get("FERS_DEV_LOCK_PATH", "ferslockfile.txt");
	EUDAQ_INFO("Lockfile path "+lock_path);
	try{
		m_file_lock = fopen(lock_path.c_str(), "a");
		if (m_file_lock == NULL) std::cerr<<"Lockfile error "<<strerror(errno)<<std::endl;
	}catch(...){
		std::cerr<<"Lockfile error "<<strerror(errno)<<std::endl;
	}
#ifndef _WIN32
	if(flock(fileno(m_file_lock), LOCK_EX|LOCK_NB)){ //fail
		EUDAQ_THROW("unable to lock the lockfile: "+lock_path );
	}
#endif

	fers_ip_address = ini->Get("FERS_IP_ADDRESS", "1.0.0.0");
	fers_id = ini->Get("FERS_ID","");	
	EUDAQ_INFO("FERS Address "+fers_ip_address+" and id "+fers_id);
	//memset(&handle, -1, sizeof(handle));
	for (int i=0; i<FERSLIB_MAX_NBRD; i++)
		vhandle[i] = -1;
	char ip_address[20];
	char connection_path[40];
	strcpy(ip_address, fers_ip_address.c_str());
	sprintf(connection_path,"eth:%s",ip_address);
	connections_string = connection_path;
	
	int ret = FERS_OpenDevice(connection_path, &handle);

	//std::cout <<"-------- ret= "<<ret<<" handle = "<<handle<<std::endl;
	if(ret == 0){
		EUDAQ_INFO("Connected to: " + connections_string);
		vhandle[WDcfg.NumBrd] = handle;
	} else
		EUDAQ_THROW("unable to connect to fers with ip address: "+ fers_ip_address);

	// Readout Mode
	// 0	// Disable sorting 
	// 1	// Enable event sorting by Trigger Tstamp 
	// 2	// Enable event sorting by Trigger ID
	int ROmode = ini->Get("FERS_RO_MODE",0);
	int allocsize;
	FERS_InitReadout(handle,ROmode,&allocsize);


	EUDAQ_INFO("Connected to handle "+std::to_string(handle)
			+" ip "+fers_ip_address+" "+fers_id
		  );
}

//----------DOC-MARK-----BEG*CONF-----DOC-MARK----------
void FERSProducer::DoConfigure(){
	auto conf = GetConfiguration();
	//conf->Print(std::cout);
	m_plane_id = conf->Get("EX0_PLANE_ID", 0);
	m_ms_busy = std::chrono::milliseconds(conf->Get("EX0_DURATION_BUSY_MS", 50));
	m_flag_ts = conf->Get("EX0_ENABLE_TIMESTAMP", 0);
	m_flag_tg = conf->Get("EX0_ENABLE_TRIGERNUMBER", 0);
	unsigned current_time = std::time(0); //check
	if(!m_flag_ts && !m_flag_tg){
		EUDAQ_WARN("Both Timestamp and TriggerNumber are disabled. Now, Timestamp is enabled by default");
		m_flag_ts = false;
		m_flag_tg = true;
	}
	//std::string fers_conf_dir = conf->Get("FERS_CONF_DIR",".");
	std::string fers_conf_filename= conf->Get("FERS_CONF_FILE","NOFILE");
	//std::string conf_filename = fers_conf_dir + fers_conf_file;
	m_LG_Gain = conf->Get("FERS_LG_Gain",0);
	m_HG_Gain = conf->Get("FERS_HG_Gain",0);
	fers_final_filename = conf->Get("FERS_MODIFIED_FOLDER","")+"FERS_"+std::to_string(m_plane_id)+"_run_"+std::to_string(GetRunNumber())+"_"+std::to_string(current_time)+".txt";
	fers_acq_mode = conf->Get("FERS_ACQ_MODE",0);
	m_hold_delay = conf->Get("FERS_HOLD_DELAY",0);
	EUDAQ_INFO("FERSProducer::DoConfigure, file " + fers_final_filename+ " m_LG_Gain "+std::to_string(m_LG_Gain));
	m_fers_add_events = conf->Get("FERS_ADD_EVENTS",0);
	int ret = -1; // to store return code from calls to fers
	//EUDAQ_WARN(fers_conf_dir);
	//EUDAQ_WARN(fers_conf_filename);
	std::ifstream input_file(fers_conf_filename.c_str());
	//std::cout<<fers_conf_filename<<std::endl;
	std::ofstream new_file(fers_final_filename.c_str());
	if (input_file.good() && new_file.good()){
		//std::cout<<"Fers Files open"<<std::endl;
		input_file.seekg(0, std::ios::end);    
		long length = input_file.tellg();           
		input_file.seekg(0, std::ios::beg);  
		//std::cout << "file size: " << length << std::endl;
		for (std::string input_line; std::getline(input_file, input_line); ) {
			if (input_line.find("Open[0]")!=std::string::npos) input_line = "Open[0] "+connections_string;
			if ((input_line.find("LG_Gain")!=std::string::npos) && (m_LG_Gain >0)) input_line = "LG_Gain                            "+std::to_string(m_LG_Gain);
			if ((input_line.find("HG_Gain")!=std::string::npos) && (m_HG_Gain >0)) input_line = "HG_Gain                            "+std::to_string(m_HG_Gain);
			if ((input_line.find("HoldDelay")!=std::string::npos) && (m_hold_delay >0)) input_line = "HoldDelay                          "+std::to_string(m_hold_delay)+" ns";
			new_file<<input_line<<"\n";
			//std::cout<<input_line<<std::endl;
		}
	input_file.close();
	new_file.close();
	} else {
		EUDAQ_WARN("Files not open");
		std::cerr<<"Files not open "<<strerror(errno)<<std::endl;
	}
	//
	FILE* conf_file = fopen(fers_final_filename.c_str(),"r"); //check

	if (conf_file == NULL) 
	{
		EUDAQ_THROW("unable to open config file "+fers_final_filename);
	} else {
		ret = ParseConfigFile(conf_file,&WDcfg, 1);
		if (ret != 0)
		{ 
			fclose(conf_file);
			EUDAQ_THROW("Parsing failed");
		}
	}
	fclose(conf_file);
	//EUDAQ_WARN( "AcquisitionMode: "+std::to_string(WDcfg.AcquisitionMode));

	ret = ConfigureFERS(handle, 0); // 0 = hard, 1 = soft (no acq restart)
	if (ret != 0)
	{
		EUDAQ_THROW("ConfigureFERS failed");
	}


	float fers_dummyvar = 0;
	int ret_dummy = 0;
	

	stair_do = (bool)(conf->Get("stair_do",0));
	stair_shapingt = (uint16_t)(conf->Get("stair_shapingt",0));
	stair_start = (uint16_t)(conf->Get("stair_start",0));
	stair_stop  = (uint16_t)(conf->Get("stair_stop",0));
	stair_step  = (uint16_t)(conf->Get("stair_step",0));
	stair_dwell_time  = (uint32_t)(conf->Get("stair_dwell_time",0));
	sleep(1);
}


void FERSProducer::DoStartRun(){
	m_exit_of_run = false;
	// here the hardware is told to startup
	FERS_SendCommand( handle, CMD_ACQ_START );
	EUDAQ_INFO("StartRun - FERS_ReadoutStatus (0=idle, 1=running) = "+std::to_string(FERS_ReadoutStatus));
}


void FERSProducer::DoStopRun(){
	m_exit_of_run = true;
	FERS_SendCommand( handle, CMD_ACQ_STOP );
	EUDAQ_INFO("StopRun - FERS_ReadoutStatus (0=idle, 1=running) = "+std::to_string(FERS_ReadoutStatus));
}


void FERSProducer::DoReset(){
	m_exit_of_run = true;
	if(m_file_lock){
#ifndef _WIN32
		flock(fileno(m_file_lock), LOCK_UN);
#endif
		fclose(m_file_lock);
		m_file_lock = 0;
	}
	m_ms_busy = std::chrono::milliseconds();
	//m_exit_of_run = false;
	FERS_CloseReadout(handle);
	FERS_CloseDevice(handle);	
	handle = -1;
}


void FERSProducer::DoTerminate(){
	m_exit_of_run = true;
	if(m_file_lock){
		fclose(m_file_lock);
		m_file_lock = 0;
	}
	FERS_CloseReadout(handle);
	FERS_CloseDevice(handle);	
	handle = -1;
}


void FERSProducer::RunLoop(){
	auto tp_start_run = std::chrono::steady_clock::now();
	auto start_clock = std::chrono::system_clock::now();
	uint32_t trigger_n = 0;
	uint8_t x_pixel = 8;
	uint8_t y_pixel = 8;
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint32_t> position(0, x_pixel*y_pixel-1);
	std::uniform_int_distribution<uint32_t> signal(0, 63);
	auto durationSinceEpoch = start_clock.time_since_epoch();

	// Convert the duration to seconds or any other desired unit (e.g., milliseconds)
	auto secondsSinceEpoch = std::chrono::duration_cast<std::chrono::duration<double>>(durationSinceEpoch);

	// Convert the duration to a double type
	double secondsDouble = secondsSinceEpoch.count();
	std::cout<<secondsDouble<<std::endl;
	// std::this_thread::sleep_until(tp_start_run+std::chrono::milliseconds(100));
	while(!m_exit_of_run){

		// staircase?
		static bool stairdone = false;

		int nchan = x_pixel*y_pixel;
		int DataQualifier = -1;
		void *Event;

		//auto ev = eudaq::Event::MakeUnique("FERSProducer_"+std::to_string(m_plane_id));
		auto ev = eudaq::Event::MakeUnique("fers"); 

		
		//auto tp_end_of_polling = tp_trigger + std::chrono::milliseconds(10);

		double tstamp_us = -1;
		int nb = -1;
		int bindex = -1;
		int status = -1;

		// real data
		// 
		// while(!FERS_GetEvent(vhandle, &bindex, &DataQualifier, &tstamp_us, &Event, &nb) && !m_exit_of_run){
		// 	auto tp_end_of_polling = std::chrono::steady_clock::now() + std::chrono::milliseconds(10);
		// 	std::this_thread::sleep_until(tp_end_of_polling);
		// }

		status= FERS_GetEvent(vhandle, &bindex, &DataQualifier, &tstamp_us, &Event, &nb);
		//if (status > 0)std::cout<<"--status of FERS_GetEvent (0=No Data, 1=Good Data 2=Not Running, <0 = error) = "<< std::to_string(status)<<std::endl;
		//
		auto tp_trigger = std::chrono::steady_clock::now();
		auto tp_end_of_busy = tp_trigger + m_ms_busy;
		// event creation
		if ( DataQualifier >0 || (trigger_n < m_fers_add_events) ) {

			std::chrono::nanoseconds du_ts_beg_ns(tp_trigger - tp_start_run);
			std::chrono::nanoseconds du_ts_end_ns(tp_end_of_busy - tp_start_run);

			ev->SetTimestamp(du_ts_beg_ns.count(), du_ts_end_ns.count());
			ev->SetTriggerN(trigger_n);

			ev->SetTag("Detector_ID", std::to_string(m_plane_id));
			if (trigger_n == 0) {
				ev->SetTag("LGgain", std::to_string(m_LG_Gain));
				ev->SetTag("HGgain", std::to_string(m_HG_Gain));
				ev->SetTag("HoldDelay", std::to_string(m_hold_delay));
				ev->SetTag("fers_final_filename", fers_final_filename);
			}
			//std::cout<<"--FERS_ReadoutStatus (0=idle, 1=running) = " << FERS_ReadoutStatus <<std::endl;
			//std::cout<<"--status of FERS_GetEvent (0=No Data, 1=Good Data 2=Not Running, <0 = error) = "<< std::to_string(status)<<std::endl;
			//std::cout<<"  --bindex = "<< std::to_string(bindex) <<" tstamp_us = "<< std::to_string(tstamp_us) <<std::endl;
			//std::cout<<"  --DataQualifier = "<< std::to_string(DataQualifier) +" nb = "<< std::to_string(nb) <<std::endl;
			
			std::vector<uint8_t> data;
			if (DataQualifier ==17) DataQualifier=1;
			//make_header(handle, x_pixel, y_pixel, DataQualifier, &data);
			//make_header(brd, DataQualifier, &data);
			double run_time =  ev->GetTimestampEnd() -  ev->GetTimestampBegin();
			FERSpack_CLEAR_event(Event, m_plane_id, GetRunNumber(), trigger_n, m_fers_add_events, secondsDouble, data);
			uint32_t block_id = m_plane_id;
			ev->SetRunN(GetRunNumber());
			ev->SetEventN(trigger_n);

			ev->AddBlock(0, data);
			SendEvent(std::move(ev));
			
			// std::this_thread::sleep_until(tp_end_of_busy);
			if (trigger_n < m_fers_add_events) std::this_thread::sleep_until(tp_end_of_busy);
			trigger_n++;
		}else{
			//std::this_thread::sleep_until(tp_end_of_polling);
		}
	}
}