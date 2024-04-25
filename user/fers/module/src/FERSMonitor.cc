/////////////////////////////////////////////////////////////////////
//                         2023 May 08                             //
//                   authors: R. Persiani & F. Tortorici           //
//                email: rinopersiani@gmail.com                    //
//                email: francesco.tortorici@ct.infn.it            //
//                        notes:                                   //
/////////////////////////////////////////////////////////////////////

#include "eudaq/Monitor.hh"
#include "eudaq/StandardEvent.hh"
#include "eudaq/StdEventConverter.hh"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include "FERSlib.h"

#include "eudaq/RawEvent.hh"

#include "FERS_EUDAQ.h"

// all the event types
SpectEvent_t    EventSpect;
ListEvent_t     EventList;
CountingEvent_t EventCount;
WaveEvent_t     EventWave;
TestEvent_t     EventTest;
StaircaseEvent_t StaircaseEvent;
// all struct vars
// use them to print etc
//uint8_t x_pixel = 8; // can be overwritten by the event header if needed
//uint8_t y_pixel = 8; // this too
uint8_t dataq;
int nchan = 64;//x_pixel*y_pixel;
double   tstamp_us                      ;
uint64_t trigger_id                     ;
uint64_t chmask                         ;
uint64_t qdmask                         ;
uint16_t energyHG[MAX_LIST_SIZE]        ;
uint16_t energyLG[MAX_LIST_SIZE]        ;
uint32_t tstamp[MAX_LIST_SIZE]          ;
uint16_t ToT[MAX_LIST_SIZE]             ;
uint32_t counts[MAX_LIST_SIZE]          ;
uint32_t t_or_counts                    ;
uint32_t q_or_counts                    ;
uint16_t ns                             ;
uint16_t wave_hg[MAX_WAVEFORM_LENGTH]   ;
uint16_t wave_lg[MAX_WAVEFORM_LENGTH]   ;
uint8_t  dig_probes[MAX_WAVEFORM_LENGTH];
uint16_t nhits                          ;
uint8_t  channel[MAX_LIST_SIZE]         ;
uint16_t nwords                         ;
uint32_t test_data[MAX_TEST_NWORDS]     ;

uint16_t threshold;
uint16_t dwell_time; // in seconds, divide hitcnt by this to get rate
uint32_t chmean; // over channels, no division by time
uint16_t shapingt; // enum, see FERS_Registers.h
float    HV;
uint32_t Tor_cnt;
uint32_t Qor_cnt;
uint32_t hitcnt[FERSLIB_MAX_NCH];

extern struct shmseg *shmp;
extern int shmid;

class FERSEventConverter: public eudaq::StdEventConverter{
public:
  bool Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const override;
  static const uint32_t m_id_factory = eudaq::cstr2hash("FERSProducer");
};
namespace{
  auto dummy1 = eudaq::Factory<eudaq::StdEventConverter>::
    Register<FERSEventConverter>(FERSEventConverter::m_id_factory);
}

//----------DOC-MARK-----BEG*DEC-----DOC-MARK----------
class FERSMonitor : public eudaq::Monitor {
	public:
		FERSMonitor(const std::string & name, const std::string & runcontrol);
		void DoInitialise() override;
		void DoConfigure() override;
		void DoStartRun() override;
		void DoStopRun() override;
		void DoTerminate() override;
		void DoReset() override;
		void DoReceive(eudaq::EventSP ev) override;

		static const uint32_t m_id_factory = eudaq::cstr2hash("FERSMonitor");

		bool Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2) const;

	private:
		bool m_en_print;
		bool m_en_std_converter;
		bool m_en_std_print;

		int brd;
};

namespace{
	auto dummy0 = eudaq::Factory<eudaq::Monitor>::
		Register<FERSMonitor, const std::string&, const std::string&>(FERSMonitor::m_id_factory);

	//auto dummy1 = eudaq::Factory<eudaq::StdEventConverter>::
	//  Register<FERSEventConverter>(FERSEventConverter::m_id_factory);
}

FERSMonitor::FERSMonitor(const std::string & name, const std::string & runcontrol)
	:eudaq::Monitor(name, runcontrol){
	}

