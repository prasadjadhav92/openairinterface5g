%addpath('../../../openair1/SIMULATION/LTE_PHY/')
%addpath('../../../openair1/PHY/LTE_ESTIMATION/')
%addpath('../../../openair1/PHY/LTE_REFSIG/')
%addpath('../../../targets/ARCH/EXMIMO/USERSPACE/OCTAVE')

% load the LTE sync sequence
primary_synch;

%% this generates one LTE frame (10ms) full of OFDM modulated random QPSK symbols

nb_rb = 100; %this can be 25, 50, or 100
num_carriers = 2048/100*nb_rb;
num_zeros = num_carriers-(12*nb_rb+1);
prefix_length = num_carriers/4; %this is extended CP
num_symbols_frame = 120;
preamble_length = 120;

[s1,f1] = OFDM_TX_FRAME(num_carriers,num_zeros,prefix_length,num_symbols_frame,preamble_length);
% scale to conserve energy (Matlabs IFFT does not scale)
s1=s1*sqrt(num_carriers);

% upsample PSS to the right frequency and insert it in the first symbol of the frame

pss0_up = interp(primary_synch0_time,num_carriers/128);
pss0_up_cp = [pss0_up(num_carriers-prefix_length+1:end) pss0_up];

s1(1:num_carriers+prefix_length) = pss0_up_cp;

%% and now the other carrier
nb_rb = 50; %this can be 25, 50, or 100
num_carriers = 2048/100*nb_rb;
num_zeros = num_carriers-(12*nb_rb+1);
prefix_length = num_carriers/4; %this is extended CP

[s2,f2] = OFDM_TX_FRAME(num_carriers,num_zeros,prefix_length,num_symbols_frame,preamble_length);
% scale to conserve energy (Matlabs IFFT does not scale)
s2=s2*sqrt(num_carriers);

% upsample PSS to the right frequency and insert it in the first symbol of the frame

pss0_up = interp(primary_synch0_time,num_carriers/128);
pss0_up_cp = [pss0_up(num_carriers-prefix_length+1:end) pss0_up];

s2(1:num_carriers+prefix_length) = pss0_up_cp;

%% combine the two carriers
f1_shift = -7.5e6;
f2_shift = 7.5e6;
sample_rate = 30.72e6*2;
s1_up = interp(s1,2);
s1_shift = s1_up .* exp(2*1i*pi*f1_shift*(0:length(s1_up)-1)/sample_rate);
s2_up = interp(s2,4);
s2_shift = s2_up .* exp(2*1i*pi*f2_shift*(0:length(s2_up)-1)/sample_rate);
s = s1_shift + s2_shift/sqrt(2);

plot(abs(fftshift(fft(s))))

%% save for later use (channel estimation and transmission with the SMBV)
save('ofdm_pilots_sync_30MHz.mat','-v7','s','f','num_carriers','num_zeros','prefix_length','num_symbols_frame','preamble_length');
mat2wv(s1, 'ofdm_pilots_sync_30MHz.wv', sample_rate, 1);