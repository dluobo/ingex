/* 
   libltcsmpte - en+decode linear SMPTE timecode

   Copyright (C) 2006, 2007, 2008 Robin Gareus <robin@gareus.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser Public License as published by
   the Free Software Foundation; either version 2.1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser Public License for more details.

   You should have received a copy of the GNU Lesser Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <framerate.h>

long int FR_insert_drop_frames (long int frames) {
	long int minutes = (frames / 17982L) * 10L; ///< 17982 frames in 10 mins base.
	long int off_f = frames % 17982L;
	long int off_adj = 0;

	if (off_f >= 1800L) { // nothing to do in the first minute 
		off_adj  = 2 + 2 * (long long int) floor(((off_f-1800L) / 1798L)); 
	}

	return ( 1800L * minutes + off_f + off_adj);
}

long int FR_drop_frames (FrameRate *fr, int f, int s, int m, int h) {
	double fps = (double)fr->num/(double)fr->den; // (30.0*1000.0/1001.0)
	int fpsi = (int) rint(fps); // 30.0 !
	long int base_time = ((h*3600) + ((m/10) * 10 * 60)) * fps;
	long off_m = m % 10;
	long off_s = (off_m * 60) + s;
	long off_f = (fpsi * off_s) + f - (2 * off_m);
	return (base_time + (long int) off_f);
}

double FR_todbl(FrameRate *fr) {
	return((double)fr->num/(double)fr->den);
}

int FR_toint(FrameRate *fr) {
	return((int)rint(((double)fr->num/(double)fr->den)));
}

/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

#define FIX_SMPTE_OVERFLOW(THIS,NEXT,INC) \
	if (bcd[(THIS)] >= (INC)) { long int ov = (long int) floor((double) bcd[(THIS)] / (INC));  bcd[(THIS)] -= ov*(INC); bcd[(NEXT)]+=ov;} \
	if (bcd[(THIS)] < 0 ) { long int ov = (long int) floor((double) bcd[(THIS)] / (INC));   bcd[(THIS)] -= ov*(INC); bcd[(NEXT)]+=ov;} 

/* supports only integer framerates */
void FR_frame_to_bcd (FrameRate*fr, long int *bcd, long int frame) {
	if (fr->flags&FRF_DROP_FRAMES)
		frame = FR_insert_drop_frames(frame);
#if 0 
	double sec = (double)frame/(double)FR_toint(fr);
	bcd[SMPTE_HOUR] = ((long)floor(sec/3600.0));
	bcd[SMPTE_MIN] = ((long)floor(sec/60.0))%60;
	bcd[SMPTE_SEC] = ((long)floor(sec))%60;
	bcd[SMPTE_FRAME] = frame - ( (bcd[SMPTE_HOUR]*3600 + bcd[SMPTE_MIN]*60 + bcd[SMPTE_SEC])*FR_toint(fr));
#else
	int i;
	int smpte_table[SMPTE_LAST] =  { 1, 60, 60, 24, 0 };
	for (i = 0;i<SMPTE_LAST;i++) bcd[i] = 0L;

	smpte_table[0] = FR_toint(fr);
	bcd[SMPTE_FRAME] = frame;
	for (i = 0;(i+1)<SMPTE_LAST;i++)
		FIX_SMPTE_OVERFLOW(i, i+1, smpte_table[i]);
#endif
}

/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

FrameRate * FR_create(int num, int den, int flags) {
	FrameRate *f = calloc(1, sizeof(FrameRate));
	assert(num>0 && den >0 && flags <= FRF_LAST);
	f->num = num;
	f->den = den;
	f->samplerate = 0;
	f->aoffset = 0;
	f->voffset = 0;
	f->flags = flags&~FRF_SAMPLERATE;
//	f->flags = flags&~(FRF_SAMPLERATE|FRF_AOFFSET|FRF_VOFFSET);
	return(f);
}

void FR_setsamplerate(FrameRate *f, int samplerate) {
	assert(samplerate > 0);
	f->flags |= FRF_SAMPLERATE;
	f->samplerate = samplerate;
}