void FERSMonitor::DoInitialise(){
	shmid = shmget(SHM_KEY, sizeof(struct shmseg), 0644|IPC_CREAT);
	if (shmid == -1) {
		perror("Shared memory");
	}
	EUDAQ_WARN("monitor init: shmid = "+std::to_string(shmid));
	shmp = (shmseg*)shmat(shmid, NULL,0);
	if (shmp == (void*)-1) {
		perror("Shared memory attach");
	}

	auto ini = GetInitConfiguration();
	ini->Print(std::cout);
}

void FERSMonitor::DoConfigure(){
	auto conf = GetConfiguration();
	conf->Print(std::cout);
	m_en_print = conf->Get("EX0_ENABLE_PRINT", 1);
	m_en_std_converter = conf->Get("EX0_ENABLE_STD_CONVERTER", 0);
	m_en_std_print = conf->Get("EX0_ENABLE_STD_PRINT", 0);

	char printme[200] = "";
	std::sprintf( printme, "monitor > print = %d std_converter = %d std_print = %d", m_en_print, m_en_std_converter, m_en_print);
	EUDAQ_INFO(printme);
}

void FERSMonitor::DoStartRun(){
	openasciistream(shmp, brd);
}

void FERSMonitor::DoStopRun(){
	closeasciistream(shmp);
}

void FERSMonitor::DoReset(){
}

void FERSMonitor::DoTerminate(){
}

