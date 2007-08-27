/*  			DirectSound
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Rob Riggs
 * Copyright 2000-2002 TransGaming Technologies, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>

#define NONAMELESSSTRUCT
#define NONAMELESSUNION
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "mmsystem.h"
#include "winternl.h"
#include "mmddk.h"
#include "wine/debug.h"
#include "dsound.h"
#include "dsdriver.h"
#include "dsound_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dsound);

static void DSOUND_RecalcPrimary(DirectSoundDevice *device)
{
	DWORD nBlockAlign;
	DWORD fraglen;
	TRACE("(%p)\n", device);

	nBlockAlign = device->pwfx->nBlockAlign;
	/* Alsa doesn't have continuous buffers, instead it has buffers with power of 2,
	 * If DS_TIME_DEL is about 10 ms, 512 * nBlockAlign is roughly correct */
	fraglen = 512 * nBlockAlign;

	/* Compensate for only being roughly accurate */
	if (device->pwfx->nSamplesPerSec <= 26000)
		fraglen /= 2;

	if (device->pwfx->nSamplesPerSec <= 12000)
		fraglen /= 2;

	if (device->pwfx->nSamplesPerSec >= 80000)
		fraglen *= 2;

	device->fraglen = fraglen;
	device->helfrags = device->buflen / fraglen;
	TRACE("fraglen=%d helfrags=%d\n", device->fraglen, device->helfrags);

	if (device->hwbuf && device->drvdesc.dwFlags & DSDDESC_DONTNEEDWRITELEAD)
		device->writelead = 0;
	else
	/* calculate the 10ms write lead */
		device->writelead = (device->pwfx->nSamplesPerSec / 100) * nBlockAlign;
}

HRESULT DSOUND_ReopenDevice(DirectSoundDevice *device, BOOL forcewave)
{
	HRESULT hres = DS_OK;
	if (device->driver)
	{
		IDsDriver_Close(device->driver);
		if (device->drvdesc.dwFlags & DSDDESC_DOMMSYSTEMOPEN)
			waveOutClose(device->hwo);
		IDsDriver_Release(device->driver);
		device->driver = NULL;
		device->buffer = NULL;
		device->hwo = 0;
	}
	else if (device->drvdesc.dwFlags & DSDDESC_DOMMSYSTEMOPEN)
		waveOutClose(device->hwo);

	/* DRV_QUERYDSOUNDIFACE is a "Wine extension" to get the DSound interface */
	if (ds_hw_accel != DS_HW_ACCEL_EMULATION && !forcewave)
		waveOutMessage((HWAVEOUT)device->drvdesc.dnDevNode, DRV_QUERYDSOUNDIFACE, (DWORD_PTR)&device->driver, 0);

	/* Get driver description */
	if (device->driver) {
		DWORD wod = device->drvdesc.dnDevNode;
		hres = IDsDriver_GetDriverDesc(device->driver,&(device->drvdesc));
		device->drvdesc.dnDevNode = wod;
		if (FAILED(hres)) {
			WARN("IDsDriver_GetDriverDesc failed: %08x\n", hres);
			IDsDriver_Release(device->driver);
			device->driver = NULL;
		}
        }

        /* if no DirectSound interface available, use WINMM API instead */
	if (!device->driver)
		device->drvdesc.dwFlags = DSDDESC_DOMMSYSTEMOPEN | DSDDESC_DOMMSYSTEMSETFORMAT;

	if (device->drvdesc.dwFlags & DSDDESC_DOMMSYSTEMOPEN)
	{
		DWORD flags = CALLBACK_FUNCTION;

		if (device->driver)
			flags |= WAVE_DIRECTSOUND;

		hres = mmErr(waveOutOpen(&(device->hwo), device->drvdesc.dnDevNode, device->pwfx, (DWORD_PTR)DSOUND_callback, (DWORD)device, flags));
		if (FAILED(hres)) {
			WARN("waveOutOpen failed\n");
			if (device->driver)
			{
				IDsDriver_Release(device->driver);
				device->driver = NULL;
			}
			return hres;
		}
	}

	if (device->driver)
		hres = IDsDriver_Open(device->driver);

	return hres;
}

