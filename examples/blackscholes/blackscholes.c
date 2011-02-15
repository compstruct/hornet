// -----------------------------------------------------------------------------
// Notes/Imports

/*
	Notes:
	- 'NOTE: Removed' indicates that a line of code was commented out for 
										compatability reasons

	Any significant change to the benchmark is wrapped in a
	--begin HORNET
	...
	--end HORNET 
	clause (commented out)

	TODO
	1.) check and remove extra __H_fflush(); calls
	2.) make sure __H_enable_memory_hierarchy is called

	CHANGES:
	1.) library function call names
	2.) print statements
	3.) calls to __H_read_line have temporary space allocated (see bug list in mcpu.hpp).
	4.) P/C in the input files changed to 1 0 to satisfy an INT alignment bug
*/

#ifdef LINKING
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#else
#include "rts.h"
#endif

//#define LINKING

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Black Scholes main algorithm

// Copyright (c) 2007 Intel Corp.

// Black-Scholes
// Analytical method for calculating European Options
//
// 
// Reference Source: Options, Futures, and Other Derivatives, 3rd Edition, Prentice 
// Hall, John C. Hull,

#define ENABLE_THREADS_HORNET
//#define ENABLE_THREADS
//#define WIN32

#ifdef ENABLE_PARSEC_HOOKS
#include <hooks.h>
#endif

// Multi-threaded pthreads header
#ifdef ENABLE_THREADS
#define MAX_THREADS 1024
// Add the following line so that icc 9.0 is compatible with pthread lib.
#define __thread __threadp  
MAIN_ENV
#undef __thread
BARDEC(barrier);
#endif

// Multi-threaded OpenMP header
#ifdef ENABLE_OPENMP
#include <omp.h>
#endif

// Multi-threaded header for Windows
#ifdef WIN32
#pragma warning(disable : 4305)
#pragma warning(disable : 4244)
//#include <windows.h>
#define MAX_THREADS 1024
#endif

//Precision to use for calculations
// TODO: find a way to parameterize these to the commented out fragments
#define FP_PRECISION 					// if this line is commented out, assume double
#define fptype 		float				//((FP_PRECISION == 32) ? float 			: double)
#define __H_sqrt	__H_sqrt_s	//((FP_PRECISION == 32) ? __H_sqrt_s 	: __H_sqrt_d) 
#define __H_log		__H_log_s		//((FP_PRECISION == 32) ? __H_log_s 	: __H_log_d)
#define __H_exp		__H_exp_s		//((FP_PRECISION == 32) ? __H_exp_s 	: __H_exp_d)
#define print_fp print_float  // or print_double
#define NUM_RUNS 100

// HORNET: DO NOT CHANGE THE ORDER OF THE FIELDS IN THIS STRUCT!
// (doing so will break the __H_read_line call, unless you also change setup.py)
typedef struct OptionData_ {
        fptype s;          // spot price
        fptype strike;     // strike price
        fptype r;          // risk-free interest rate
        fptype divq;       // dividend rate
        fptype v;          // volatility
        fptype t;          // time to maturity or option expiration in years 
                           //     (1yr = 1.0, 6mos = 0.5, 3mos = 0.25, ..., etc)  
        int OptionType;    // Option type.  "P"=PUT, "C"=CALL
													 // HORNET: this was char OptionType.  Changed to int
													 // OptionType because of word alignment.
        fptype divs;       // dividend vals (not used in this test)
        fptype DGrefval;   // DerivaGem Reference Value
} OptionData;

int		 * 	__H_MUTEX_BARRIER_START = 	0x003ffffc;
int		 * 	__H_MUTEX_BARRIER_FINISH = 	0x003ffff8;

int		 ** __PROXY_numOptions = 				0x003ffff4;
fptype ** __PROXY_prices = 						0x003ffff0;
int 	 ** __PROXY_otype = 						0x003fffec;
fptype ** __PROXY_sptprice = 					0x003fffe8;
fptype ** __PROXY_strike = 						0x003fffe4; 
fptype ** __PROXY_rate = 							0x003fffe0;
fptype ** __PROXY_volatility = 				0x003fffdc;
fptype ** __PROXY_otime = 						0x003fffd8;

//int numError = 0;

int nThreads;
int nThreadsMask;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Cumulative Normal Distribution Function
// See Hull, Section 11.8, P.243-244
#define inv_sqrt_2xPI 0.39894228040143270286

