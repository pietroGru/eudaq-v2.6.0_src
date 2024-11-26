/////////////////////////////////////////////////////////////////////
//                         2023 May 08                             //
//                   authors: F. Tortorici                         //
//                email: francesco.tortorici@ct.infn.it            //
//                        notes:                                   //
/////////////////////////////////////////////////////////////////////

#ifndef _FERS_EUDAQ_h
#define _FERS_EUDAQ_h

#include <vector>
#include "paramparser.h"
#include <map>
#include "FERSlib.h"

extern std::fstream runfile[MAX_NBRD]; // pointers to ascii output data files

#define DTQ_STAIRCASE 10
// staircase datatype
typedef struct {
	uint16_t threshold;
	uint16_t shapingt; // enum, see FERS_Registers.h
	uint32_t dwell_time; // in seconds, divide hitcnt by this to get rate
	uint32_t chmean; // over channels, no division by time
	uint32_t HV; // 1000 * HV from HV_Get_Vbias( handle, &HV);
	uint32_t Tor_cnt;
	uint32_t Qor_cnt;
	uint32_t hitcnt[FERSLIB_MAX_NCH];
} StaircaseEvent_t;

// known types of event (for event length checks for instance in Monitor)
static const std::map<uint8_t, int> event_lengths =
{
{ DTQ_SPECT     , sizeof(SpectEvent_t)    },
{ DTQ_TIMING    , sizeof(ListEvent_t)     },
{ DTQ_COUNT     , sizeof(CountingEvent_t) },
{ DTQ_WAVE      , sizeof(WaveEvent_t)     },
{ DTQ_TSPECT    , sizeof(SpectEvent_t)    },
{ DTQ_TEST      , sizeof(TestEvent_t)     },
{ DTQ_STAIRCASE , sizeof(StaircaseEvent_t)}
};                                
                                  

struct CLEAR_nametypes{
  uint32_t      run = 0;
  double        runTime = 0.0;
  uint32_t      event = 0;
  uint32_t      fers_evt = 0;
  double        fers_trgtime = 0.0;
  double        timestamp = 0.0;
  double        timestamp_sw = 0.0;
  uint32_t      hold = 0;
  uint32_t      gain[64] = {0};
  uint32_t      fers_ch[64] = {0};
  uint32_t      strip[64] = {0};
  int32_t       lg[64] = {0};
  int32_t       hg[64] = {0};
};


//////////////////////////        
// use this to pack every kind of  event
void FERSpackevent(void* Event, int plane_id, int dataqualifier, std::vector<uint8_t> *vec);
//////////////////////////        
                                  
// for each kind of event: the "pack" is used by FERSpackevent,
// whereas the "unpack" is meant sto be used individually

// basic types of events 
void FERSpack_CLEAR_event(void* Event, int plane_it, int run_number, int event_number, int add_events, double time_begin, std::vector<uint8_t> &vec);

void FERSpack_spectevent(void* Event, std::vector<uint8_t> *vec);
SpectEvent_t FERSunpack_spectevent(std::vector<uint8_t> *vec);

void FERSpack_listevent(void* Event, std::vector<uint8_t> *vec);
ListEvent_t FERSunpack_listevent(std::vector<uint8_t> *vec);

void FERSpack_tspectevent(void* Event, std::vector<uint8_t> *vec);
SpectEvent_t FERSunpack_tspectevent(std::vector<uint8_t> *vec);

void FERSpack_countevent(void* Event, std::vector<uint8_t> *vec);
CountingEvent_t FERSunpack_countevent(std::vector<uint8_t> *vec);

void FERSpack_waveevent(void* Event, std::vector<uint8_t> *vec);
WaveEvent_t FERSunpack_waveevent(std::vector<uint8_t> *vec);

void FERSpack_testevent(void* Event, std::vector<uint8_t> *vec);
TestEvent_t FERSunpack_testevent(std::vector<uint8_t> *vec);

// advanced
void FERSpack_staircaseevent(void* Event, std::vector<uint8_t> *vec);
StaircaseEvent_t FERSunpack_staircaseevent(std::vector<uint8_t> *vec);

/////////////////

// utilities used by the above methods

// fill "data" with some info
//void make_header(int handle, uint8_t x_pixel, uint8_t y_pixel, int DataQualifier, std::vector<uint8_t> *data);
void make_header(int board, int DataQualifier, std::vector<uint8_t> *data);

// reads back essential header info (see params)
// prints them w/ board ID info with EUDAQ_WARN
// returns index at which raw data starts
//int read_header(std::vector<uint8_t> *data, uint8_t *x_pixel, uint8_t *y_pixel, uint8_t *DataQualifier);
int read_header(std::vector<uint8_t> *data, int *board, uint8_t *DataQualifier);

void dump_vec(std::string title, std::vector<uint8_t> *vec, int start=0, int stop=0);

void FERSpack(int nbits, uint32_t input, std::vector<uint8_t> *vec);
uint16_t FERSunpack16(int index, std::vector<uint8_t> vec);
uint32_t FERSunpack32(int index, std::vector<uint8_t> vec);
uint64_t FERSunpack64(int index, std::vector<uint8_t> vec);



#endif