static HRESULT DSOUND_PrimaryOpen(DirectSoundDevice *device)
{
	HRESULT err = DS_OK;
	TRACE("(%p)\n", device);

	if (device->driver)
	{
		err = IDsDriver_CreateSoundBuffer(device->driver,device->pwfx,
						  DSBCAPS_PRIMARYBUFFER,0,
						  &(device->buflen),&(device->buffer),
						  (LPVOID*)&(device->hwbuf));

		if (err != DS_OK) {
			WARN("IDsDriver_CreateSoundBuffer failed (%08x), falling back to waveout\n", err);
			err = DSOUND_ReopenDevice(device, TRUE);
			if (FAILED(err))
			{
				WARN("Falling back to waveout failed too! Giving up\n");
				return err;
			}
		}
	}

	if (device->state == STATE_PLAYING) device->state = STATE_STARTING;
	else if (device->state == STATE_STOPPING) device->state = STATE_STOPPED;

	/* are we using waveOut stuff? */
	if (!device->driver) {
		LPBYTE newbuf;
		LPWAVEHDR headers = NULL;
		DWORD buflen, overshot, oldbuflen;
		unsigned int c;

		/* Start in pause mode, to allow buffers to get filled */
		waveOutPause(device->hwo);

		/* on original windows, the buffer it set to a fixed size, no matter what the settings are.
		   on windows this size is always fixed (tested on win-xp) */
		if (!device->buflen)
			buflen = ds_hel_buflen;
		else /* In case we move from hw accelerated to waveout */
			buflen = device->buflen;
		buflen -= ds_hel_buflen % device->pwfx->nBlockAlign;

		TRACE("desired buflen=%d, old buffer=%p\n", buflen, device->buffer);

		/* reallocate emulated primary buffer */
		if (device->buffer)
			newbuf = HeapReAlloc(GetProcessHeap(),0,device->buffer,buflen);
		else
			newbuf = HeapAlloc(GetProcessHeap(),0,buflen);

		if (!newbuf) {
			ERR("failed to allocate primary buffer\n");
			return DSERR_OUTOFMEMORY;
			/* but the old buffer might still exist and must be re-prepared */
		}

		oldbuflen = device->buflen;
		device->buflen = buflen;
		DSOUND_RecalcPrimary(device);
		if (device->pwave)
			headers = HeapReAlloc(GetProcessHeap(),0,device->pwave, device->helfrags * sizeof(WAVEHDR));
		else
			headers = HeapAlloc(GetProcessHeap(),0,device->helfrags * sizeof(WAVEHDR));

		if (!headers) {
			ERR("failed to allocate wave headers\n");
			HeapFree(GetProcessHeap(), 0, newbuf);
			device->buflen = oldbuflen;
			DSOUND_RecalcPrimary(device);
			return DSERR_OUTOFMEMORY;
		}

		device->buffer = newbuf;
		device->pwave = headers;

		/* prepare fragment headers */
		for (c=0; c<device->helfrags; c++) {
			device->pwave[c].lpData = (char*)device->buffer + c*device->fraglen;
			device->pwave[c].dwBufferLength = device->fraglen;
			device->pwave[c].dwUser = (DWORD)device;
			device->pwave[c].dwFlags = 0;
			device->pwave[c].dwLoops = 0;
			err = mmErr(waveOutPrepareHeader(device->hwo,&device->pwave[c],sizeof(WAVEHDR)));
			if (err != DS_OK) {
				while (c--)
					waveOutUnprepareHeader(device->hwo,&device->pwave[c],sizeof(WAVEHDR));
				break;
			}
		}

		overshot = device->buflen % device->helfrags;
		/* sanity */
		if(overshot)
		{
			WARN("helfrags (%d x %d) doesn't fit entirely in buflen (%d) overshot: %d\n", device->helfrags, device->fraglen, device->buflen, overshot);
			device->pwave[device->helfrags - 1].dwBufferLength += overshot;
		}

		TRACE("fraglen=%d\n", device->fraglen);
	}
	FillMemory(device->buffer, device->buflen, (device->pwfx->wBitsPerSample == 8) ? 128 : 0);
	device->pwplay = device->pwqueue = device->playpos = device->mixpos = 0;
	DSOUND_RecalcPrimary(device);

	return err;
}


static void DSOUND_PrimaryClose(DirectSoundDevice *device)
{
	TRACE("(%p)\n", device);

	/* are we using waveOut stuff? */
	if (!device->hwbuf) {
		unsigned c;

		/* get out of CS when calling the wave system */
		LeaveCriticalSection(&(device->mixlock));
		/* **** */
		device->pwqueue = (DWORD)-1; /* resetting queues */
		waveOutReset(device->hwo);
		for (c=0; c<device->helfrags; c++)
			waveOutUnprepareHeader(device->hwo, &device->pwave[c], sizeof(WAVEHDR));
		/* **** */
		EnterCriticalSection(&(device->mixlock));

		/* clear the queue */
		device->pwqueue = 0;
	} else {
		ULONG ref = IDsDriverBuffer_Release(device->hwbuf);
		if (!ref)
			device->hwbuf = 0;
		else
			ERR("Still %d references on primary buffer, refcount leak?\n", ref);
	}
}

HRESULT DSOUND_PrimaryCreate(DirectSoundDevice *device)
{
	HRESULT err = DS_OK;
	TRACE("(%p)\n", device);

	device->buflen = ds_hel_buflen;
	err = DSOUND_PrimaryOpen(device);

	if (err != DS_OK) {
		WARN("DSOUND_PrimaryOpen failed\n");
		return err;
	}

	device->state = STATE_STOPPED;
	return DS_OK;
}

HRESULT DSOUND_PrimaryDestroy(DirectSoundDevice *device)
{
	TRACE("(%p)\n", device);

	/* **** */
	EnterCriticalSection(&(device->mixlock));

	DSOUND_PrimaryClose(device);
	if (device->driver) {
		if (device->hwbuf) {
			if (IDsDriverBuffer_Release(device->hwbuf) == 0)
				device->hwbuf = 0;
		}
	} else
                HeapFree(GetProcessHeap(),0,device->pwave);
        HeapFree(GetProcessHeap(),0,device->pwfx);
        device->pwfx=NULL;

	LeaveCriticalSection(&(device->mixlock));
	/* **** */

	return DS_OK;
}

