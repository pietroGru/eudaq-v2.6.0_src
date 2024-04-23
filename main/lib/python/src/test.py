    
def replace_patterns(self, text: str, runNb: int, fileformat: str = ".root"):
    """
    Replace the patterns in the text with the corresponding values
    """
    
    # Replace the output file format
    text = text.replace('$X', fileformat)
        
    # Parse the remaining date and run formats
    flag=False
    digits = {'D': '', 'R': ''}
    for idx, c in enumerate(text):
        if c == '$':
            flag = True
            startIdx = idx
        elif flag and c in ['D', 'R']:
            flag = False
            stopIdx = idx+1
            slStr = text[startIdx:stopIdx]
            if slStr[1:-1].isnumeric(): digits[c] = slStr
            
    # Format the run number
    if digits['R'] != '':
        runNbDigits = int(digits['R'][1:-1])            
        text = text.replace(digits['R'], str(runNb).zfill(runNbDigits))

    # Remove the date
    text = text.replace(digits['D'], '')
    return text

strTesxt = "luxe$12D_Run$5R$X" 
print(strTesxt)
print(replace_patterns('', strTesxt, 12, ".root"))



import numpy as np

a1561hdp_dtypes = [('run',np.uint32),('event',np.uint32),('timestamp',np.float64),('s0',np.bool_),('c0V',np.float64),('c0I',np.float64),('s1',np.bool_),('c1V',np.float64),('c1I',np.float64),('kill',np.bool_)]
basler_dtypes = [('event',np.uint32),('timestamp',np.float64),('gain',np.float64),('exposure',np.float64),('px_num_hor',np.float64),('profile_mean_hor',np.float64),('mean_se_hor',np.float64),('profile_sigma_hor',np.float64),('sigma_se_hor',np.float64),('profile_amp_hor',np.float64),('amp_se_hor',np.float64),('chi_sqr_hor',np.float64),('r_sqr_hor',np.float64),('good_hor_fit_flag',np.bool_),('px_num_ver',np.float64),('profile_mean_ver',np.float64),('mean_se_ver',np.float64),('profile_sigma_ver',np.float64),('sigma_se_ver',np.float64),('profile_amp_ver',np.float64),('amp_se_ver',np.float64),('chi_sqr_ver',np.float64),('r_sqr_ver',np.float64),('good_ver_fit_flag',np.bool_)]
clearF_dtypes = [('event',np.uint32),('timestamp',np.float64),('beamQ',np.float64),('x',np.float64),('y',np.float64)]
dt5730_dtypes = [('run',np.uint32),('runTime',np.float64),('event',np.uint32),('timestamp',np.float64),('dgt_evt',np.uint32),('dgt_trgtime',np.uint64),('dgt_evtsize',np.uint32),('avg',np.float64),('std',np.float64),('ptNb',np.uint32),('avgV',np.float64),('stdV',np.float64),('avgQ',np.float64)]
eudaq_dtypes = [('run',np.uint32),('runTime',np.float64),('event',np.uint32),('timestamp',np.float64)]
fersCa_dtypes = [('run', np.uint32),('runTime', np.float64),('event', np.uint32),('timestamp', np.float64),('fers_evt', np.uint32),('fers_trgtime', np.float64),('fers_ch0', np.uint32),('fers_ch1', np.uint32),('strip0', np.uint32),('strip1', np.uint32),('lg0', np.int32),('hg0', np.int32),('lg1', np.int32),('hg1', np.int32)]

dtypes = {
    0: (eudaq_dtypes, 'eudaq'),
    1: (a1561hdp_dtypes, 'a1561hdp'),
    2: (basler_dtypes, 'basler'),
    3: (clearF_dtypes, 'clearF'),
    4: (dt5730_dtypes, 'dt5730'),
    5: (fersCa_dtypes, 'fersCa')
}

data = {}
# Get the list of subevents
a1561hdp = np.zeros(1, dtype=dtypes[1])
print("len is ", len(a1561hdp.tobytes()))
print(a1561hdp)
for key in deviceDtype:
    data[deviceName+key] = devData[key]


print(data)

