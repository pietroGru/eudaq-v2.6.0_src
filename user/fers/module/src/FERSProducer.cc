#include "eudaq/Producer.hh"
#include <iostream>
#include <fstream>
#include <ratio>
#include <chrono>
#include <thread>
#include <errno.h>
#include "stdlib.h"

#include "FERS_EUDAQ.h"
#include "configure.h"
#include "FERSlib.h"
// #include "JanusC.h"

int SockConsole;	// 0: use stdio console, 1: use socket console
char ErrorMsg[250];	
//int NumBrd=2; // number of boards

Config_t WDcfg;


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
		uint32_t detector_ID;
		FILE* m_file_lock;
		std::chrono::milliseconds m_ms_busy;
		bool m_exit_of_run;
		std::string fers_final_filename;
		std::string fers_ip_address;  // IP address of the board
		std::string fers_id;
		int handle[MAX_NBRD];		 	// Board handle
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

		int a1, a2, AllocSize;
		int ROmode, brdInWarning[FERSLIB_MAX_NBRD] = {};

		// Acquisition state
		int AcqStatus = 0;

		// Timestamp when the FERS run start returns
		int64_t runStartTime_ns;
		unsigned int run_n = -1;
};

namespace{
	auto dummy0 = eudaq::Factory<eudaq::Producer>::
		Register<FERSProducer, const std::string&, const std::string&>(FERSProducer::m_id_factory);
}


FERSProducer::FERSProducer(const std::string & name, const std::string & runcontrol)
	:eudaq::Producer(name, runcontrol),
	m_file_lock(0),
	m_exit_of_run(false)
	{
		std::cout << "Hello from FERSProducer!" << std::endl;
	}


void FERSProducer::DoInitialise(){
	auto ini = GetInitConfiguration();

	fers_ip_address = ini->Get("FERS_IP_ADDRESS", "1.0.0.0");
	char ip_address[20];
	strcpy(ip_address, fers_ip_address.c_str());
	char connection_path[40];
	sprintf(connection_path,"eth:%s",ip_address);

	fers_id = ini->Get("FERS_ID","0");	
	
	EUDAQ_INFO("FERS Address "+fers_ip_address+" and id "+fers_id);

	for (int i=0; i<FERSLIB_MAX_NBRD; i++)
		vhandle[i] = -1;
	
	int ret = FERS_OpenDevice(connection_path, handle);
	if(ret == 0){
		EUDAQ_INFO("Connected to: " + std::string(connection_path));
	}else{
		EUDAQ_THROW("unable to connect to fers with ip address: "+ fers_ip_address);
	}
	
	memset(handle, -1, sizeof(*handle) * MAX_NBRD);
	std::cout << "FERSProducer::DoInitialise() - FERS_OpenDevice() returned: " << handle << std::endl;
	AcqStatus = ACQSTATUS_HW_CONNECTED;

	auto ROmode = (WDcfg.EventBuildingMode != 0) ? 1 : 0;
	for (int b = 0; b < WDcfg.NumBrd; b++) {
		FERS_InitReadout(handle[b], ROmode, &a1);
	}

	AcqStatus = ACQSTATUS_READY;
}