HRESULT DSOUND_PrimaryPlay(DirectSoundDevice *device)
{
	HRESULT err = DS_OK;
	TRACE("(%p)\n", device);

	if (device->hwbuf) {
		err = IDsDriverBuffer_Play(device->hwbuf, 0, 0, DSBPLAY_LOOPING);
		if (err != DS_OK)
			WARN("IDsDriverBuffer_Play failed\n");
	} else {
		err = mmErr(waveOutRestart(device->hwo));
		if (err != DS_OK)
			WARN("waveOutRestart failed\n");
	}

	return err;
}

HRESULT DSOUND_PrimaryStop(DirectSoundDevice *device)
{
	HRESULT err = DS_OK;
	TRACE("(%p)\n", device);

	if (device->hwbuf) {
		err = IDsDriverBuffer_Stop(device->hwbuf);
		if (err == DSERR_BUFFERLOST) {
			DSOUND_PrimaryClose(device);
			err = DSOUND_ReopenDevice(device, !device->driver);
			if (FAILED(err))
				ERR("DSOUND_ReopenDevice failed\n");
			else
			{
				err = DSOUND_PrimaryOpen(device);
				if (FAILED(err))
					WARN("DSOUND_PrimaryOpen failed\n");
			}
		} else if (err != DS_OK) {
			WARN("IDsDriverBuffer_Stop failed\n");
		}
	} else {

		/* don't call the wave system with the lock set */
		LeaveCriticalSection(&(device->mixlock));
		/* **** */

		err = mmErr(waveOutPause(device->hwo));

		/* **** */
		EnterCriticalSection(&(device->mixlock));

		if (err != DS_OK)
			WARN("waveOutPause failed\n");
	}

	return err;
}

HRESULT DSOUND_PrimaryGetPosition(DirectSoundDevice *device, LPDWORD playpos, LPDWORD writepos)
{
	TRACE("(%p,%p,%p)\n", device, playpos, writepos);

	if (device->hwbuf) {
		HRESULT err=IDsDriverBuffer_GetPosition(device->hwbuf,playpos,writepos);
		if (err) {
			WARN("IDsDriverBuffer_GetPosition failed\n");
			return err;
		}
	} else {

		/* check if playpos was requested */
		if (playpos) {
			/* use the cached play position */
			*playpos = device->pwplay * device->fraglen;
		}

		/* check if writepos was requested */
		if (writepos) {
			TRACE("pwplay=%i, pwqueue=%i\n", device->pwplay, device->pwqueue);

			/* the writepos is the first non-queued position */
			*writepos = (device->pwplay + device->pwqueue) * device->fraglen;
			*writepos %= device->buflen;
		}
	}
	TRACE("playpos = %d, writepos = %d (%p, time=%d)\n", playpos?*playpos:0, writepos?*writepos:0, device, GetTickCount());
	return DS_OK;
}

HRESULT DSOUND_PrimarySetFormat(DirectSoundDevice *device, LPCWAVEFORMATEX wfex)
{
	HRESULT err = DS_OK;
	int i, alloc_size, cp_size;
	DWORD nSamplesPerSec, bpp, chans;
	TRACE("(%p,%p)\n", device, wfex);

	if (device->priolevel == DSSCL_NORMAL) {
		WARN("failed priority check!\n");
		return DSERR_PRIOLEVELNEEDED;
	}

	/* Let's be pedantic! */
	if (wfex == NULL) {
		WARN("invalid parameter: wfex==NULL!\n");
		return DSERR_INVALIDPARAM;
	}
	TRACE("(formattag=0x%04x,chans=%d,samplerate=%d,"
              "bytespersec=%d,blockalign=%d,bitspersamp=%d,cbSize=%d)\n",
	      wfex->wFormatTag, wfex->nChannels, wfex->nSamplesPerSec,
	      wfex->nAvgBytesPerSec, wfex->nBlockAlign,
	      wfex->wBitsPerSample, wfex->cbSize);

	/* **** */
	RtlAcquireResourceExclusive(&(device->buffer_list_lock), TRUE);
	EnterCriticalSection(&(device->mixlock));

	if (wfex->wFormatTag == WAVE_FORMAT_PCM) {
            alloc_size = sizeof(WAVEFORMATEX);
            cp_size = sizeof(PCMWAVEFORMAT);
        } else
            alloc_size = cp_size = sizeof(WAVEFORMATEX) + wfex->cbSize;

        device->pwfx = HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,device->pwfx,alloc_size);

	nSamplesPerSec = device->pwfx->nSamplesPerSec;
	bpp = device->pwfx->wBitsPerSample;
	chans = device->pwfx->nChannels;

        CopyMemory(device->pwfx, wfex, cp_size);

	if (!(device->drvdesc.dwFlags & DSDDESC_DOMMSYSTEMSETFORMAT) && device->hwbuf) {
		err = IDsDriverBuffer_SetFormat(device->hwbuf, device->pwfx);
		if (err != DSERR_BUFFERLOST && FAILED(err)) {
			WARN("IDsDriverBuffer_SetFormat failed\n");
			goto done;
		}
		else DSOUND_RecalcPrimary(device);
	}

	if (err == DSERR_BUFFERLOST || device->drvdesc.dwFlags & DSDDESC_DOMMSYSTEMSETFORMAT)
	{
		DSOUND_PrimaryClose(device);

		if (!device->driver)
		{
			waveOutClose(device->hwo);
			device->hwo = 0;
			err = mmErr(waveOutOpen(&(device->hwo), device->drvdesc.dnDevNode, device->pwfx, (DWORD_PTR)DSOUND_callback, (DWORD)device, CALLBACK_FUNCTION));
			if (FAILED(err))
			{
				WARN("waveOutOpen failed\n");
				goto done;
			}
		}

		err = DSOUND_PrimaryOpen(device);
		if (err != DS_OK) {
			WARN("DSOUND_PrimaryOpen failed\n");
			goto done;
		}
	}

	if (nSamplesPerSec != device->pwfx->nSamplesPerSec || bpp != device->pwfx->wBitsPerSample || chans != device->pwfx->nChannels) {
		IDirectSoundBufferImpl** dsb = device->buffers;
		for (i = 0; i < device->nrofbuffers; i++, dsb++) {
			/* **** */
			RtlAcquireResourceExclusive(&(*dsb)->lock, TRUE);

			(*dsb)->freqAdjust = ((DWORD64)(*dsb)->freq << DSOUND_FREQSHIFT) / device->pwfx->nSamplesPerSec;
			DSOUND_RecalcFormat((*dsb));
			DSOUND_MixToTemporary((*dsb), 0, (*dsb)->buflen);

			RtlReleaseResource(&(*dsb)->lock);
			/* **** */
		}
	}

