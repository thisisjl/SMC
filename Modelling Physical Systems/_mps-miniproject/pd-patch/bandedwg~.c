/*
bandedwg~ object to perform ...
based on karplus strong by Stefania Serafin june 99	
*/

#include "m_pd.h"
#include "math.h"

#define BUFSIZE 145530//10000  						// size of the buffer
#define PI 		3.14159265358979323846
#define NMODES  3 									// number of modes

static t_class *bandedwg_class;

typedef struct bandedwg { 
	t_object x_obj;
	//float v_freq;   // frequency of the string, this will determine the length of the delay line
	//float v_fb;		// Input that will start the KS model
	float b_ym1[NMODES];    // variable storing x[n-1];
	float xm1[NMODES],xm2[NMODES];  // x[n-1],x[n-2]
	float ym1[NMODES],ym2[NMODES];	// y[n-1],y[n-2],
	int v_posright[NMODES]; // variable storing the position in the delay line
	float *delayline[NMODES];
	
} t_bandedwg;

// most important function of the object is always the perform function
// it is the one that calculates the DSP loop.

static t_int *bandedwg_perform(t_int *w)
{  
	t_float *fb 	= (t_float *)(w[1]);    // input to the system (noise in the case of original KS)
	t_float *freq 	= (t_float *)(w[2]);  	//frequency of the string 
	t_float *damp 	= (t_float *)(w[3]);    // damping as coefficient of the lowpass filter
	t_float *atten 	= (t_float *)(w[4]);   	// overall attenuation of the system
	t_float *out 	= (t_float *)(w[5]);   	//output. 
	t_bandedwg *x 	= (t_bandedwg *)(w[6]);    
	int n_tick 		= (int)(w[7]);

	
	int i;

	float vin;

	float fs = 44100;

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
	float y[NMODES];
	
	float f0 					= *freq;
	float fb_gain				= 0.995;
	float decaytimes[NMODES]  	= {3.3,2.5,1.7};
	// float B[NMODES] 			= {0.4,0.3,0.1};
	float B[NMODES];
	float fm_ratios[NMODES] 	= {1,1.97,2.97};

	int posr[NMODES];
	t_float mode_out[NMODES];

	float fm[NMODES];
	float xm1[NMODES], xm2[NMODES];
	float ym1[NMODES], ym2[NMODES];
	
	float phi[NMODES], R[NMODES], theta[NMODES], A0[NMODES];		// band pass filter parameters
	int d[NMODES];
	float *delayline[NMODES];
	for(int mode_i=0; mode_i<NMODES; mode_i++)
	{
		fm[mode_i]  = f0 * fm_ratios[mode_i];
		xm1[mode_i] = x->xm1[mode_i];
		xm2[mode_i] = x->xm2[mode_i];
		ym1[mode_i] = x->ym1[mode_i];
		ym2[mode_i] = x->ym2[mode_i];
		
		delayline[mode_i] = x->delayline[mode_i];

		B[mode_i] 		= decaytimes[mode_i] * 2 * PI * fm[mode_i];
		// B           = decaytimes .* (2 * pi * fm);
		
		phi[mode_i] 	= 2 * PI * (fm[mode_i]/fs);													// resonance frequency
		// R[mode_i]   	= 0.99 - B[mode_i]/2;                                                     	// filter bandwidth
		R[mode_i] 		= exp(-PI*B[mode_i]*(1/fs));
		theta[mode_i] 	= acos(2 * R[mode_i] * cos(phi[mode_i]) / (1 + R[mode_i] * R[mode_i]));		//
		A0[mode_i] 		= (1 - R[mode_i] * R[mode_i]) * sin(theta[mode_i]);							// gain
		d[mode_i] 		= floor(fs/fm[mode_i]);                                                				// compute delay line length

		
	}
	// post("fm[0]:%f fm[1]:%f fm[2]:%f ", fm[0],fm[1],fm[2]);
	// post("B[0]:%f B[1]:%f B[2]:%f ", B[0],B[1],B[2]);
	// post("phi[0]:%f phi[1]:%f phi[2]:%f ", phi[0],phi[1],phi[2]);
	// post("R[0]:%f R[1]:%f R[2]:%f ", R[0],R[1],R[2]);
	// post("theta[0]:%f theta[1]:%f theta[2]:%f ", theta[0],theta[1],theta[2]);
	// post("A0[0]:%f A0[1]:%f A0[2]:%f ", A0[0],A0[1],A0[2]);
	// post("d[0]:%i d[1]:%i d[2]:%i ", d[0],d[1],d[2]);
	// this is the loop that makes the DSP for the karplus strong.
	
	for(i=0; i<n_tick; i++)
	{
		out[i] = 0.0;
		for (int mode_i = 0; mode_i < NMODES; mode_i++)
		{
			posr[mode_i] 		= x->v_posright[mode_i];    	// posr represents where we currenty are in the buffer. 

			// check that the delay line length is between zero and the maximum size.
			// we do not want a delay line that is less than zero or bigger than BUFSIZE 
			if(d[mode_i]<0) 		d[mode_i] = 0;
			if(d[mode_i]>BUFSIZE-1) d[mode_i] = BUFSIZE-1;

			// get the incoming velocity at the current position (posr) in the delay line
			vin = delayline[mode_i][(posr[mode_i]+i + BUFSIZE - d[mode_i])% BUFSIZE];

			y[mode_i] 	= A0[mode_i] * (fb[i] - xm2[mode_i] + 2* R[mode_i] * cos(theta[mode_i]) * ym1[mode_i] - R[mode_i] * R[mode_i] * ym2[mode_i]);
			// out[i] 		= y[mode_i] + fb_gain * vin;			                    // output sample: sum sample to delayed sample
			// delayline[mode_i][(posr[mode_i]+i+BUFSIZE)%BUFSIZE] = out[i];			// write back the value to the current position in the buffer
			mode_out[mode_i]  = y[mode_i] + fb_gain * vin;			                    // output sample: sum sample to delayed sample
			delayline[mode_i][(posr[mode_i]+i+BUFSIZE)%BUFSIZE] = mode_out[mode_i];  	// write back the value to the current position in the buffer
			// out[i] = fb[i];
	       
			xm2[mode_i] = xm1[mode_i];
			xm1[mode_i] = fb[i];

			ym2[mode_i] = ym1[mode_i];
			ym1[mode_i] = y[mode_i];

			out[i] = out[i] + mode_out[mode_i];

		}
		// post("fb[i]: %f out[i]: %f ",fb[i],out[i]);
	}

	for (int mode_i = 0; mode_i < NMODES; mode_i++)
	{
		x->b_ym1[mode_i]	= ym1[mode_i];    							// write back to the x[n-1] value in the class.
		x->xm1[mode_i] 		= xm1[mode_i];
		x->xm2[mode_i] 		= xm2[mode_i];
		x->ym1[mode_i] 		= ym1[mode_i];
		x->ym2[mode_i] 		= ym2[mode_i];

		x->v_posright[mode_i]= (posr[mode_i] + n_tick) % BUFSIZE;    	// update the variable position and write it back to the class variable.
	}
	
	out:
	return (w+ 8);
 }
 