//----------DOC-MARK-----BEG*CONF-----DOC-MARK----------
void FERSProducer::DoConfigure(){
	auto conf = GetConfiguration();
	// conf->Print(std::cout);

	detector_ID = conf->Get("detector_ID", 0);

	// Read the FERS file configuration from its file
	std::string ConfigFileName = conf->Get("FERS_CONF_FILE", "NOFILE");
	auto cfg = fopen(ConfigFileName.c_str(), "r");
	if (cfg == NULL) {
		EUDAQ_THROW("unable to open config file "+fers_final_filename);
	}
	int ret = ParseConfigFile(cfg, &WDcfg, 1);
	fclose(cfg);
	
	// This section allows to override some configuration by inserting the corresponding key in the section
	// of the config file
	
	auto AcquisitionMode_local = conf->Get("AcquisitionMode", NULL);
	if (AcquisitionMode_local != NULL) WDcfg.AcquisitionMode = AcquisitionMode_local;
	auto EnableToT_local = conf->Get("EnableToT", NULL);
	if (EnableToT_local != NULL) WDcfg.EnableToT = EnableToT_local;
	// auto BunchTrgSource_local = conf->Get("BunchTrgSource", NULL);
	// if (BunchTrgSource_local != NULL) WDcfg.BunchTrgSource = BunchTrgSource_local;
	// auto VetoSource_local = conf->Get("VetoSource", NULL);
	// if (VetoSource_local != NULL) WDcfg.VetoSource = VetoSource_local;
	// auto ValidationSource_local = conf->Get("ValidationSource", NULL);
	// if (ValidationSource_local != NULL) WDcfg.ValidationSource = ValidationSource_local;
	// auto ValidationMode_local = conf->Get("ValidationMode", NULL);
	// if (ValidationMode_local != NULL) WDcfg.ValidationMode = ValidationMode_local;
	// auto CountingMode_local = conf->Get("CountingMode", NULL);
	// if (CountingMode_local != NULL) WDcfg.CountingMode = CountingMode_local;
	// auto ChTrg_Width_local = conf->Get("ChTrg_Width", NULL);
	// if (ChTrg_Width_local != NULL) WDcfg.ChTrg_Width = ChTrg_Width_local;
	auto EnableCntZeroSuppr_local = conf->Get("EnableCntZeroSuppr", NULL);
	if (EnableCntZeroSuppr_local != NULL) WDcfg.EnableCntZeroSuppr = EnableCntZeroSuppr_local;
	auto TrgIdMode_local = conf->Get("TrgIdMode", NULL);
	if (TrgIdMode_local != NULL) WDcfg.TrgIdMode = TrgIdMode_local;
	auto TriggerLogic_local = conf->Get("TriggerLogic", NULL);
	if (TriggerLogic_local != NULL) WDcfg.TriggerLogic = TriggerLogic_local;
	// auto Tlogic_Width_local = conf->Get("Tlogic_Width", NULL);
	// if (Tlogic_Width_local != NULL) WDcfg.Tlogic_Width = Tlogic_Width_local;
	auto MajorityLevel_local = conf->Get("MajorityLevel", NULL);
	if (MajorityLevel_local != NULL) WDcfg.MajorityLevel = MajorityLevel_local;
	auto PtrgPeriod_local = conf->Get("PtrgPeriod", NULL);
	if (PtrgPeriod_local != NULL) WDcfg.PtrgPeriod = PtrgPeriod_local;
	// auto TrefSource_local = conf->Get("TrefSource", NULL);
	// if (TrefSource_local != NULL) WDcfg.TrefSource = TrefSource_local;
	auto TrefWindow_local = conf->Get("TrefWindow", NULL);
	if (TrefWindow_local != NULL) WDcfg.TrefWindow = TrefWindow_local;
	auto TrefDelay_local = conf->Get("TrefDelay", NULL);
	if (TrefDelay_local != NULL) WDcfg.TrefDelay = TrefDelay_local;
	// auto T0_Out_local = conf->Get("T0_Out", NULL);
	// if (T0_Out_local != NULL) WDcfg.T0_Out = T0_Out_local;
	// auto T1_Out_local = conf->Get("T1_Out", NULL);
	// if (T1_Out_local != NULL) WDcfg.T1_Out = T1_Out_local;
	// auto ChEnableMask0_local = conf->Get("ChEnableMask0", NULL);
	// if (ChEnableMask0_local != NULL) WDcfg.ChEnableMask0 = ChEnableMask0_local;
	// auto ChEnableMask1_local = conf->Get("ChEnableMask1", NULL);
	// if (ChEnableMask1_local != NULL) WDcfg.ChEnableMask1 = ChEnableMask1_local;
	auto GainSelect_local = conf->Get("GainSelect", NULL);
	if (GainSelect_local != NULL) WDcfg.GainSelect = GainSelect_local;
	// auto HG_Gain_local = conf->Get("HG_Gain", NULL);
	// if (HG_Gain_local != NULL) WDcfg.HG_Gain = HG_Gain_local;
	// auto LG_Gain_local = conf->Get("LG_Gain", NULL);
	// if (LG_Gain_local != NULL) WDcfg.LG_Gain = LG_Gain_local;
	auto Pedestal_local = conf->Get("Pedestal", NULL);
	if (Pedestal_local != NULL) WDcfg.Pedestal = Pedestal_local;
	// auto ZS_Threshold_LG_local = conf->Get("ZS_Threshold_LG", NULL);
	// if (ZS_Threshold_LG_local != NULL) WDcfg.ZS_Threshold_LG = ZS_Threshold_LG_local;
	// auto ZS_Threshold_HG_local = conf->Get("ZS_Threshold_HG", NULL);
	// if (ZS_Threshold_HG_local != NULL) WDcfg.ZS_Threshold_HG = ZS_Threshold_HG_local;
	auto HG_ShapingTime_local = conf->Get("HG_ShapingTime", NULL);
	if (HG_ShapingTime_local != NULL) WDcfg.HG_ShapingTime = HG_ShapingTime_local;
	auto LG_ShapingTime_local = conf->Get("LG_ShapingTime", NULL);
	if (LG_ShapingTime_local != NULL) WDcfg.LG_ShapingTime = LG_ShapingTime_local;
	auto HoldDelay_local = conf->Get("HoldDelay", NULL);
	if (HoldDelay_local != NULL) WDcfg.HoldDelay = HoldDelay_local;
	auto MuxClkPeriod_local = conf->Get("MuxClkPeriod", NULL);
	if (MuxClkPeriod_local != NULL) WDcfg.MuxClkPeriod = MuxClkPeriod_local;
	auto EHistoNbin_local = conf->Get("EHistoNbin", NULL);
	if (EHistoNbin_local != NULL) WDcfg.EHistoNbin = EHistoNbin_local;
	auto ToAHistoNbin_local = conf->Get("ToAHistoNbin", NULL);
	if (ToAHistoNbin_local != NULL) WDcfg.ToAHistoNbin = ToAHistoNbin_local;
	auto ToARebin_local = conf->Get("ToARebin", NULL);
	if (ToARebin_local != NULL) WDcfg.ToARebin = ToARebin_local;
	auto ToAHistoMin_local = conf->Get("ToAHistoMin", NULL);
	if (ToAHistoMin_local != NULL) WDcfg.ToAHistoMin = ToAHistoMin_local;
	auto MCSHistoNbin_local = conf->Get("MCSHistoNbin", NULL);
	if (MCSHistoNbin_local != NULL) WDcfg.MCSHistoNbin = MCSHistoNbin_local;


	// for (int b = 0; b < WDcfg.NumBrd; b++) {
	// 	ret = ConfigureFERS(handle[b], CFG_HARD);
	// 	if (ret < 0)	EUDAQ_THROW(ret);
	// }
	ret = ConfigureFERS(handle[0], CFG_HARD);
	// if (ret < 0)	EUDAQ_THROW(ret);

	m_fers_add_events = conf->Get("FERS_ADD_EVENTS",0);
}