done:
	LeaveCriticalSection(&(device->mixlock));
	RtlReleaseResource(&(device->buffer_list_lock));
	/* **** */

	return err;
}

/*******************************************************************************
 *		PrimaryBuffer
 */
/* This sets this format for the <em>Primary Buffer Only</em> */
/* See file:///cdrom/sdk52/docs/worddoc/dsound.doc page 120 */
static HRESULT WINAPI PrimaryBufferImpl_SetFormat(
    LPDIRECTSOUNDBUFFER iface,
    LPCWAVEFORMATEX wfex)
{
    TRACE("(%p,%p)\n", iface, wfex);
    return DSOUND_PrimarySetFormat(((PrimaryBufferImpl *)iface)->device, wfex);
}

static HRESULT WINAPI PrimaryBufferImpl_SetVolume(
	LPDIRECTSOUNDBUFFER iface,LONG vol
) {
	DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	DWORD ampfactors;
	DSVOLUMEPAN volpan;
        HRESULT hres = DS_OK;
	TRACE("(%p,%d)\n", iface, vol);

	if (!(device->dsbd.dwFlags & DSBCAPS_CTRLVOLUME)) {
		WARN("control unavailable\n");
		return DSERR_CONTROLUNAVAIL;
	}

	if ((vol > DSBVOLUME_MAX) || (vol < DSBVOLUME_MIN)) {
		WARN("invalid parameter: vol = %d\n", vol);
		return DSERR_INVALIDPARAM;
	}

	/* **** */
	EnterCriticalSection(&(device->mixlock));

        waveOutGetVolume(device->hwo, &ampfactors);
        volpan.dwTotalLeftAmpFactor=ampfactors & 0xffff;
        volpan.dwTotalRightAmpFactor=ampfactors >> 16;
        DSOUND_AmpFactorToVolPan(&volpan);
        if (vol != volpan.lVolume) {
            volpan.lVolume=vol;
            DSOUND_RecalcVolPan(&volpan);
            if (device->hwbuf) {
                hres = IDsDriverBuffer_SetVolumePan(device->hwbuf, &volpan);
                if (hres != DS_OK)
                    WARN("IDsDriverBuffer_SetVolumePan failed\n");
            } else {
                ampfactors = (volpan.dwTotalLeftAmpFactor & 0xffff) | (volpan.dwTotalRightAmpFactor << 16);
                waveOutSetVolume(device->hwo, ampfactors);
            }
        }

	LeaveCriticalSection(&(device->mixlock));
	/* **** */

	return hres;
}

static HRESULT WINAPI PrimaryBufferImpl_GetVolume(
	LPDIRECTSOUNDBUFFER iface,LPLONG vol
) {
	DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	DWORD ampfactors;
	DSVOLUMEPAN volpan;
	TRACE("(%p,%p)\n", iface, vol);

	if (!(device->dsbd.dwFlags & DSBCAPS_CTRLVOLUME)) {
		WARN("control unavailable\n");
		return DSERR_CONTROLUNAVAIL;
	}

	if (vol == NULL) {
		WARN("invalid parameter: vol = NULL\n");
		return DSERR_INVALIDPARAM;
	}

	waveOutGetVolume(device->hwo, &ampfactors);
	volpan.dwTotalLeftAmpFactor=ampfactors & 0xffff;
	volpan.dwTotalRightAmpFactor=ampfactors >> 16;
	DSOUND_AmpFactorToVolPan(&volpan);
	*vol = volpan.lVolume;
	return DS_OK;
}