fptype CNDF ( fptype InputX ) 
{
    int sign;

    fptype OutputX;
    fptype xInput;
    fptype xNPrimeofX;
    fptype expValues;
    fptype xK2;
    fptype xK2_2, xK2_3;
    fptype xK2_4, xK2_5;
    fptype xLocal, xLocal_1;
    fptype xLocal_2, xLocal_3;

    // Check for negative value of InputX
    if (InputX < 0.0) {
        InputX = -InputX;
        sign = 1;
    } else 
        sign = 0;

    xInput = InputX;
 
    // Compute NPrimeX term common to both four & six decimal accuracy calcs
    expValues = __H_exp(-0.5f * InputX * InputX);
    xNPrimeofX = expValues;
    xNPrimeofX = xNPrimeofX * inv_sqrt_2xPI;

    xK2 = 0.2316419 * xInput;
    xK2 = 1.0 + xK2;
    xK2 = 1.0 / xK2;
    xK2_2 = xK2 * xK2;
    xK2_3 = xK2_2 * xK2;
    xK2_4 = xK2_3 * xK2;
    xK2_5 = xK2_4 * xK2;
    
    xLocal_1 = xK2 * 0.319381530;
    xLocal_2 = xK2_2 * (-0.356563782);
    xLocal_3 = xK2_3 * 1.781477937;
    xLocal_2 = xLocal_2 + xLocal_3;
    xLocal_3 = xK2_4 * (-1.821255978);
    xLocal_2 = xLocal_2 + xLocal_3;
    xLocal_3 = xK2_5 * 1.330274429;
    xLocal_2 = xLocal_2 + xLocal_3;

    xLocal_1 = xLocal_2 + xLocal_1;
    xLocal   = xLocal_1 * xNPrimeofX;
    xLocal   = 1.0 - xLocal;

    OutputX  = xLocal;
    
    if (sign) {
        OutputX = 1.0 - OutputX;
    }
    
    return OutputX;
} 