void FERSProducer::DoStartRun(){
	m_exit_of_run = false;
	// here the hardware is told to startup
	auto ret = FERS_StartAcquisition(handle, 1, STARTRUN_ASYNC);
	auto now = std::chrono::system_clock::now();
	runStartTime_ns = now.time_since_epoch().count();
	if (ret != 0){
		EUDAQ_THROW("FERS_StartAcquisition failed");
		AcqStatus = ACQSTATUS_ERROR;
	}
	AcqStatus = ACQSTATUS_RUNNING;
	run_n += 1;
}


void FERSProducer::DoStopRun(){
	m_exit_of_run = true;
	auto ret = FERS_StopAcquisition(handle, 1, STARTRUN_ASYNC);
	FERS_FlushData(handle[0]);
	if (ret != 0){
		EUDAQ_THROW("FERS_StopAcquisition failed");
		AcqStatus = ACQSTATUS_ERROR;
	}
	AcqStatus = ACQSTATUS_READY;
}


void FERSProducer::DoReset(){
	m_exit_of_run = true;

	if(AcqStatus == ACQSTATUS_RUNNING){
		// call stop acquisition and then mark ready
		DoTerminate();
		AcqStatus = ACQSTATUS_RESTARTING;
	}
}


void FERSProducer::DoTerminate(){
	m_exit_of_run = true;

	DoStopRun();
	FERS_CloseDevice(handle[0]);
	FERS_CloseReadout(handle[0]);
}