static HRESULT WINAPI PrimaryBufferImpl_SetFrequency(
	LPDIRECTSOUNDBUFFER iface,DWORD freq
) {
	PrimaryBufferImpl *This = (PrimaryBufferImpl *)iface;
	TRACE("(%p,%d)\n",This,freq);

	/* You cannot set the frequency of the primary buffer */
	WARN("control unavailable\n");
	return DSERR_CONTROLUNAVAIL;
}

static HRESULT WINAPI PrimaryBufferImpl_Play(
	LPDIRECTSOUNDBUFFER iface,DWORD reserved1,DWORD reserved2,DWORD flags
) {
	DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	TRACE("(%p,%08x,%08x,%08x)\n", iface, reserved1, reserved2, flags);

	if (!(flags & DSBPLAY_LOOPING)) {
		WARN("invalid parameter: flags = %08x\n", flags);
		return DSERR_INVALIDPARAM;
	}

	/* **** */
	EnterCriticalSection(&(device->mixlock));

	if (device->state == STATE_STOPPED)
		device->state = STATE_STARTING;
	else if (device->state == STATE_STOPPING)
		device->state = STATE_PLAYING;

	LeaveCriticalSection(&(device->mixlock));
	/* **** */

	return DS_OK;
}

static HRESULT WINAPI PrimaryBufferImpl_Stop(LPDIRECTSOUNDBUFFER iface)
{
	DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	TRACE("(%p)\n", iface);

	/* **** */
	EnterCriticalSection(&(device->mixlock));

	if (device->state == STATE_PLAYING)
		device->state = STATE_STOPPING;
	else if (device->state == STATE_STARTING)
		device->state = STATE_STOPPED;

	LeaveCriticalSection(&(device->mixlock));
	/* **** */

	return DS_OK;
}

static ULONG WINAPI PrimaryBufferImpl_AddRef(LPDIRECTSOUNDBUFFER iface)
{
    PrimaryBufferImpl *This = (PrimaryBufferImpl *)iface;
    ULONG ref = InterlockedIncrement(&(This->ref));
    TRACE("(%p) ref was %d\n", This, ref - 1);
    return ref;
}

static ULONG WINAPI PrimaryBufferImpl_Release(LPDIRECTSOUNDBUFFER iface)
{
    PrimaryBufferImpl *This = (PrimaryBufferImpl *)iface;
    DWORD ref = InterlockedDecrement(&(This->ref));
    TRACE("(%p) ref was %d\n", This, ref + 1);

    if (!ref) {
        This->device->primary = NULL;
        HeapFree(GetProcessHeap(), 0, This);
        TRACE("(%p) released\n", This);
    }
    return ref;
}

static HRESULT WINAPI PrimaryBufferImpl_GetCurrentPosition(
	LPDIRECTSOUNDBUFFER iface,LPDWORD playpos,LPDWORD writepos
) {
	HRESULT	hres;
        DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	TRACE("(%p,%p,%p)\n", iface, playpos, writepos);

	/* **** */
	EnterCriticalSection(&(device->mixlock));

	hres = DSOUND_PrimaryGetPosition(device, playpos, writepos);
	if (hres != DS_OK) {
		WARN("DSOUND_PrimaryGetPosition failed\n");
		LeaveCriticalSection(&(device->mixlock));
		return hres;
	}
	if (writepos) {
		if (device->state != STATE_STOPPED)
			/* apply the documented 10ms lead to writepos */
			*writepos += device->writelead;
		while (*writepos >= device->buflen) *writepos -= device->buflen;
	}

	LeaveCriticalSection(&(device->mixlock));
	/* **** */

	TRACE("playpos = %d, writepos = %d (%p, time=%d)\n", playpos?*playpos:0, writepos?*writepos:0, device, GetTickCount());
	return DS_OK;
}

static HRESULT WINAPI PrimaryBufferImpl_GetStatus(
	LPDIRECTSOUNDBUFFER iface,LPDWORD status
) {
        DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	TRACE("(%p,%p)\n", iface, status);

	if (status == NULL) {
		WARN("invalid parameter: status == NULL\n");
		return DSERR_INVALIDPARAM;
	}

	*status = 0;
	if ((device->state == STATE_STARTING) ||
	    (device->state == STATE_PLAYING))
		*status |= DSBSTATUS_PLAYING | DSBSTATUS_LOOPING;

	TRACE("status=%x\n", *status);
	return DS_OK;
}