// For debugging
void print_xmm(fptype in, char* s) {
    __H_printf("%s: %f\n", s, in);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
fptype BlkSchlsEqEuroNoDiv( fptype sptprice,
                            fptype strike, fptype rate, fptype volatility,
                            fptype time, int otype, float timet )
{
    fptype OptionPrice;

    // local private working variables for the calculation
    fptype xStockPrice;
    fptype xStrikePrice;
    fptype xRiskFreeRate;
    fptype xVolatility;
    fptype xTime;
    fptype xSqrtTime;

    fptype logValues;
    fptype xLogTerm;
    fptype xD1; 
    fptype xD2;
    fptype xPowerTerm;
    fptype xDen;
    fptype d1;
    fptype d2;
    fptype FutureValueX;
    fptype NofXd1;
    fptype NofXd2;
    fptype NegNofXd1;
    fptype NegNofXd2;    
    
    xStockPrice = sptprice;
    xStrikePrice = strike;
    xRiskFreeRate = rate;
    xVolatility = volatility;

    xTime = time;
    xSqrtTime = __H_sqrt(xTime);

    logValues = __H_log( sptprice / strike );
        
    xLogTerm = logValues;
    

    xPowerTerm = xVolatility * xVolatility;
    xPowerTerm = xPowerTerm * 0.5;

    xD1 = xRiskFreeRate + xPowerTerm;
    xD1 = xD1 * xTime;
    xD1 = xD1 + xLogTerm;

    xDen = xVolatility * xSqrtTime;
    xD1 = xD1 / xDen;
    xD2 = xD1 -  xDen;

    d1 = xD1;
    d2 = xD2;

    NofXd1 = CNDF( d1 );
    NofXd2 = CNDF( d2 );

    FutureValueX = strike * ( __H_exp( -(rate)*(time) ) );        
    if (otype == 0) {            
        OptionPrice = (sptprice * NofXd1) - (FutureValueX * NofXd2);
    } else { 
        NegNofXd1 = (1.0 - NofXd1);
        NegNofXd2 = (1.0 - NofXd2);
        OptionPrice = (FutureValueX * NegNofXd2) - (sptprice * NegNofXd1);
    }
    
    return OptionPrice;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#ifdef WIN32
DWORD WINAPI bs_thread(LPVOID tid_ptr){
#else
int bs_thread(void *tid_ptr) {
#endif
#ifdef ENABLE_THREADS
    BARRIER(barrier);
#endif
#ifdef ENABLE_THREADS_HORNET
		while (!__H_ucLoadWord(__H_MUTEX_BARRIER_START)) { /* spin */ };
#endif
    int i, j;
    fptype price;
    fptype priceDelta;
		int numOptions = **__PROXY_numOptions;
    int tid = *(int *)tid_ptr;

		fptype * prices = *__PROXY_prices;
		int    * otype = *__PROXY_otype;
		fptype * sptprice = *__PROXY_sptprice;
		fptype * strike = *__PROXY_strike;
		fptype * rate = *__PROXY_rate;
		fptype * volatility = *__PROXY_volatility;
		fptype * otime = *__PROXY_otime;

    int start = tid * (numOptions / nThreads);
    int end = start + (numOptions / nThreads);

    print_string("tid=");
		print_int(tid);
		print_string(" go (start: ");
		print_int(start);
		print_string(", end: ");
		print_int(end);
		print_string(")\n");

    for (j=0; j<NUM_RUNS; j++) {
			print_string("tid=");
			print_int(tid);
			print_string(", runs=");
			print_int(j);
			print_string("\n");
			__H_fflush();
#ifdef ENABLE_OPENMP
			#pragma omp parallel for
      for (i=0; i<numOptions; i++) {
#else  //ENABLE_OPENMP
			for (i=start; i<end; i++) {
#endif //ENABLE_OPENMP
		    // Calling main function to calculate option value based on Black & 
				// Sholes's equation.
				
		    price = BlkSchlsEqEuroNoDiv( sptprice[i], strike[i],
		                                 rate[i], volatility[i], otime[i], 
		                                 otype[i], 0);
		    prices[i] = price;
				
				print_string("\nBlkSchlsEqEuroNoDiv loop[");
				print_int(j);
				print_string("][");
				print_int(i);
				print_string("]:\n");
				__H_fflush();

				/*print_fp(sptprice[i]);
				print_string("\n");
				print_fp(strike[i]);
				print_string("\n");
				print_fp(rate[i]);
				print_string("\n");
				print_fp(volatility[i]);
				print_string("\n");				
				print_fp(otime[i]);
				print_string("\n");
				print_int(otype[i]);
				print_string("\n");
				__H_fflush();*/

				print_string("Price: ");
				print_float(price);
				print_string("\n");
        __H_fflush();
    
#ifdef ERR_CHK
				__H_exit(51); // data no longer has global scope, and DGrefval isn't being properly shared
        priceDelta = data[i].DGrefval - price;
        if( fabs(priceDelta) >= 1e-4 ) {
           	print_string("Error on ");
						print_int(i);
						print_string(", Computed=");
						print_fp(price);
						print_string(", Ref=");
						print_fp(data[i].DGrefval);
						print_string(", Delta=");
						print_fp(priceDelta);
						print_string("\n");
            numError++;
        }
#endif
        }
    }
#ifdef ENABLE_THREADS
    print_string("tid=");
		print_int(tid);
		print_string(" done\n");
    BARRIER(barrier);
#else//ENABLE_THREADS
#ifdef ENABLE_THREADS_HORNET
		__H_ucSetBit(__H_MUTEX_BARRIER_FINISH, tid);
#endif//ENABLE_THREADS_HORNET
#endif//ENABLE_THREADS

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	unsigned id = cpu_id();

#ifdef ENABLE_THREADS_HORNET
	nThreads = 4;
	nThreadsMask = 0xF; // 4 1s in lsb positions -- TODO: turn into a function
#else
	nThreads = 1;
	nThreadsMask = 0x1;
#endif

#ifdef ENABLE_THREADS_HORNET
	if (id == 0) { // only intialize with the master thread
#endif
		int numOptions;
		OptionData *data;
		int file;   
		int i;
    int loopnum;
    fptype * buffer;
    int * buffer2;
    int rv;

		fptype * prices;
		int    * otype;
		fptype * sptprice;
		fptype * strike;
		fptype * rate;
		fptype * volatility;
		fptype * otime;

#ifdef PARSEC_VERSION
#define __PARSEC_STRING(x) #x
#define __PARSEC_XSTRING(x) __PARSEC_STRING(x)
	__H_printf("PARSEC Benchmark Suite Version "__PARSEC_XSTRING(PARSEC_VERSION)"\n");
	__H_fflush(NULL);
#else
	__H_printf("PARSEC Benchmark Suite\n");
	__H_fflush(NULL);
#endif
#ifdef ENABLE_PARSEC_HOOKS
	__parsec_bench_begin(__parsec_blackscholes);
#endif

 	char * inputFile = "in_4K.bin";
	char * outputFile = "results";

	//Read input data from file
	file = __H_fopen(inputFile);
	if (file == 0) { // was: NULL
		print_string("\nERROR: Unable to read from file: ");
		print_string(inputFile);
		print_string("\n");
		__H_exit(1);
	}

	int tmp;
	rv = __H_read_line(file, (char *) &tmp, 4);
	numOptions = tmp;
	*__PROXY_numOptions = (int *) malloc(sizeof(int));
	**__PROXY_numOptions = tmp;

	if(rv != 4) {
		print_string("\nERROR: Couldn't read numOptions count in file: ");
		print_string(inputFile);
		print_string("\n");	
		__H_fclose(file);
		__H_exit(1);		
	}
	if(nThreads > numOptions) {
		print_string("\nWARNING: Not enough work, reducing number of threads to match number of options.\n");
		nThreads = numOptions;
	}

#if !defined(ENABLE_THREADS) && !defined(ENABLE_OPENMP) && !defined(ENABLE_THREADS_HORNET)
	if(nThreads != 1) {
		print_string("Error: <nthreads> must be 1 (serial version)\n");
		__H_exit(1);
	}
#endif

	// alloc spaces for the option data
#ifdef FP_PRECISION
	int epl = 36;
#else
	int epl = 68;
#endif

	OptionData tmp_OptionData;
	data = (OptionData*) malloc(numOptions*sizeof(OptionData));
	prices = (fptype*) malloc(numOptions*sizeof(fptype));
	*__PROXY_prices = prices;
	for ( loopnum = 0; loopnum < numOptions; ++ loopnum ) {
		  rv = __H_read_line(file, (char *) &tmp_OptionData, epl);

			data[loopnum].s = tmp_OptionData.s;
			data[loopnum].strike = tmp_OptionData.strike;
			data[loopnum].r = tmp_OptionData.r;
			data[loopnum].divq = tmp_OptionData.divq;
			data[loopnum].v = tmp_OptionData.v;
			data[loopnum].t = tmp_OptionData.t;
			data[loopnum].OptionType = tmp_OptionData.OptionType;
			data[loopnum].divs = tmp_OptionData.divs;
			data[loopnum].DGrefval = tmp_OptionData.DGrefval;

			print_string("Reading line: ");
			print_int(loopnum);
			print_string("\n");
			__H_fflush();

			/*print_string("\ns: ");
			print_fp(data[loopnum].s);
			print_string("\n");

			print_string("strike: ");
			print_fp(data[loopnum].strike);
			print_string("\n");

			print_string("r: ");
			print_fp(data[loopnum].r);
			print_string("\n");

			print_string("divq: ");
			print_fp(data[loopnum].divq);
			print_string("\n");

			print_string("v: ");
			print_fp(data[loopnum].v);
			print_string("\n");

			print_string("t: ");
			print_fp(data[loopnum].t);
			print_string("\n");

			print_string("OptionType: ");
			print_int(data[loopnum].OptionType);
			print_string("\n");

			print_string("divs: ");
			print_fp(data[loopnum].divs);
			print_string("\n");

			print_string("DGrefval: ");
			print_fp(data[loopnum].DGrefval);
			print_string("\n\n");
			__H_fflush();*/

		  if (rv != epl) {
		    print_string("ERROR: Wrong byte count on line ");
				print_int(loopnum);
				print_string(" ~ expected: ");
				print_int(epl);
				print_string(", actual: ");
				print_int(rv);
				print_string("\n");
		    __H_exit(1);
		  }
	}
	rv = __H_fclose(file);
	if(rv != 0) {
		print_string("ERROR: Unable to close file.");
		__H_exit(1);
	}

#ifdef ENABLE_THREADS
    MAIN_INITENV(,8000000,nThreads);
    BARINIT(barrier, nThreads);
#endif

	print_string("Num of Options: ");
	print_int(numOptions);
	print_string("\n");
	__H_fflush();

	print_string("Num of Runs: ");
	print_int(NUM_RUNS);
	print_string("\n");
	__H_fflush();

#define PAD 256
#define LINESIZE 64

    buffer = (fptype *) malloc(5 * numOptions * sizeof(fptype) + PAD);
    sptprice = (fptype *) (((unsigned long long)buffer + PAD) & ~(LINESIZE - 1));
    *__PROXY_sptprice = sptprice;
		strike = sptprice + numOptions;
		*__PROXY_strike = strike;
    rate = strike + numOptions;
		*__PROXY_rate = rate;
    volatility = rate + numOptions;
		*__PROXY_volatility = volatility;
    otime = volatility + numOptions;
		*__PROXY_otime = otime;

    buffer2 = (int *) malloc(numOptions * sizeof(fptype) + PAD);
    otype = (int *) (((unsigned long long)buffer2 + PAD) & ~(LINESIZE - 1));
		*__PROXY_otype = otype;

    for (i=0; i<numOptions; i++) { // TODO: collapse this into the data[] to speedup initialization time
        otype[i]      = (data[i].OptionType) ? 1 : 0; // --begin HORNET used to be " == 'P'..." 
        sptprice[i]   = data[i].s;
        strike[i]     = data[i].strike;
        rate[i]       = data[i].r;
        volatility[i] = data[i].v;    
        otime[i]      = data[i].t;

			print_string("Copying line: ");
			print_int(i);
			print_string("\n");
			__H_fflush();
    }

    print_string("Size of data: ");
		print_int((int) numOptions * (sizeof(OptionData) + sizeof(int)));
		print_string("\n");
		__H_fflush();

#ifdef ENABLE_THREADS_HORNET
	}
#endif

// -----------------------------------------------------------------------------
//														 Threads fork
// -----------------------------------------------------------------------------

	__H_enable_memory_hierarchy(); // Initialization is done!

#ifdef ENABLE_PARSEC_HOOKS
    __parsec_roi_begin();
#endif

#ifdef ENABLE_THREADS
    int tids[nThreads];
    for(i=0; i<nThreads; i++) {
        tids[i]=i;
        CREATE_WITH_ARG(bs_thread, &tids[i]);
    }
    __H_printf("%d threads spawned", i);
    WAIT_FOR_END(nThreads);
#else//ENABLE_THREADS
#ifdef ENABLE_THREADS_HORNET
		if (id == 0) {
			__H_ucSetBit(__H_MUTEX_BARRIER_START, 0);
		}
		int tid=id;
		bs_thread(&tid);
		while (__H_ucLoadWord(__H_MUTEX_BARRIER_FINISH) != nThreadsMask) {/*spin*/};
#else//ENABLE_THREADS_HORNET
#ifdef ENABLE_OPENMP
    {
        int tid=0;
        omp_set_num_threads(nThreads);
        bs_thread(&tid);
    }
#else //ENABLE_OPENMP
#ifdef WIN32 
	if (nThreads > 1)
	{
		  HANDLE threads[MAX_THREADS];
		          int nums[MAX_THREADS];
		          for(i=0; i<nThreads; i++) {
		                  nums[i] = i;
		                  threads[i] = CreateThread(0, 0, bs_thread, &nums[i], 0, 0);
		          }
		          WaitForMultipleObjects(nThreads, threads, TRUE, INFINITE);
	} else
#endif
	{
		__H_exit(50); // this code segment needs to be updated before being run again

		int tid=0;
		bs_thread(*numOptions_shared, &tid);
	}
#endif //ENABLE_OPENMP
#endif //ENABLE_THREADS_HORNET
#endif //ENABLE_THREADS

#ifdef ENABLE_PARSEC_HOOKS
    __parsec_roi_end();
#endif

// -----------------------------------------------------------------------------
//														 Threads join
// -----------------------------------------------------------------------------

/*

	SKIP THE POST PROCESSING

	//Write prices to output file
	file = __H_fopen(outputFile, "w");
	if(file == NULL) {
		__H_printf("ERROR: Unable to open file `%s'.\n", outputFile);
		__H_exit(1);
	}
	rv = f__H_printf(file, "%i\n", numOptions);
	if(rv < 0) {
		__H_printf("ERROR: Unable to write to file `%s'.\n", outputFile);
		fclose(file);
		__H_exit(1);
	}
	for(i=0; i<numOptions; i++) {
		rv = f__H_printf(file, "%.18f\n", prices[i]);
		if(rv < 0) {
		  __H_printf("ERROR: Unable to write to file `%s'.\n", outputFile);
		  fclose(file);
		  __H_exit(1);
		}
	}
	rv = fclose(file);
	if(rv != 0) {
		__H_printf("ERROR: Unable to close file `%s'.\n", outputFile);
		__H_exit(1);
	}

	#ifdef ERR_CHK
	__H_printf("Num Errors: %d\n", numError);
	#endif
	free(data);
	free(prices);

#ifdef ENABLE_PARSEC_HOOKS
    __parsec_bench_end();
#endif

	*/

	return 0;
}