void FERSMonitor::DoReceive(eudaq::EventSP ev){
	if(m_en_print) 
		ev->Print(std::cout);

	float vmon, imon;

	// dump su log
	std::string printme="";
	size_t nblocks= ev->NumBlocks();
	EUDAQ_INFO("Monitor > number of blocks: "+std::to_string(nblocks));
	auto block_n_list = ev->GetBlockNumList();

	for(auto &block_n: block_n_list){
		std::vector<uint8_t> block = ev->GetBlock(block_n);

		//uint8_t x_pixel;
		//uint8_t y_pixel;
		//uint8_t dataq;
		//int index = read_header(&block, &x_pixel, &y_pixel, &dataq);
		int index = read_header(&block, &brd, &dataq);

		int expected_size = ( *( event_lengths.find(dataq) ) ).second;
		int x_pixel = int(sqrt(shmp->nchannels[brd]));
		int y_pixel = int(sqrt(shmp->nchannels[brd]));

		if(block.size() < index + expected_size) // map event_lengths is defined in FERS_EUDAQ.h
			EUDAQ_THROW("Unknown data");

		//EUDAQ_INFO("monitor > " + std::to_string(index) +
		//" x_pixel: " +	std::to_string(x_pixel) + " y_pixel: " + std::to_string(y_pixel) +
		//" DataQualifier : " +	std::to_string(dataq) );
		std::vector<uint8_t> data(block.begin()+index, block.end());
		//EUDAQ_INFO("monitor > data size: "+std::to_string(data.size()));

		//dump_vec("data in monitor",&data,index+28,index+28+2*8);

		//		// all the event types
		//		SpectEvent_t    EventSpect;
		//		ListEvent_t     EventList;
		//		CountingEvent_t EventCount;
		//		WaveEvent_t     EventWave;
		//		TestEvent_t     EventTest;
		//		StaircaseEvent_t StaircaseEvent;
		//		// all struct vars
		//		// use them to print etc
		//		int nchan = x_pixel*y_pixel;
		//		double   tstamp_us                      ;
		//		uint64_t trigger_id                     ;
		//		uint64_t chmask                         ;
		//		uint64_t qdmask                         ;
		//		uint16_t energyHG[nchan]                ;
		//		uint16_t energyLG[nchan]                ;
		//		uint32_t tstamp[nchan]                  ;
		//		uint16_t ToT[nchan]                     ;
		//		uint32_t counts[nchan]                  ;
		//		uint32_t t_or_counts                    ;
		//		uint32_t q_or_counts                    ;
		//		uint16_t ns                             ;
		//		uint16_t wave_hg[MAX_WAVEFORM_LENGTH]   ;
		//		uint16_t wave_lg[MAX_WAVEFORM_LENGTH]   ;
		//		uint8_t  dig_probes[MAX_WAVEFORM_LENGTH];
		//		uint16_t nhits                          ;
		//		uint8_t  channel[MAX_LIST_SIZE]         ;
		//		uint16_t nwords                         ;
		//		uint32_t test_data[MAX_TEST_NWORDS]     ;
		//
		//		uint16_t threshold;
		//		uint16_t dwell_time; // in seconds, divide hitcnt by this to get rate
		//		uint32_t chmean; // over channels, no division by time
		//		uint16_t shapingt; // enum, see FERS_Registers.h
		//		float    HV;
		//		uint32_t Tor_cnt;
		//		uint32_t Qor_cnt;
		//		uint32_t hitcnt[FERSLIB_MAX_NCH];
		//
		//		std::vector<uint16_t> hit; // fill it with energyHG or whatever
		std::string hitname=""; // display name for hit
		int hitrows, hitcols; // how many rows and columns to display
		std::vector<uint16_t> hit; // fill it with energyHG or whatever

		std::string ascii = "";
		dumpshm( shmp, brd);
		// string to prefix, in order to identify the board responsible for the current message in the log
		std::string prefix = "["+std::string(shmp->IP[brd])+"]: ";
		EUDAQ_INFO(prefix+"Monitor > ---------- start dumping (block "+std::to_string(block_n)+")");
		switch ( dataq ) {
			case DTQ_SPECT:
				EUDAQ_WARN(prefix+"Trying to decode SPECT event");
				// fill the relevant vars
				EventSpect = FERSunpack_spectevent(&data);
				tstamp_us      = EventSpect.tstamp_us ;
				trigger_id     = EventSpect.trigger_id;
				chmask         = EventSpect.chmask    ;
				qdmask         = EventSpect.qdmask    ;
				for (int i=0; i<nchan; i++)
				{
					energyHG[i] = EventSpect.energyHG[i];
					energyLG[i] = EventSpect.energyLG[i];
					// hit is dumped below, for test purposes
					hit.push_back( energyHG[i] );
					hitname="energyHG";
				}
				hitcols = y_pixel;
				hitrows = x_pixel;
				// dump the scalar ones
				EUDAQ_INFO(prefix+"tstamp_us : "+std::to_string(tstamp_us ));
				EUDAQ_INFO(prefix+"trigger_id: "+std::to_string(trigger_id));
				EUDAQ_INFO(prefix+"chmask    : "+std::to_string(chmask    ));
				EUDAQ_INFO(prefix+"qdmask    : "+std::to_string(qdmask    ));

				ascii += std::to_string(tstamp_us);
				break;
			case DTQ_TIMING:
				EUDAQ_WARN(prefix+"Trying to decode TIMING event");
				EventList = FERSunpack_listevent(&data);
				nhits = EventList.nhits;
				for (int i=0; i<MAX_LIST_SIZE; i++)
				{
					channel[i] = EventList.channel[i];
					tstamp[i] = EventList.tstamp[i];
					ToT[i]  = EventList.ToT[i];
					// hit is dumped below,for test purposes
					hit.push_back( ToT[i] );
					hitname="ToT";
				}
				hitcols = y_pixel;
				hitrows = MAX_LIST_SIZE / hitcols;
				// dump the scalar ones
				EUDAQ_INFO(prefix+"nhits : "+std::to_string(nhits ));
				ascii += std::to_string(nhits);
				break;
			case DTQ_COUNT:
				EUDAQ_WARN(prefix+"Trying to decode COUNTING event");
				EventCount = FERSunpack_countevent(&data);
				tstamp_us   = EventCount.tstamp_us;
				trigger_id  = EventCount.trigger_id;
				chmask      = EventCount.chmask;
				for (int i=0; i<nchan; i++)
				{
					counts[i]   = EventCount.counts[i];
					// hit is dumped below,for test purposes
					hit.push_back( counts[i] );
					hitname="counts";
				}
				hitcols = y_pixel;
				hitrows = x_pixel;
				t_or_counts = EventCount.t_or_counts;
				q_or_counts = EventCount.q_or_counts;
				// dump the scalar ones
				EUDAQ_INFO(prefix+"tstamp_us : "+std::to_string(tstamp_us ));
				EUDAQ_INFO(prefix+"trigger_id : "+std::to_string(trigger_id ));
				EUDAQ_INFO(prefix+"chmask : "+std::to_string(chmask ));
				EUDAQ_INFO(prefix+"t_or_counts : "+std::to_string(t_or_counts ));
				EUDAQ_INFO(prefix+"q_or_counts : "+std::to_string(q_or_counts ));
				ascii += std::to_string(tstamp_us);
				break;
			case DTQ_WAVE:
				EUDAQ_WARN(prefix+"Trying to decode WAVE event, espected "+std::to_string(sizeof(EventWave))+" bytes");
				EventWave = FERSunpack_waveevent(&data);
				EUDAQ_INFO(prefix+"unpack ok");
				tstamp_us = EventWave.tstamp_us;
				trigger_id= EventWave.trigger_id;
				ns        = EventWave.ns;
				for (int i=0; i<MAX_WAVEFORM_LENGTH; i++)
				{
					wave_hg[i]= EventWave.wave_hg[i];
					wave_lg[i]= EventWave.wave_lg[i];
					dig_probes[i] = EventWave.dig_probes[i];
					// hit is dumped below,for test purposes
					hit.push_back( wave_hg[i] );
					hitname="wave_hg";
				}
				hitcols = y_pixel;
				hitrows = MAX_WAVEFORM_LENGTH/hitcols;
				// dump the scalar ones
				EUDAQ_INFO(prefix+"trigger_id : "+std::to_string(trigger_id ));
				EUDAQ_INFO(prefix+"tstamp_us : "+std::to_string(tstamp_us ));
				EUDAQ_INFO(prefix+"ns : "+std::to_string(ns ));
				ascii += std::to_string(tstamp_us);
				break;
			case DTQ_TSPECT:
				EUDAQ_WARN(prefix+"Trying to decode TSPECT event");
				EventSpect = FERSunpack_tspectevent(&data);
				// fill the relevant vars
				tstamp_us      = EventSpect.tstamp_us;
				trigger_id     = EventSpect.trigger_id;
				chmask         = EventSpect.chmask;
				qdmask         = EventSpect.qdmask;
				for (int i=0; i<nchan; i++)
				{
					energyHG[i] = EventSpect.energyHG[i];
					energyLG[i] = EventSpect.energyLG[i];
					tstamp[i]   = EventSpect.tstamp[i]  ;
					ToT[i]      = EventSpect.ToT[i]     ;
					// hit is dumped below,for test purposes
					//hit.push_back( ToT[i] );
					//hitname="ToT";
					hit.push_back( energyHG[i] );
					hitname="energyHG";
				}
				hitcols = y_pixel;
				hitrows = x_pixel;
				// dump the scalar ones
				EUDAQ_INFO(prefix+"tstamp_us : "+std::to_string(tstamp_us ));
				EUDAQ_INFO(prefix+"trigger_id: "+std::to_string(trigger_id));
				EUDAQ_INFO(prefix+"chmask    : "+std::to_string(chmask    ));
				EUDAQ_INFO(prefix+"qdmask    : "+std::to_string(qdmask    ));
				ascii += std::to_string(tstamp_us);
				break;
			case DTQ_TEST:
				EUDAQ_WARN(prefix+"Trying to decode TEST event");
				EventTest = FERSunpack_testevent(&data);
				tstamp_us = EventTest.tstamp_us;
				trigger_id = EventTest.trigger_id;
				nwords = EventTest.nwords;
				for (int i=0; i<MAX_TEST_NWORDS; i++)
				{
					test_data[i] = EventTest.test_data[i];
					// hit is dumped below,for test purposes
					hit.push_back( test_data[i] );
					hitname="test_data";
				}
				hitcols = MAX_TEST_NWORDS;
				hitrows = 1;
				// dump the scalar ones
				EUDAQ_INFO(prefix+"tstamp_us : "+std::to_string(tstamp_us ));
				EUDAQ_INFO(prefix+"trigger_id : "+std::to_string(trigger_id ));
				EUDAQ_INFO(prefix+"nwords : "+std::to_string(nwords ));
				ascii += std::to_string(tstamp_us);
				break;
			case DTQ_STAIRCASE:
				EUDAQ_WARN(prefix+"Trying to decode STAIRCASE event");
				std::vector<uint8_t> data1(data.end() - sizeof(StaircaseEvent_t), data.end());
				StaircaseEvent = FERSunpack_staircaseevent(&data1);

				float rateHz = (float)StaircaseEvent.chmean / StaircaseEvent.dwell_time * 1000;
				float HV = (float)StaircaseEvent.HV / 1000; // Volt

				EUDAQ_INFO(prefix+"threshold : "+std::to_string(StaircaseEvent.threshold ));
				EUDAQ_INFO(prefix+"chmean / dwell_time : "+std::to_string(rateHz)+" Hz");

				//EUDAQ_INFO("dwell_time ms: "+std::to_string(StaircaseEvent.dwell_time ));
				//EUDAQ_INFO("chmean    : "+std::to_string(StaircaseEvent.chmean ));
				//EUDAQ_INFO("shapingt  : "+std::to_string(StaircaseEvent.shapingt ));
				//EUDAQ_INFO("HV        : "+std::to_string(HV);
				//EUDAQ_INFO("Tor_cnt   : "+std::to_string(StaircaseEvent.Tor_cnt ));
				//EUDAQ_INFO("Qor_cnt   : "+std::to_string(StaircaseEvent.Qor_cnt ));
				for (int i=0; i<FERSLIB_MAX_NCH; i++)
				{
					hitcnt[i] = StaircaseEvent.hitcnt[i];
					// hit is dumped below,for test purposes
					hit.push_back( hitcnt[i] );
					hitname="hitcnt";
				}
				hitcols = y_pixel;
				hitrows = x_pixel;
				EUDAQ_WARN(prefix+"*** *** PLEASE STOP THE RUN *** ***");
				ascii += std::to_string(rateHz);
		}


		// dumping of hit
		EUDAQ_INFO(prefix+hitname);
		for(size_t row = 0; row < hitrows; ++row) {
			printme="";
			for(size_t col = 0; col < hitcols; ++col){
				printme += std::to_string(hit[col+row*x_pixel]) +" ";
				ascii += ","+std::to_string(hit[col+row*x_pixel]);
			};
			EUDAQ_INFO(prefix+printme);
			// also in ascii file
			writeasciistream(brd, ascii);
		}

	}
	EUDAQ_INFO("Monitor > ---------- end dumping");


	if(m_en_std_converter){
		auto stdev = std::dynamic_pointer_cast<eudaq::StandardEvent>(ev);
		if(!stdev){
			stdev = eudaq::StandardEvent::MakeShared();
			eudaq::StdEventConverter::Convert(ev, stdev, nullptr); //no conf
			//Converting(ev, stdev);
			EUDAQ_INFO("Monitor > event received!");
			if(m_en_std_print){
				stdev->Print(std::cout);

			}
		}
	}
}