static HRESULT WINAPI PrimaryBufferImpl_GetFormat(
    LPDIRECTSOUNDBUFFER iface,
    LPWAVEFORMATEX lpwf,
    DWORD wfsize,
    LPDWORD wfwritten)
{
    DWORD size;
    DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
    TRACE("(%p,%p,%d,%p)\n", iface, lpwf, wfsize, wfwritten);

    size = sizeof(WAVEFORMATEX) + device->pwfx->cbSize;

    if (lpwf) {	/* NULL is valid */
        if (wfsize >= size) {
            CopyMemory(lpwf,device->pwfx,size);
            if (wfwritten)
                *wfwritten = size;
        } else {
            WARN("invalid parameter: wfsize too small\n");
            if (wfwritten)
                *wfwritten = 0;
            return DSERR_INVALIDPARAM;
        }
    } else {
        if (wfwritten)
            *wfwritten = sizeof(WAVEFORMATEX) + device->pwfx->cbSize;
        else {
            WARN("invalid parameter: wfwritten == NULL\n");
            return DSERR_INVALIDPARAM;
        }
    }

    return DS_OK;
}

static HRESULT WINAPI PrimaryBufferImpl_Lock(
	LPDIRECTSOUNDBUFFER iface,DWORD writecursor,DWORD writebytes,LPVOID *lplpaudioptr1,LPDWORD audiobytes1,LPVOID *lplpaudioptr2,LPDWORD audiobytes2,DWORD flags
) {
	HRESULT hres;
        DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	TRACE("(%p,%d,%d,%p,%p,%p,%p,0x%08x) at %d\n",
		iface,
		writecursor,
		writebytes,
		lplpaudioptr1,
		audiobytes1,
		lplpaudioptr2,
		audiobytes2,
		flags,
		GetTickCount()
	);

	if (device->priolevel != DSSCL_WRITEPRIMARY) {
		WARN("failed priority check!\n");
		return DSERR_PRIOLEVELNEEDED;
	}

        /* when this flag is set, writecursor is meaningless and must be calculated */
	if (flags & DSBLOCK_FROMWRITECURSOR) {
		/* GetCurrentPosition does too much magic to duplicate here */
		hres = IDirectSoundBuffer_GetCurrentPosition(iface, NULL, &writecursor);
		if (hres != DS_OK) {
			WARN("IDirectSoundBuffer_GetCurrentPosition failed\n");
			return hres;
		}
	}

        /* when this flag is set, writebytes is meaningless and must be set */
	if (flags & DSBLOCK_ENTIREBUFFER)
		writebytes = device->buflen;

        if (writecursor >= device->buflen) {
                WARN("Invalid parameter, writecursor: %u >= buflen: %u\n",
		     writecursor, device->buflen);
                return DSERR_INVALIDPARAM;
        }

        if (writebytes > device->buflen) {
                WARN("Invalid parameter, writebytes: %u > buflen: %u\n",
		     writebytes, device->buflen);
                return DSERR_INVALIDPARAM;
        }

	if (!(device->drvdesc.dwFlags & DSDDESC_DONTNEEDPRIMARYLOCK) && device->hwbuf) {
		hres = IDsDriverBuffer_Lock(device->hwbuf,
					    lplpaudioptr1, audiobytes1,
					    lplpaudioptr2, audiobytes2,
					    writecursor, writebytes,
					    0);
		if (hres != DS_OK) {
			WARN("IDsDriverBuffer_Lock failed\n");
			return hres;
		}
	} else {
		if (writecursor+writebytes <= device->buflen) {
			*(LPBYTE*)lplpaudioptr1 = device->buffer+writecursor;
			*audiobytes1 = writebytes;
			if (lplpaudioptr2)
				*(LPBYTE*)lplpaudioptr2 = NULL;
			if (audiobytes2)
				*audiobytes2 = 0;
			TRACE("->%d.0\n",writebytes);
		} else {
			*(LPBYTE*)lplpaudioptr1 = device->buffer+writecursor;
			*audiobytes1 = device->buflen-writecursor;
			if (lplpaudioptr2)
				*(LPBYTE*)lplpaudioptr2 = device->buffer;
			if (audiobytes2)
				*audiobytes2 = writebytes-(device->buflen-writecursor);
			TRACE("->%d.%d\n",*audiobytes1,audiobytes2?*audiobytes2:0);
		}
	}
	return DS_OK;
}

static HRESULT WINAPI PrimaryBufferImpl_SetCurrentPosition(
	LPDIRECTSOUNDBUFFER iface,DWORD newpos
) {
	PrimaryBufferImpl *This = (PrimaryBufferImpl *)iface;
	TRACE("(%p,%d)\n",This,newpos);

	/* You cannot set the position of the primary buffer */
	WARN("invalid call\n");
	return DSERR_INVALIDCALL;
}