// dsp function sets inlets and outlets.
// calls perform function
void bandedwg_dsp(t_bandedwg *x, t_signal **sp)
{
	dsp_add(bandedwg_perform, 7, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, x, sp[0]->s_n);
}

// function that initializes the buffer and the related space:
void bandedwg_init(t_bandedwg *x)
{
	int i;

	for (int mode_i = 0; mode_i < NMODES; mode_i++)
	{
		x->delayline[mode_i] =  (float *) t_getbytes(BUFSIZE*sizeof(float)) ; // allocates space in memory for delay
		
		for(i=0 ; i< BUFSIZE;  i++)
		{
			x->delayline[mode_i][i] = 0. ; // initialises content of the buffer to zero
		}
	}

}

// creates a new signal processing object
void *bandedwg_new(double f)
{
	t_bandedwg *x = (t_bandedwg *)pd_new(bandedwg_class);
	
	// inlet_new creates its inlets (additional to the first default one)
	inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal); 	// signal processing inlet
	inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal); 	// does not need to be dsp inlet
	inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);	// does not need to be dsp inlet
	// floatinlet_new (&x->x_obj, &x->v_freq);                 		// third inlet is a float, panning amount
	// floatinlet_new (&x->x_obj, &x->f_firstsignal);               // third inlet is a float, panning amount

	outlet_new(&x->x_obj, gensym("signal"));

	// initialize variables of the class 
	//x->v_fb 		= 0.0;
	x->v_posright[0]= 0;
	//x->v_freq 		= 440;

	for (int mode_i = 0; mode_i < NMODES; mode_i++)
	{
		x->b_ym1[mode_i] = 0.;    	// initialize the x[n-1] value to zero	
		x -> xm1[mode_i] = 0;
		x -> xm2[mode_i] = 0;  		// x[n-1],x[n-2]
		x -> ym1[mode_i] = 0;
		x -> ym2[mode_i] = 0;		// y[n-1],y[n-2],
	}
	
	
	
	
	bandedwg_init(x);		// call the init function
	
	return(x);
}
 
// creates the object. Name of the object needs to be specified
void bandedwg_tilde_setup(void)
{
  bandedwg_class = class_new(gensym("bandedwg~"), (t_newmethod)bandedwg_new, 0,
			   sizeof(t_bandedwg), 0, A_DEFFLOAT, 0);
    class_addmethod(bandedwg_class, nullfn, gensym("signal"), 0);
  class_addmethod(bandedwg_class, (t_method)bandedwg_dsp, gensym("dsp"), 0);
}