bool FERSEventConverter::Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const{
	int brd; // current board
  auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(d1);
  size_t nblocks= ev->NumBlocks();
  auto block_n_list = ev->GetBlockNumList();
  for(auto &block_n: block_n_list){
    std::vector<uint8_t> block = ev->GetBlock(block_n);
    uint8_t dataq;
    //int index = read_header(&block, &x_pixel, &y_pixel, &dataq);
    int index = read_header(&block, &brd, &dataq);

    int expected_size = ( *( event_lengths.find(dataq) ) ).second;
    if(block.size() < index + expected_size) // map event_lengths is defined in FERS_EUDAQ.h
      EUDAQ_THROW("Unknown data");


    std::vector<uint8_t> data(block.begin()+index, block.end());

    int x_pixel = int(sqrt(shmp->nchannels[brd]));
    int y_pixel = int(sqrt(shmp->nchannels[brd]));
    nchan = shmp->nchannels[brd];

    // translate arrays of d1 as planes in d2, and ignore scalar quantities (not ideal, I know)
    uint16_t col;
    uint16_t row;
    eudaq::StandardPlane energyHGplane  (block_n, "energyHGplane  ", "energyHGplane  ");
    eudaq::StandardPlane energyLGplane  (block_n, "energyLGplane  ", "energyLGplane  ");
    eudaq::StandardPlane channelplane   (block_n, "channelplane   ", "channelplane   ");
    eudaq::StandardPlane tstampplane    (block_n, "tstampplane    ", "tstampplane    ");
    eudaq::StandardPlane ToTplane       (block_n, "ToTplane       ", "ToTplane       ");
    eudaq::StandardPlane countsplane    (block_n, "countsplane    ", "countsplane    ");
    eudaq::StandardPlane wave_hgplane   (block_n, "wave_hgplane   ", "wave_hgplane   ");
    eudaq::StandardPlane wave_lgplane   (block_n, "wave_lgplane   ", "wave_lgplane   ");
    eudaq::StandardPlane dig_probesplane(block_n, "dig_probesplane", "dig_probesplane");
    eudaq::StandardPlane test_dataplane (block_n, "test_dataplane ", "test_dataplane ");
    energyHGplane  .SetSizeZS( x_pixel, y_pixel, 0);
    energyLGplane  .SetSizeZS( x_pixel, y_pixel, 0);
    channelplane   .SetSizeZS( x_pixel, y_pixel, 0);
    tstampplane    .SetSizeZS( x_pixel, y_pixel, 0);
    ToTplane       .SetSizeZS( x_pixel, y_pixel, 0);
    countsplane    .SetSizeZS( x_pixel, y_pixel, 0);
    wave_hgplane   .SetSizeZS( MAX_WAVEFORM_LENGTH, 1, 0);
    wave_lgplane   .SetSizeZS( MAX_WAVEFORM_LENGTH, 1, 0);
    dig_probesplane.SetSizeZS( MAX_WAVEFORM_LENGTH, 1, 0);
    test_dataplane .SetSizeZS( MAX_TEST_NWORDS, 1, 0);


    EUDAQ_WARN("CONVERTING "+std::to_string(dataq));
    switch ( dataq ) {
	    case DTQ_SPECT:
		    // fill the relevant vars
		    EventSpect = FERSunpack_spectevent(&data);
		    //tstamp_us      = EventSpect.tstamp_us ;
		    //trigger_id     = EventSpect.trigger_id;
		    //chmask         = EventSpect.chmask    ;
		    //qdmask         = EventSpect.qdmask    ;
		    for (int i=0; i<nchan; i++)
		    {
			    //energyHG[i] = EventSpect.energyHG[i];
			    //energyLG[i] = EventSpect.energyLG[i];
			    col = i % x_pixel;
			    row = int( i / x_pixel );
			    energyHGplane.PushPixel(col,row,EventSpect.energyHG[i]);
			    energyLGplane.PushPixel(col,row,EventSpect.energyLG[i]);
		    }
		    d2->AddPlane(energyHGplane);
		    d2->AddPlane(energyLGplane);
		    break;
	    case DTQ_TIMING:
		    EventList = FERSunpack_listevent(&data);
		    //nhits = EventList.nhits;
		    for (int i=0; i<MAX_LIST_SIZE; i++)
		    {
			    col = i % x_pixel;
			    row = int( i / x_pixel );
			    //channel[i] = EventList.channel[i];
			    //tstamp[i] = EventList.tstamp[i];
			    //ToT[i]  = EventList.ToT[i];
			    channelplane.PushPixel(col, row,EventList.channel[i]);
			    tstampplane .PushPixel(col, row,EventList.tstamp[i]);
			    ToTplane    .PushPixel(col, row,EventList.ToT[i]);
		    }
		    d2->AddPlane(channelplane);
		    d2->AddPlane(tstampplane );
		    d2->AddPlane(ToTplane    );
		    break;
	    case DTQ_COUNT:
		    EventCount = FERSunpack_countevent(&data);
		    //tstamp_us   = EventCount.tstamp_us;
		    //trigger_id  = EventCount.trigger_id;
		    //chmask      = EventCount.chmask;
		    for (int i=0; i<nchan; i++)
		    {
			    col = i % x_pixel;
			    row = int( i / x_pixel );
			    //counts[i]   = EventCount.counts[i];
			    countsplane.PushPixel(col,row,EventCount.counts[i]);
		    }
		    //t_or_counts = EventCount.t_or_counts;
		    //q_or_counts = EventCount.q_or_counts;
		    d2->AddPlane(countsplane);
		    break;
	    case DTQ_WAVE:
		    EventWave = FERSunpack_waveevent(&data);
		    //tstamp_us = EventWave.tstamp_us;
		    //trigger_id= EventWave.trigger_id;
		    //ns        = EventWave.ns;
		    for (int i=0; i<MAX_WAVEFORM_LENGTH; i++)
		    {
			    //wave_hg[i]= EventWave.wave_hg[i];
			    //wave_lg[i]= EventWave.wave_lg[i];
			    //dig_probes[i] = EventWave.dig_probes[i];
			    wave_hgplane   .PushPixel(i,0,EventWave.wave_hg[i]);
			    wave_lgplane   .PushPixel(i,0,EventWave.wave_lg[i]);
			    dig_probesplane.PushPixel(i,0,EventWave.dig_probes[i]);
		    }
		    d2->AddPlane(wave_hgplane   );
		    d2->AddPlane(wave_lgplane   );
		    d2->AddPlane(dig_probesplane);
		    break;
	    case DTQ_TSPECT:
		    EventSpect = FERSunpack_tspectevent(&data);
		    //tstamp_us      = EventSpect.tstamp_us;
		    //trigger_id     = EventSpect.trigger_id;
		    //chmask         = EventSpect.chmask;
		    //qdmask         = EventSpect.qdmask;
		    for (int i=0; i<nchan; i++)
		    {
			    col = i % x_pixel;
			    row = int( i / x_pixel );
			    //energyHG[i] = EventSpect.energyHG[i];
			    //energyLG[i] = EventSpect.energyLG[i];
			    //tstamp[i]   = EventSpect.tstamp[i]  ;
			    //ToT[i]      = EventSpect.ToT[i]     ;
			    energyHGplane.PushPixel(col,row,EventSpect.energyHG[i]);
			    energyLGplane.PushPixel(col,row,EventSpect.energyLG[i]);
			    tstampplane  .PushPixel(col,row,EventSpect.tstamp[i]);
			    ToTplane     .PushPixel(col,row,EventSpect.ToT[i]);
		    }
		    d2->AddPlane(energyHGplane);
		    d2->AddPlane(energyLGplane);
		    d2->AddPlane(tstampplane  );
		    d2->AddPlane(ToTplane     );
		    break;
	    case DTQ_TEST:
		    EventTest = FERSunpack_testevent(&data);
		    //tstamp_us = EventTest.tstamp_us;
		    //trigger_id = EventTest.trigger_id;
		    //nwords = EventTest.nwords;
		    for (int i=0; i<MAX_TEST_NWORDS; i++)
		    {
			    //test_data[i] = EventTest.test_data[i];
			    test_dataplane.PushPixel(i,0,EventTest.test_data[i]);
		    }
		    d2->AddPlane(test_dataplane);
		    break;
	    case DTQ_STAIRCASE:
		    std::vector<uint8_t> data1(data.end() - sizeof(StaircaseEvent_t), data.end());
		    StaircaseEvent = FERSunpack_staircaseevent(&data1);

		    //float rateHz = (float)StaircaseEvent.chmean / StaircaseEvent.dwell_time * 1000;
		    //float HV = (float)StaircaseEvent.HV / 1000; // Volt

		    //EUDAQ_INFO("threshold : "+std::to_string(StaircaseEvent.threshold ));
		    //EUDAQ_INFO("chmean / dwell_time : "+std::to_string(rateHz)+" Hz");

		    //EUDAQ_INFO("dwell_time ms: "+std::to_string(StaircaseEvent.dwell_time ));
		    //EUDAQ_INFO("chmean    : "+std::to_string(StaircaseEvent.chmean ));
		    //EUDAQ_INFO("shapingt  : "+std::to_string(StaircaseEvent.shapingt ));
		    //EUDAQ_INFO("HV        : "+std::to_string(HV);
		    //EUDAQ_INFO("Tor_cnt   : "+std::to_string(StaircaseEvent.Tor_cnt ));
		    //EUDAQ_INFO("Qor_cnt   : "+std::to_string(StaircaseEvent.Qor_cnt ));
		    for (int i=0; i<FERSLIB_MAX_NCH; i++)
		    {
			    col = i % x_pixel;
			    row = int( i / x_pixel );
			    //hitcnt[i] = StaircaseEvent.hitcnt[i];
			    countsplane.PushPixel(col,row,StaircaseEvent.hitcnt[i]);
		    }
		    d2->AddPlane(countsplane);
    }



  }
  return true;
}