static HRESULT WINAPI PrimaryBufferImpl_SetPan(
	LPDIRECTSOUNDBUFFER iface,LONG pan
) {
        DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	DWORD ampfactors;
	DSVOLUMEPAN volpan;
        HRESULT hres = DS_OK;
	TRACE("(%p,%d)\n", iface, pan);

	if (!(device->dsbd.dwFlags & DSBCAPS_CTRLPAN)) {
		WARN("control unavailable\n");
		return DSERR_CONTROLUNAVAIL;
	}

	if ((pan > DSBPAN_RIGHT) || (pan < DSBPAN_LEFT)) {
		WARN("invalid parameter: pan = %d\n", pan);
		return DSERR_INVALIDPARAM;
	}

	/* **** */
	EnterCriticalSection(&(device->mixlock));

        waveOutGetVolume(device->hwo, &ampfactors);
        volpan.dwTotalLeftAmpFactor=ampfactors & 0xffff;
        volpan.dwTotalRightAmpFactor=ampfactors >> 16;
        DSOUND_AmpFactorToVolPan(&volpan);
        if (pan != volpan.lPan) {
            volpan.lPan=pan;
            DSOUND_RecalcVolPan(&volpan);
            if (device->hwbuf) {
                hres = IDsDriverBuffer_SetVolumePan(device->hwbuf, &volpan);
                if (hres != DS_OK)
                    WARN("IDsDriverBuffer_SetVolumePan failed\n");
            } else {
                ampfactors = (volpan.dwTotalLeftAmpFactor & 0xffff) | (volpan.dwTotalRightAmpFactor << 16);
                waveOutSetVolume(device->hwo, ampfactors);
            }
        }

	LeaveCriticalSection(&(device->mixlock));
	/* **** */

	return hres;
}

static HRESULT WINAPI PrimaryBufferImpl_GetPan(
	LPDIRECTSOUNDBUFFER iface,LPLONG pan
) {
        DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	DWORD ampfactors;
	DSVOLUMEPAN volpan;
	TRACE("(%p,%p)\n", iface, pan);

	if (!(device->dsbd.dwFlags & DSBCAPS_CTRLPAN)) {
		WARN("control unavailable\n");
		return DSERR_CONTROLUNAVAIL;
	}

	if (pan == NULL) {
		WARN("invalid parameter: pan == NULL\n");
		return DSERR_INVALIDPARAM;
	}

	waveOutGetVolume(device->hwo, &ampfactors);
	volpan.dwTotalLeftAmpFactor=ampfactors & 0xffff;
	volpan.dwTotalRightAmpFactor=ampfactors >> 16;
	DSOUND_AmpFactorToVolPan(&volpan);
	*pan = volpan.lPan;
	return DS_OK;
}

static HRESULT WINAPI PrimaryBufferImpl_Unlock(
	LPDIRECTSOUNDBUFFER iface,LPVOID p1,DWORD x1,LPVOID p2,DWORD x2
) {
        DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	TRACE("(%p,%p,%d,%p,%d)\n", iface, p1, x1, p2, x2);

	if (device->priolevel != DSSCL_WRITEPRIMARY) {
		WARN("failed priority check!\n");
		return DSERR_PRIOLEVELNEEDED;
	}

	if (!(device->drvdesc.dwFlags & DSDDESC_DONTNEEDPRIMARYLOCK) && device->hwbuf) {
		HRESULT	hres;
		
		hres = IDsDriverBuffer_Unlock(device->hwbuf, p1, x1, p2, x2);
		if (hres != DS_OK) {
			WARN("IDsDriverBuffer_Unlock failed\n");
			return hres;
		}
	}

	return DS_OK;
}

static HRESULT WINAPI PrimaryBufferImpl_Restore(
	LPDIRECTSOUNDBUFFER iface
) {
	PrimaryBufferImpl *This = (PrimaryBufferImpl *)iface;
	FIXME("(%p):stub\n",This);
	return DS_OK;
}

static HRESULT WINAPI PrimaryBufferImpl_GetFrequency(
	LPDIRECTSOUNDBUFFER iface,LPDWORD freq
) {
        DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
	TRACE("(%p,%p)\n", iface, freq);

	if (freq == NULL) {
		WARN("invalid parameter: freq == NULL\n");
		return DSERR_INVALIDPARAM;
	}

	if (!(device->dsbd.dwFlags & DSBCAPS_CTRLFREQUENCY)) {
		WARN("control unavailable\n");
		return DSERR_CONTROLUNAVAIL;
	}

	*freq = device->pwfx->nSamplesPerSec;
	TRACE("-> %d\n", *freq);

	return DS_OK;
}

static HRESULT WINAPI PrimaryBufferImpl_Initialize(
	LPDIRECTSOUNDBUFFER iface,LPDIRECTSOUND dsound,LPCDSBUFFERDESC dbsd
) {
	PrimaryBufferImpl *This = (PrimaryBufferImpl *)iface;
	WARN("(%p) already initialized\n", This);
	return DSERR_ALREADYINITIALIZED;
}

static HRESULT WINAPI PrimaryBufferImpl_GetCaps(
	LPDIRECTSOUNDBUFFER iface,LPDSBCAPS caps
) {
        DirectSoundDevice *device = ((PrimaryBufferImpl *)iface)->device;
  	TRACE("(%p,%p)\n", iface, caps);

	if (caps == NULL) {
		WARN("invalid parameter: caps == NULL\n");
		return DSERR_INVALIDPARAM;
	}

	if (caps->dwSize < sizeof(*caps)) {
		WARN("invalid parameter: caps->dwSize = %d\n", caps->dwSize);
		return DSERR_INVALIDPARAM;
	}

	caps->dwFlags = device->dsbd.dwFlags;
	caps->dwBufferBytes = device->buflen;

	/* Windows reports these as zero */
	caps->dwUnlockTransferRate = 0;
	caps->dwPlayCpuOverhead = 0;

	return DS_OK;
}