void FR_free (FrameRate *f) {
	if (f) free(f);
}

/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

#define APV ((double)(fr->samplerate/(double)fr->num*(double)fr->den))

long long int FR_vf2af (FrameRate *fr, long int vf) {
	assert(fr->flags&FRF_SAMPLERATE);
	return ((long long int) floor((double)vf*APV)); 
}

long int FR_af2vfi (FrameRate *fr, long long int af) {
	assert(fr->flags&FRF_SAMPLERATE);
	long int rv = (long int) floor((double)af/APV); 
	return(rv);
}

double FR_af2vf (FrameRate *fr, long long int af) {
	assert(fr->flags&FRF_SAMPLERATE);
	return (((double)af/APV)); 
}

/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

void FR_vf2smpte(FrameRate *fr, char *smptestring, long int vf) {
	if (!smptestring) return;
	long int bcd[SMPTE_LAST];

	if (fr->flags&FRF_DROP_FRAMES)
		vf = FR_insert_drop_frames(vf);

	FR_frame_to_bcd(fr, bcd, vf);

	snprintf(smptestring,13,"%02li:%02li:%02li.%02li",
			bcd[SMPTE_HOUR]%100,
			bcd[SMPTE_MIN],
			bcd[SMPTE_SEC],
			bcd[SMPTE_FRAME]);
}

long int FR_smpte2vf (FrameRate *fr, int f, int s, int m, int h, int overflow) {
	long int rv = 0 ;
	double fps = (double)fr->num/(double)fr->den; // ought to equal (30.0*1000.0/1001.0)

	if (fr->flags&FRF_DROP_FRAMES)
		rv = FR_drop_frames(fr,f,s,m,h);
	else
		rv = f + fps * ( s + 60*m + 3600*h);

	return(rv);
}

long int FR_bcd2vf (FrameRate *fr, int bcd[SMPTE_LAST]) {
	return (FR_smpte2vf(fr, 
			bcd[SMPTE_FRAME], 
			bcd[SMPTE_SEC], 
			bcd[SMPTE_MIN], 
			bcd[SMPTE_HOUR], 0));
}

void FR_setflags(FrameRate *fr, int flags) {
	fr->flags = flags;
}

void FR_setratio(FrameRate *fr, int num, int den) {
	fr->num = num;
	fr->den = den;
}

void FR_setdbl(FrameRate *fr, double fps, int mode) {
	FR_setratio(fr, rint(fps*100000.0),100000); // FIXME use libgmp - see below
	if (mode && (rint(100.0*fps) == 2997.0)) {
		fr->flags |= FRF_DROP_FRAMES;
		FR_setratio(fr, 30000,1001); 
	} else if (mode) 
		fr->flags &= ~FRF_DROP_FRAMES;
}


/*-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_GMP

# include <gmp.h>
int fl2ratio(long int *num, unsigned long int *den, double val) {
	mpq_t rational;
	mpz_t mtmp;
	if (!num || !den) return (1);
	mpz_init (mtmp);
	mpq_init (rational);
	//mpq_set_ui(rational, mnum, mden);
	mpq_set_d(rational, val);
	mpq_canonicalize(rational);
	mpq_get_num(mtmp, rational);
	*num = (long int) mpz_get_d(mtmp);
	mpq_get_den(mtmp, rational);
	*den = (long int) mpz_get_d(mtmp);

	mpq_t ntsc;
	mpq_init (ntsc);
	mpq_set_ui(ntsc,30000,1001);
#if 0 // debug
	printf("cmp: %f <=> 30000/1001 -> %i\n",val, mpq_cmp(rational,ntsc));
#endif
	mpq_clear (ntsc);
#if 0
	char *rats = mpq_get_str(NULL,10,rational);
	printf("compare %f:= %f = %li/%li = %s\n",val,mpq_get_d(rational),*num,*den,rats);
	free(rats);
#endif
	mpq_clear (rational);
	mpz_clear (mtmp);
	return(0);
}

#else /* HAVE_GMP */
int fl2ratio(long int *num, long int *den, double val) {
	return (1);
}
#endif /* HAVE_GMP */