void FERSProducer::RunLoop(){
	auto tp_start_run = std::chrono::steady_clock::now();
	auto start_clock = std::chrono::system_clock::now();
	uint32_t trigger_n = 0;

	// Convert the duration to seconds or any other desired unit (e.g., milliseconds)
	auto durationSinceEpoch = start_clock.time_since_epoch();
	auto secondsSinceEpoch = std::chrono::duration_cast<std::chrono::duration<double>>(durationSinceEpoch);
	// Convert the duration to a double type
	double secondsDouble = secondsSinceEpoch.count();

	int brd, DataQualifier, nb;
	DataQualifier = -5;
	double tstamp_us;
	void *Event;

	int current_trgid[MAX_NBRD];
	double current_tstamp_us[MAX_NBRD];
	double current_eventAbs_tstamp_ns[MAX_NBRD] = {runStartTime_ns};
	

	while(!m_exit_of_run){
		// Pool the FERS card to het an event
		int status = FERS_GetEvent(handle, &brd, &DataQualifier, &tstamp_us, &Event, &nb);

		// elapsedPC_s = (Stats.current_time > Stats.start_time) ? ((float)(Stats.current_time - Stats.start_time)) / 1000 : 0;
		// elapsedBRD_s = (float)(Stats.current_tstamp_us[0] * 1e-6);

		if(status==1){
			auto now = std::chrono::system_clock::now();
			auto time_ns = now.time_since_epoch().count();
			//if (status > 0)std::cout<<"--status of FERS_GetEvent (0=No Data, 1=Good Data 2=Not Running, <0 = error) = "<< std::to_string(status)<<std::endl;
			//
			auto tp_trigger = std::chrono::steady_clock::now();
			auto tp_end_of_busy = tp_trigger + m_ms_busy;
			// event creation
			if(((DataQualifier & 0xF) == DTQ_SPECT) || ((DataQualifier & 0xF) == DTQ_TSPECT)){
				SpectEvent_t* Ev = (SpectEvent_t*)Event;
				current_trgid[brd] = Ev->trigger_id;
				current_tstamp_us[brd] = tstamp_us;
				current_eventAbs_tstamp_ns[brd] = runStartTime_ns + tstamp_us*1e3;

				// SaveList(b, Stats.current_tstamp_us[b], Stats.current_trgid[b], Ev, dtq);
				if(DataQualifier & DTQ_SPECT){
					
					uint8_t datatype = 0x0;	// XXTA XCHL	C=Counting T=ToT A=ToA (timestamp) H=HG L=LG - not use in Counting/Timimng mode alone for the moment
					uint8_t i, b8 = brd;

					CLEAR_nametypes Ev_data;
					Ev_data.run = run_n;
					Ev_data.runTime = runStartTime_ns*1e-9;
					Ev_data.event = trigger_n;
					Ev_data.fers_evt = Ev->trigger_id;
					Ev_data.fers_trgtime = tstamp_us*1e-6;
					Ev_data.timestamp = (runStartTime_ns+tstamp_us*1e3)*1e-9;
					Ev_data.timestamp_sw = time_ns*1e-9;
					// Ev_data.hold = ;
					// Ev_data.gain = ;
					// Ev_data.fers_ch = static_cast<uint64_t>(Ev->chmask);
					// // Ev_data.strip = ;
					// Ev_data.lg = static_cast<int32_t>(Ev->energyLG);
					// Ev_data.hg = static_cast<int32_t>(Ev->energyHG);
					
					// Create the event
					auto eudaqEv = eudaq::Event::MakeUnique("fers"); 
					eudaqEv->SetTag("Detector_ID", std::to_string(detector_ID));
					eudaqEv->SetTag("LGgain", std::to_string(m_LG_Gain));
					eudaqEv->SetTag("HGgain", std::to_string(m_HG_Gain));
					eudaqEv->SetTag("HoldDelay", std::to_string(m_hold_delay));
					eudaqEv->SetTag("fers_final_filename", fers_final_filename);

					eudaqEv->SetRunN(GetRunNumber());
					eudaqEv->SetEventN(trigger_n);
					eudaqEv->SetTriggerN(trigger_n);

					eudaqEv->SetTimestamp(Ev_data.timestamp*1e9, Ev_data.timestamp_sw*1e9);
					
					std::vector<uint8_t> buffer(sizeof(CLEAR_nametypes));
					// Copy the structure's memory into the vector
    				std::memcpy(buffer.data(), &Ev_data, sizeof(CLEAR_nametypes));

					eudaqEv->AddBlock(0, buffer);
					SendEvent(std::move(eudaqEv));
				}


				// if (trigger_n < m_fers_add_events) std::this_thread::sleep_until(tp_end_of_busy);
				trigger_n++;
				// EUDAQ_INFO("DataQualifier is SPECT: 		"+std::to_string(DataQualifier));
			}else{
				// EUDAQ_WARN("DataQualifier is not SPECT: "+std::to_string(DataQualifier));
			}
		}else if(status<0){
			// Error, stop the acquisition
			EUDAQ_THROW("Error in FERS_GetEvent: "+std::to_string(status));
			m_exit_of_run = true;
		}

		// This is to reduce the polling rate in order not to reduce CPU load and not overload the bandwidth 
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}