static HRESULT WINAPI PrimaryBufferImpl_QueryInterface(
	LPDIRECTSOUNDBUFFER iface,REFIID riid,LPVOID *ppobj
) {
        PrimaryBufferImpl *This = (PrimaryBufferImpl *)iface;
        DirectSoundDevice *device = This->device;
	TRACE("(%p,%s,%p)\n", iface, debugstr_guid(riid), ppobj);

	if (ppobj == NULL) {
		WARN("invalid parameter\n");
		return E_INVALIDARG;
	}

	*ppobj = NULL;	/* assume failure */

	if ( IsEqualGUID(riid, &IID_IUnknown) ||
	     IsEqualGUID(riid, &IID_IDirectSoundBuffer) ) {
		IDirectSoundBuffer_AddRef((LPDIRECTSOUNDBUFFER)This);
		*ppobj = This;
		return S_OK;
	}

	/* DirectSoundBuffer and DirectSoundBuffer8 are different and */
	/* a primary buffer can't have a DirectSoundBuffer8 interface */
	if ( IsEqualGUID( &IID_IDirectSoundBuffer8, riid ) ) {
		WARN("app requested DirectSoundBuffer8 on primary buffer\n");
		return E_NOINTERFACE;
	}

	if ( IsEqualGUID( &IID_IDirectSoundNotify, riid ) ) {
		ERR("app requested IDirectSoundNotify on primary buffer\n");
		/* FIXME: should we support this? */
		return E_NOINTERFACE;
	}

	if ( IsEqualGUID( &IID_IDirectSound3DBuffer, riid ) ) {
		ERR("app requested IDirectSound3DBuffer on primary buffer\n");
		return E_NOINTERFACE;
	}

        if ( IsEqualGUID( &IID_IDirectSound3DListener, riid ) ) {
		if (!device->listener)
			IDirectSound3DListenerImpl_Create(device, &device->listener);
		if (device->listener) {
			*ppobj = device->listener;
			IDirectSound3DListener_AddRef((LPDIRECTSOUND3DLISTENER)*ppobj);
			return S_OK;
		}

		WARN("IID_IDirectSound3DListener failed\n");
		return E_NOINTERFACE;
	}

	if ( IsEqualGUID( &IID_IKsPropertySet, riid ) ) {
		FIXME("app requested IKsPropertySet on primary buffer\n");
		return E_NOINTERFACE;
	}

	FIXME( "Unknown IID %s\n", debugstr_guid( riid ) );
	return E_NOINTERFACE;
}

static const IDirectSoundBufferVtbl dspbvt =
{
	PrimaryBufferImpl_QueryInterface,
	PrimaryBufferImpl_AddRef,
	PrimaryBufferImpl_Release,
	PrimaryBufferImpl_GetCaps,
	PrimaryBufferImpl_GetCurrentPosition,
	PrimaryBufferImpl_GetFormat,
	PrimaryBufferImpl_GetVolume,
	PrimaryBufferImpl_GetPan,
        PrimaryBufferImpl_GetFrequency,
	PrimaryBufferImpl_GetStatus,
	PrimaryBufferImpl_Initialize,
	PrimaryBufferImpl_Lock,
	PrimaryBufferImpl_Play,
	PrimaryBufferImpl_SetCurrentPosition,
	PrimaryBufferImpl_SetFormat,
	PrimaryBufferImpl_SetVolume,
	PrimaryBufferImpl_SetPan,
	PrimaryBufferImpl_SetFrequency,
	PrimaryBufferImpl_Stop,
	PrimaryBufferImpl_Unlock,
	PrimaryBufferImpl_Restore
};

HRESULT PrimaryBufferImpl_Create(
	DirectSoundDevice * device,
	PrimaryBufferImpl ** ppdsb,
	LPCDSBUFFERDESC dsbd)
{
	PrimaryBufferImpl *dsb;
	TRACE("%p,%p,%p)\n",device,ppdsb,dsbd);

	if (dsbd->lpwfxFormat) {
		WARN("invalid parameter: dsbd->lpwfxFormat != NULL\n");
		*ppdsb = NULL;
		return DSERR_INVALIDPARAM;
	}

	dsb = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*dsb));

	if (dsb == NULL) {
		WARN("out of memory\n");
		*ppdsb = NULL;
		return DSERR_OUTOFMEMORY;
	}

	dsb->ref = 0;
	dsb->device = device;
	dsb->lpVtbl = &dspbvt;

	CopyMemory(&device->dsbd, dsbd, sizeof(*dsbd));

	TRACE("Created primary buffer at %p\n", dsb);
	TRACE("(formattag=0x%04x,chans=%d,samplerate=%d,"
		"bytespersec=%d,blockalign=%d,bitspersamp=%d,cbSize=%d)\n",
		device->pwfx->wFormatTag, device->pwfx->nChannels,
                device->pwfx->nSamplesPerSec, device->pwfx->nAvgBytesPerSec,
                device->pwfx->nBlockAlign, device->pwfx->wBitsPerSample,
                device->pwfx->cbSize);

	*ppdsb = dsb;
	return S_OK;
}
