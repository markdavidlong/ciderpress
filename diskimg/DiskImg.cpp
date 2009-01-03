/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of the DiskImg class.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"
#include "TwoImg.h"


/*
 * ===========================================================================
 *		DiskImg
 * ===========================================================================
 */

/*
 * Standard NibbleDescr profiles.
 *
 * These will be tried in the order in which they appear here.
 *
 * IMPORTANT: if you add or remove an entry, update the StdNibbleDescr enum
 * in DiskImg.h.
 *
 * Formats that allow the data checksum to be ignored should NOT be written.
 * It's possible that the DOS on the disk is ignoring the checksums, but
 * it's more likely that they're using a non-standard seed, and the newly-
 * written sectors will have the wrong checksum value.
 *
 * Non-standard headers are usually okay, because we don't rewrite the
 * headers, just the sector contents.
 */
/*static*/ const DiskImg::NibbleDescr DiskImg::kStdNibbleDescrs[] = {
    {
        "DOS 3.3 Standard",
        16,
        { 0xd5, 0xaa, 0x96 }, { 0xde, 0xaa, 0xeb },
        0x00,   // checksum seed
        true,   // verify checksum
        true,   // verify track
        2,      // epilog verify count
        { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
        0x00,   // checksum seed
        true,   // verify checksum
        2,      // epilog verify count
        kNibbleEnc62,
        kNibbleSpecialNone,
    },
    {
        "DOS 3.3 Patched",
        16,
        { 0xd5, 0xaa, 0x96 }, { 0xde, 0xaa, 0xeb },
        0x00,   // checksum seed
        false,  // verify checksum
        false,  // verify track
        0,      // epilog verify count
        { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
        0x00,   // checksum seed
        true,   // verify checksum
        0,      // epilog verify count
        kNibbleEnc62,
        kNibbleSpecialNone,
    },
    {
        "DOS 3.3 Ignore Checksum",
        16,
        { 0xd5, 0xaa, 0x96 }, { 0xde, 0xaa, 0xeb },
        0x00,   // checksum seed
        false,  // verify checksum
        false,  // verify track
        0,      // epilog verify count
        { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
        0x00,   // checksum seed
        false,  // verify checksum
        0,      // epilog verify count
        kNibbleEnc62,
        kNibbleSpecialNone,
    },
    {
        "DOS 3.2 Standard",
        13,
        { 0xd5, 0xaa, 0xb5 }, { 0xde, 0xaa, 0xeb },
        0x00,
        true,
        true,
        2,
        { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
        0x00,
        true,
        2,
        kNibbleEnc53,
        kNibbleSpecialNone,
    },
    {
        "DOS 3.2 Patched",
        13,
        { 0xd5, 0xaa, 0xb5 }, { 0xde, 0xaa, 0xeb },
        0x00,
        false,
        false,
        0,
        { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
        0x00,
        true,
        0,
        kNibbleEnc53,
        kNibbleSpecialNone,
    },
    {
        "Muse DOS 3.2",     // standard DOS 3.2 with doubled sectors
        13,
        { 0xd5, 0xaa, 0xb5 }, { 0xde, 0xaa, 0xeb },
        0x00,
        true,
        true,
        2,
        { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
        0x00,
        true,
        2,
        kNibbleEnc53,
        kNibbleSpecialMuse,
    },
    {
        "RDOS 3.3",         // SSI 16-sector RDOS, with altered headers
        16,
        { 0xd4, 0xaa, 0x96 }, { 0xde, 0xaa, 0xeb },
        0x00,
        true,
        true,
        0,		// epilog verify count
        { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
        0x00,
        true,
        2,
        kNibbleEnc62,
        kNibbleSpecialSkipFirstAddrByte,
		/* odd tracks use d4aa96, even tracks use d5aa96 */
    },
    {
        "RDOS 3.2",         // SSI 13-sector RDOS, with altered headers
        13,
        { 0xd4, 0xaa, 0xb7 }, { 0xde, 0xaa, 0xeb },
        0x00,
        true,
        true,
        2,
        { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
        0x00,
        true,
        2,
        kNibbleEnc53,
        kNibbleSpecialNone,
    },
	{
		"Custom",	// reserve space for empty slot
		0,
	},
};
/*static*/ const DiskImg::NibbleDescr*
DiskImg::GetStdNibbleDescr(StdNibbleDescr idx)
{
    if ((int)idx < 0 || (int)idx >= (int) NELEM(kStdNibbleDescrs))
        return nil;
    return &kStdNibbleDescrs[(int)idx];
}


/*
 * Initialize the members during construction.
 */
DiskImg::DiskImg(void)
{
    assert(Global::GetAppInitCalled());

    fOuterFormat = kOuterFormatUnknown;
	fFileFormat = kFileFormatUnknown;
	fPhysical = kPhysicalFormatUnknown;
	fpNibbleDescr = nil;
	fOrder = kSectorOrderUnknown;
	fFormat = kFormatUnknown;

	fFileSysOrder = kSectorOrderUnknown;
	fSectorPairing = false;
	fSectorPairOffset = -1;

	fpOuterGFD = nil;
	fpWrapperGFD = nil;
	fpDataGFD = nil;
	fpOuterWrapper = nil;
	fpImageWrapper = nil;
	fpParentImg = nil;
	fDOSVolumeNum = kVolumeNumNotSet;
    fOuterLength = -1;
	fWrappedLength = -1;
	fLength = -1;
    fExpandable = false;
	fReadOnly = true;
    fDirty = false;

    fHasSectors = false;
    fHasBlocks = false;
    fHasNibbles = false;

	fNumTracks = -1;
	fNumSectPerTrack = -1;
	fNumBlocks = -1;

	fpScanProgressCallback = NULL;

	/*
	 * Create a working copy of the nibble descr table.  We want to leave
	 * open the possibility of applications editing or discarding entries,
	 * so we work off of a copy.
	 *
	 * Ideally we'd allow these to be set per-track, so that certain odd
	 * formats could be handled transparently (e.g. Muse tweaked DOS 3.2)
	 * for formatting as well as reading.
	 */
	assert(kStdNibbleDescrs[kNibbleDescrCustom].numSectors == 0);
	assert(kNibbleDescrCustom == NELEM(kStdNibbleDescrs)-1);
    fpNibbleDescrTable = new NibbleDescr[NELEM(kStdNibbleDescrs)];
    fNumNibbleDescrEntries = NELEM(kStdNibbleDescrs);
    memcpy(fpNibbleDescrTable, kStdNibbleDescrs, sizeof(kStdNibbleDescrs));

    fNibbleTrackBuf = nil;
    fNibbleTrackLoaded = -1;

	fNuFXCompressType = kNuThreadFormatLZW2;

	fNotes = nil;
	fpBadBlockMap = nil;
	fDiskFSRefCnt = 0;
}

/*
 * Throw away local storage.
 */
DiskImg::~DiskImg(void)
{
	if (fpDataGFD != nil) {
		WMSG0("~DiskImg closing GenericFD(s)\n");
	}
	(void) CloseImage();
	delete[] fpNibbleDescrTable;
    delete[] fNibbleTrackBuf;
	delete[] fNotes;
	delete fpBadBlockMap;

    /* normally these will be closed, but perhaps not if something failed */
    if (fpOuterGFD != nil)
        delete fpOuterGFD;
    if (fpWrapperGFD != nil)
        delete fpWrapperGFD;
    if (fpDataGFD != nil)
        delete fpDataGFD;
    if (fpOuterWrapper != nil)
        delete fpOuterWrapper;
    if (fpImageWrapper != nil)
        delete fpImageWrapper;

	fDiskFSRefCnt = 100;	// flag as freed
}


/*
 * Set the nibble descr pointer.
 */
void
DiskImg::SetNibbleDescr(int idx)
{
	assert(idx >= 0 && idx < kNibbleDescrMAX);
	fpNibbleDescr = &fpNibbleDescrTable[idx];
}

/*
 * Set up a custom nibble descriptor.
 */
void
DiskImg::SetCustomNibbleDescr(const NibbleDescr* pDescr)
{
	if (pDescr == NULL) {
		fpNibbleDescr = NULL;
	} else {
		assert(fpNibbleDescrTable != NULL);
		//WMSG2("Overwriting entry %d with new value (special=%d)\n",
		//	kNibbleDescrCustom, pDescr->special);
		fpNibbleDescrTable[kNibbleDescrCustom] = *pDescr;
		fpNibbleDescr = &fpNibbleDescrTable[kNibbleDescrCustom];
	}
}


/*
 * Open a volume or a file on disk.
 *
 * For Windows, we need to handle logical/physical volumes specially.  If
 * the filename matches the appropriate pattern, use a different GFD.
 */
DIError
DiskImg::OpenImage(const char* pathName, char fssep, bool readOnly)
{
	DIError dierr = kDIErrNone;
	bool isWinDevice = false;

	if (fpDataGFD != nil) {
		WMSG0(" DI already open!\n");
		return kDIErrAlreadyOpen;
	}
	WMSG3(" DI OpenImage '%s' '%.1s' ro=%d\n", pathName, &fssep, readOnly);

	fReadOnly = readOnly;

#ifdef _WIN32
	if ((fssep == '\0' || fssep == '\\') &&
		pathName[0] >= 'A' && pathName[0] <= 'Z' &&
		pathName[1] == ':' && pathName[2] == '\\' &&
		pathName[3] == '\0')
	{
		isWinDevice = true;		// logical volume ("A:\")
	}
	if ((fssep == '\0' || fssep == '\\') &&
		isdigit(pathName[0]) && isdigit(pathName[1]) &&
		pathName[2] == ':' && pathName[3] == '\\' &&
		pathName[4] == '\0')
	{
		isWinDevice = true;		// physical volume ("80:\")
	}
	if ((fssep == '\0' || fssep == '\\') &&
		strncmp(pathName, kASPIDev, strlen(kASPIDev)) == 0 &&
		pathName[strlen(pathName)-1] == '\\')
	{
		isWinDevice = true;		// ASPI volume ("ASPI:x:y:z\")
	}
#endif

	if (isWinDevice) {
#ifdef _WIN32
		GFDWinVolume* pGFDWinVolume = new GFDWinVolume;

		dierr = pGFDWinVolume->Open(pathName, fReadOnly);
		if (dierr != kDIErrNone) {
			delete pGFDWinVolume;
			goto bail;
		}

		fpWrapperGFD = pGFDWinVolume;
		// Use a unique extension to skip some of the probing.
		dierr = AnalyzeImageFile("CPDevice.cp-win-vol", '\0');
		if (dierr != kDIErrNone)
			goto bail;
#endif
	} else {
		GFDFile* pGFDFile = new GFDFile;

		dierr = pGFDFile->Open(pathName, fReadOnly);
		if (dierr != kDIErrNone) {
			delete pGFDFile;
			goto bail;
		}

		//fImageFileName = new char[strlen(pathName) + 1];
		//strcpy(fImageFileName, pathName);

		fpWrapperGFD = pGFDFile;
		pGFDFile = nil;

		dierr = AnalyzeImageFile(pathName, fssep);
		if (dierr != kDIErrNone)
			goto bail;
	}


    assert(fpDataGFD != nil);

bail:
	return dierr;
}

/*
 * Open from a buffer, which could point to unadorned ready-to-go content
 * or to a preloaded image file.
 */
DIError
DiskImg::OpenImage(const void* buffer, long length, bool readOnly)
{
	if (fpDataGFD != nil) {
		WMSG0(" DI already open!\n");
		return kDIErrAlreadyOpen;
	}
	WMSG3(" DI OpenImage %08lx %ld ro=%d\n", (long) buffer, length, readOnly);

	DIError dierr;
	GFDBuffer* pGFDBuffer;

	fReadOnly = readOnly;
	pGFDBuffer = new GFDBuffer;

	dierr = pGFDBuffer->Open(const_cast<void*>(buffer), length, false, false,
				readOnly);
	if (dierr != kDIErrNone) {
		delete pGFDBuffer;
		return dierr;
	}

	fpWrapperGFD = pGFDBuffer;
	pGFDBuffer = nil;

	dierr = AnalyzeImageFile("", '\0');
	if (dierr != kDIErrNone)
		return dierr;

    assert(fpDataGFD != nil);
	return kDIErrNone;
}

/*
 * Open a range of blocks from an already-open disk image.  This is only
 * useful for things like UNIDOS volumes, which don't have an associated
 * file in the image and are linear.
 *
 * The "read only" flag is inherited from the parent.
 *
 * For embedded images with visible file structure, we should be using
 * an EmbeddedFD instead.  [Note these were never implemented.]
 *
 * NOTE: there is an implicit ProDOS block ordering imposed on the parent
 * image.  It turns out that all of our current embedded parents use
 * ProDOS-ordered blocks, so it works out okay, but the "linear" requirement
 * above goes beyond just having contiguous blocks.
 */
DIError
DiskImg::OpenImage(DiskImg* pParent, long firstBlock, long numBlocks)
{
	WMSG3(" DI OpenImage parent=0x%08lx %ld %ld\n", (long) pParent, firstBlock,
		numBlocks);
	if (fpDataGFD != nil) {
		WMSG0(" DI already open!\n");
		return kDIErrAlreadyOpen;
	}

	if (pParent == nil || firstBlock < 0 || numBlocks <= 0 ||
		firstBlock + numBlocks > pParent->GetNumBlocks())
	{
		assert(false);
		return kDIErrInvalidArg;
	}

	fReadOnly = pParent->GetReadOnly();		// very important

    DIError dierr;
    GFDGFD* pGFDGFD;

    pGFDGFD = new GFDGFD;
    dierr = pGFDGFD->Open(pParent->fpDataGFD, firstBlock * kBlockSize, fReadOnly);
    if (dierr != kDIErrNone) {
        delete pGFDGFD;
        return dierr;
    }

    fpDataGFD = pGFDGFD;
    assert(fpWrapperGFD == nil);

	/*
	 * This replaces the call to "analyze image file" because we know we
	 * already have an open file with specific characteristics.
	 */
	//fOffset = pParent->fOffset + kBlockSize * firstBlock;
	fLength = numBlocks * kBlockSize;
    fOuterLength = fWrappedLength = fLength;
	fFileFormat = kFileFormatUnadorned;
	fPhysical = pParent->fPhysical;
	fOrder = pParent->fOrder;

	fpParentImg = pParent;

    return dierr;
}
DIError
DiskImg::OpenImage(DiskImg* pParent, long firstTrack, long firstSector,
	long numSectors)
{
	WMSG4(" DI OpenImage parent=0x%08lx %ld %ld %ld\n", (long) pParent,
        firstTrack, firstSector, numSectors);
	if (fpDataGFD != nil) {
		WMSG0(" DI already open!\n");
		return kDIErrAlreadyOpen;
	}

	if (pParent == nil)
		return kDIErrInvalidArg;

	int prntSectPerTrack = pParent->GetNumSectPerTrack();
	int lastTrack = firstTrack +
						(numSectors + prntSectPerTrack-1) / prntSectPerTrack;
	if (firstTrack < 0 || numSectors <= 0 ||
		lastTrack > pParent->GetNumTracks())
	{
		return kDIErrInvalidArg;
	}

	fReadOnly = pParent->GetReadOnly();		// very important

	DIError dierr;
	GFDGFD* pGFDGFD;

	pGFDGFD = new GFDGFD;
	dierr = pGFDGFD->Open(pParent->fpDataGFD,
                kSectorSize * firstTrack * prntSectPerTrack, fReadOnly);
	if (dierr != kDIErrNone) {
		delete pGFDGFD;
		return dierr;
	}
	
	fpDataGFD = pGFDGFD;
    assert(fpWrapperGFD == nil);

	/*
	 * This replaces the call to "analyze image file" because we know we
	 * already have an open file with specific characteristics.
	 */
	assert(firstSector == 0);	// else fOffset calculation breaks
	//fOffset = pParent->fOffset + kSectorSize * firstTrack * prntSectPerTrack;
	fLength = numSectors * kSectorSize;
    fOuterLength = fWrappedLength = fLength;
	fFileFormat = kFileFormatUnadorned;
	fPhysical = pParent->fPhysical;
	fOrder = pParent->fOrder;

	fpParentImg = pParent;

	return dierr;
}


/*
 * Enable sector pairing.  Useful for OzDOS.
 */
void
DiskImg::SetPairedSectors(bool enable, int idx)
{
	fSectorPairing = enable;
	fSectorPairOffset = idx;

	if (enable) {
		assert(idx == 0 || idx == 1);
	}
}

/*
 * Close the image, freeing resources.
 *
 * If we write to a child DiskImg, it's responsible for setting the "dirty"
 * flag in its parent (and so on up the chain).  That's necessary so that,
 * when we close the file, changes made to a child DiskImg cause the parent
 * to do any necessary recompression.
 *
 * [ This is getting called even when image creation failed with an error.
 * This is probably the correct behavior, but we may want to be aborting the
 * image creation instead of completing it.  That's a higher-level decision
 * though. ++ATM 20040506 ]
 */
DIError
DiskImg::CloseImage(void)
{
	DIError dierr;

	WMSG1("CloseImage %p\n", this);

	/* check for DiskFS objects that still point to us */
	if (fDiskFSRefCnt != 0) {
		WMSG1("ERROR: CloseImage: fDiskFSRefCnt=%d\n", fDiskFSRefCnt);
		assert(false); //DebugBreak();
	}

	/*
	 * Flush any changes.
	 */
	dierr = FlushImage(kFlushAll);
	if (dierr != kDIErrNone)
		return dierr;

    /*
     * Clean up.  Close GFD, OrigGFD, and OuterGFD.  Delete ImageWrapper
     * and OuterWrapper.
     *
     * In some cases we will have the file open more than once (e.g. a
     * NuFX archive, which must be opened on disk).
     */
	if (fpDataGFD != nil) {
		fpDataGFD->Close();
		delete fpDataGFD;
        fpDataGFD = nil;
	}
	if (fpWrapperGFD != nil) {
		fpWrapperGFD->Close();
		delete fpWrapperGFD;
        fpWrapperGFD = nil;
	}
	if (fpOuterGFD != nil) {
		fpOuterGFD->Close();
		delete fpOuterGFD;
        fpOuterGFD = nil;
	}
	delete fpImageWrapper;
    fpImageWrapper = nil;
	delete fpOuterWrapper;
    fpOuterWrapper = nil;

	return dierr;
}


/*
 * Flush data to disk.
 *
 * The only time this really needs to do anything on a disk image file is
 * when we have compressed data (NuFX, DDD, .gz, .zip).  The uncompressed
 * wrappers either don't do anything ("unadorned") or just update some
 * header fields (DiskCopy42).
 *
 * If "mode" is kFlushFastOnly, we only flush the formats that don't really
 * need flushing.  This is part of a scheme to keep the disk contents in a
 * reasonable state on the off chance we crash with a modified file open.
 * It also helps the user understand when changes are being made immediately
 * vs. when they're written to memory and compressed later.  We could just
 * refuse to raise the "dirty" flag when modifying "simple" file formats,
 * but that would change the meaning of the flag from "something has been
 * changed" to "what's in the file and what's in memory differ".  I want it
 * to be a "dirty" flag.
 */
DIError
DiskImg::FlushImage(FlushMode mode)
{
	DIError dierr = kDIErrNone;

    WMSG2(" DI FlushImage (dirty=%d mode=%d)\n", fDirty, mode);
	if (!fDirty)
		return kDIErrNone;
	if (fpDataGFD == nil) {
		/*
		 * This can happen if we tried to create a disk image but failed, e.g.
		 * couldn't create the output file because of access denied on the
		 * directory.  There's no data, therefore nothing to flush, but the
		 * "dirty" flag is set because CreateImageCommon sets it almost
		 * immediately.
		 */
		WMSG0("  (disk must've failed during creation)\n");
		fDirty = false;
		return kDIErrNone;
	}

	if (mode == kFlushFastOnly &&
		((fpImageWrapper != nil && !fpImageWrapper->HasFastFlush()) ||
		 (fpOuterWrapper != nil && !fpOuterWrapper->HasFastFlush()) ))
	{
		WMSG0("DI fast flush requested, but one or both wrappers are slow\n");
		return kDIErrNone;
	}

    /*
     * Step 1: make sure any local caches have been flushed.
     */
    /* (none) */

    /*
     * Step 2: push changes from fpDataGFD to fpWrapperGFD.  This will
     * cause ImageWrapper to rebuild itself (SHK, DDD, whatever).  In
     * some cases this amounts to copying the data on top of itself,
     * which we can avoid easily.
	 *
	 * Embedded volumes don't have wrappers; when you write to an
	 * embedded volume, it passes straight through to the parent.
	 *
	 * (Note to self: formats like NuFX that write to a temp file and then
	 * rename over the old will close fpWrapperGFD and just access it
	 * directly.  This is bad, because it doesn't allow them to have an
	 * "outer" format, but it's the way life is.  The point is that it's
	 * okay for fpWrapperGFD to be non-nil but represent a closed file,
	 * so long as the "Flush" function has it figured out.)
     */
	if (fpWrapperGFD != nil) {
		WMSG2(" DI flushing data changes to wrapper (fLen=%ld fWrapLen=%ld)\n",
			(long) fLength, (long) fWrappedLength);
		dierr = fpImageWrapper->Flush(fpWrapperGFD, fpDataGFD, fLength,
					&fWrappedLength);
		if (dierr != kDIErrNone) {
			WMSG1(" ERROR: wrapper flush failed (err=%d)\n", dierr);
			return dierr;
		}
		/* flush the GFD in case it's a Win32 volume with block caching */
		dierr = fpWrapperGFD->Flush();
	} else {
		assert(fpParentImg != nil);
	}

    /*
     * Step 3: if we have an fpOuterGFD, rebuild the file with the data
     * in fpWrapperGFD.
     */
    if (fpOuterWrapper != nil) {
        WMSG1(" DI saving wrapper to outer, fWrapLen=%ld\n",
            (long) fWrappedLength);
        assert(fpOuterGFD != nil);
        dierr = fpOuterWrapper->Save(fpOuterGFD, fpWrapperGFD,
                    fWrappedLength);
        if (dierr != kDIErrNone) {
            WMSG1(" ERROR: outer save failed (err=%d)\n", dierr);
            return dierr;
        }
    }

    fDirty = false;
	return kDIErrNone;
}



/*
 * Given the filename extension and a GFD, figure out what's inside.
 *
 * The filename extension should give us some idea what to expect:
 *	SHK, SDK, BXY - ShrinkIt compressed disk image
 *  GZ - gzip-compressed file (with something else inside)
 *	ZIP - ZIP archive with a single disk image inside
 *	DDD - DDD, DDD Pro, or DDD5.0 compressed image
 *	DSK - DiskCopy 4.2 or DO/PO
 *  DC - DiskCopy 4.2 (or 6?)
 *  DC6 - DiskCopy 6 (usually just raw sectors)
 *	DO, PO, D13, RAW? - DOS-order or ProDOS-order uncompressed
 *	IMG - Copy ][+ image (unadorned, physical sector order)
 *	HDV - virtual hard drive image
 *	NIB, RAW? - nibblized image
 *	(no extension) uncompressed
 *	cp-win-vol - our "magic" extension to indicate a Windows logical volume
 *
 * We can also examine the file length to see if it's a standard size
 * (140K, 800K) and look for magic values in the header.
 *
 * If we can access the contents directly from disk, we do so.  It's
 * possibly more efficient to load the whole thing into memory, but if
 * we have that much memory then the OS should cache it for us.  (I have
 * some 20MB disk images from my hard drive that shouldn't be loaded
 * in their entirety.  Certainly don't want to load a 512MB CFFA image.)
 *
 * On input, the following fields must be set:
 *  fpWrapperGFD - GenericFD for the file pointed to by "pathname" (or for a
 *            memory buffer if this is a sub-volume)
 *
 * On success, the following fields will be set:
 *  fWrappedLength, fOuterLength - set appropriately
 *	fpDataGFD - GFD for the raw data, possibly just a GFDGFD with an offset
 *	fLength - length of unadorned data in the file, or the length of
 *	          data stored in fBuffer (test for fBuffer!=nil)
 *	fFileFormat - set to the overall file format, mostly interesting
 *	          for identification of the file "wrapper"
 *	fPhysicalFormat - set to the type of data this holds
 *	(maybe) fOrder - set when the file format or extension dictates, e.g.
 *	           2MG or *.po; not always reliable
 *  (maybe) fDOSVolumeNum - set to DOS volume number from wrapper
 *
 * This may set fReadOnly if one of the wrappers looks okay but is reporting
 * a bad checksum.
 */
DIError
DiskImg::AnalyzeImageFile(const char* pathName, char fssep)
{
	DIError dierr = kDIErrNone;
	FileFormat probableFormat;
	bool reliableExt;
    const char* ext = FindExtension(pathName, fssep);
    char* extBuf = nil;		// uses malloc/free
	bool needExtFromOuter = false;

    if (ext != nil) {
        assert(*ext == '.');
        ext++;
    } else
        ext = "";

	WMSG1(" DI AnalyzeImageFile ext='%s'\n", ext);

	/* sanity check: nobody should have configured these yet */
	assert(fOuterFormat == kOuterFormatUnknown);
	assert(fFileFormat == kFileFormatUnknown);
	assert(fOrder == kSectorOrderUnknown);
	assert(fFormat == kFormatUnknown);
	fLength = -1;
	dierr = fpWrapperGFD->Seek(0, kSeekEnd);
	if (dierr != kDIErrNone) {
		WMSG0("  DI Couldn't seek to end of wrapperGFD\n");
		goto bail;
	}
	fWrappedLength = fOuterLength = fpWrapperGFD->Tell();

	/* quick test for zero-length files */
	if (fWrappedLength == 0)
		return kDIErrUnrecognizedFileFmt;

    /*
     * Start by checking for a zip/gzip "wrapper wrapper".  We want to strip
     * that away before we do anything else.  Because web sites tend to
     * gzip everything in sight whether it needs it or not, we treat this
     * as a special case and assume that anything could be inside.
     *
     * Some cases are difficult to handle, e.g. ".SDK", since NufxLib
     * doesn't let us open an archive that is sitting in memory.
     *
     * We could also handle disk images stored as ordinary files stored
     * inside SHK.  Not much point in handling multiple files down at
     * this level though.
     */
    if (strcasecmp(ext, "gz") == 0 &&
        OuterGzip::Test(fpWrapperGFD, fOuterLength) == kDIErrNone)
    {
        WMSG0("  DI found gz outer wrapper\n");

		fpOuterWrapper = new OuterGzip();
        if (fpOuterWrapper == nil) {
            dierr = kDIErrMalloc;
            goto bail;
        }
		fOuterFormat = kOuterFormatGzip;

		/* drop the ".gz" and get down to the next extension */
        ext = "";
        extBuf = strdup(pathName);
        if (extBuf != nil) {
            char* localExt;

            localExt = (char*) FindExtension(extBuf, fssep);
            if (localExt != nil)
                *localExt = '\0';
            localExt = (char*) FindExtension(extBuf, fssep);
            if (localExt != nil) {
                ext = localExt;
                assert(*ext == '.');
                ext++;
            }
        }
        WMSG1("  DI after gz, ext='%s'\n", ext == nil ? "(nil)" : ext);

	} else if (strcasecmp(ext, "zip") == 0) {
		dierr = OuterZip::Test(fpWrapperGFD, fOuterLength);
		if (dierr != kDIErrNone)
			goto bail;

        WMSG0("  DI found ZIP outer wrapper\n");

		fpOuterWrapper = new OuterZip();
        if (fpOuterWrapper == nil) {
            dierr = kDIErrMalloc;
            goto bail;
        }
		fOuterFormat = kOuterFormatZip;

		needExtFromOuter = true;

    } else {
		fOuterFormat = kOuterFormatNone;
	}

	/* finish up outer wrapper stuff */
	if (fOuterFormat != kOuterFormatNone) {
        GenericFD* pNewGFD = nil;
        dierr = fpOuterWrapper->Load(fpWrapperGFD, fOuterLength, fReadOnly,
                    &fWrappedLength, &pNewGFD);
        if (dierr != kDIErrNone) {
            WMSG0("  DI outer prep failed\n");
            /* extensions are "reliable", so failure is unavoidable */
            goto bail;
        }

		/* Load() sets this */
		if (fpOuterWrapper->IsDamaged()) {
			AddNote(kNoteWarning, "The zip/gzip wrapper appears to be damaged.");
			fReadOnly = true;
		}

        /* shift GFDs */
        fpOuterGFD = fpWrapperGFD;
        fpWrapperGFD = pNewGFD;

		if (needExtFromOuter) {
			ext = fpOuterWrapper->GetExtension();
			if (ext == nil)
				ext = "";
		}
	}

	/*
	 * Try to figure out what format the file is in.
	 *
	 * First pass, try only what the filename says it is.  This way, if
	 * two file formats look alike, we have a good chance of getting it
	 * right.
	 *
	 * The "Test" functions have the complete file at their disposal.  The
	 * file's length is stored in "fWrappedLength" for convenience.
	 */
	reliableExt = false;
	probableFormat = kFileFormatUnknown;
	if (strcasecmp(ext, "2mg") == 0 || strcasecmp(ext, "2img") == 0) {
		reliableExt = true;
		if (Wrapper2MG::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
			probableFormat = kFileFormat2MG;
	} else if (strcasecmp(ext, "shk") == 0 || strcasecmp(ext, "sdk") == 0 ||
		strcasecmp(ext, "bxy") == 0)
	{
		DIError dierr2;
		reliableExt = true;
		dierr2 = WrapperNuFX::Test(fpWrapperGFD, fWrappedLength);
		if (dierr2 == kDIErrNone)
			probableFormat = kFileFormatNuFX;
		else if (dierr2 == kDIErrFileArchive) {
			WMSG0(" AnalyzeImageFile thinks it found a NuFX file archive\n");
			dierr = dierr2;
			goto bail;
		}
    } else if (strcasecmp(ext, "hdv") == 0) {
		/* usually just a "raw" disk, but check for Sim //e */
        if (WrapperSim2eHDV::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
            probableFormat = kFileFormatSim2eHDV;

        /* ProDOS .hdv volumes can expand */
        fExpandable = true;
	} else if (strcasecmp(ext, "dsk") == 0 || strcasecmp(ext, "dc") == 0) {
        /* might be DiskCopy */
		if (WrapperDiskCopy42::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
			probableFormat = kFileFormatDiskCopy42;
	} else if (strcasecmp(ext, "ddd") == 0) {
        /* do this after compressed formats but before unadorned */
		reliableExt = true;
		if (WrapperDDD::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
			probableFormat = kFileFormatDDD;
	} else if (strcasecmp(ext, "app") == 0) {
		reliableExt = true;
		if (WrapperTrackStar::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
			probableFormat = kFileFormatTrackStar;
	} else if (strcasecmp(ext, "fdi") == 0) {
		reliableExt = true;
		if (WrapperFDI::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
			probableFormat = kFileFormatFDI;
	} else if (strcasecmp(ext, "img") == 0) {
		if (WrapperUnadornedSector::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
        {
			probableFormat = kFileFormatUnadorned;
			fPhysical = kPhysicalFormatSectors;
			fOrder = kSectorOrderPhysical;
		}
	} else if (strcasecmp(ext, "nib") == 0 || strcasecmp(ext, "raw") == 0) {
		if (WrapperUnadornedNibble::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
        {
			probableFormat = kFileFormatUnadorned;
			fPhysical = kPhysicalFormatNib525_6656;
			/* figure out NibbleFormat later */
		}
	} else if (strcasecmp(ext, "do") == 0 || strcasecmp(ext, "po") == 0 ||
        strcasecmp(ext, "d13") == 0 || strcasecmp(ext, "dc6") == 0)
    {
		if (WrapperUnadornedSector::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
        {
			probableFormat = kFileFormatUnadorned;
			fPhysical = kPhysicalFormatSectors;
			if (strcasecmp(ext, "do") == 0 || strcasecmp(ext, "d13") == 0)
				fOrder = kSectorOrderDOS;
			else
				fOrder = kSectorOrderProDOS;    // po, dc6
            WMSG1("  DI guessing order is %d by extension\n", fOrder);
		}
	} else if (strcasecmp(ext, "cp-win-vol") == 0) {
		/* this is a Windows logical volume */
		reliableExt = true;
		probableFormat = kFileFormatUnadorned;
		fPhysical = kPhysicalFormatSectors;
		fOrder = kSectorOrderProDOS;
	} else {
		/* no match on the filename extension; start guessing */
	}

	if (probableFormat != kFileFormatUnknown) {
		/*
		 * Found a match.  Use "probableFormat" to open the file.
		 */
		WMSG1(" DI scored hit on extension '%s'\n", ext);
	} else {
		/*
		 * Didn't work.  If the file extension was marked "reliable", then
		 * either we have the wrong extension on the file, or the contents
		 * are damaged.
		 *
		 * If the extension isn't reliable, or simply absent, then we have
		 * to probe through the formats we know and just hope for the best.
		 *
		 * If the "test" function returns with a checksum failure, we take
		 * it to mean that the format was positively identified, but the
		 * data inside is corrupted.  This results in an immediate return
		 * with the checksum failure noted.  Only a few wrapper formats
		 * have checksums embedded.  (The "test" functions should only
         * be looking at header checksums.)
		 */
		if (reliableExt) {
			WMSG1(" DI file extension '%s' did not match contents\n", ext);
			dierr = kDIErrBadFileFormat;
			goto bail;
		} else {
			WMSG1(" DI extension '%s' not useful, probing formats\n", ext);
			dierr = WrapperNuFX::Test(fpWrapperGFD, fWrappedLength);
			if (dierr == kDIErrNone) {
				probableFormat = kFileFormatNuFX;
				goto gotit;
			} else if (dierr == kDIErrFileArchive)
				goto bail;		// we know it's NuFX, we know we can't use it
			else if (dierr == kDIErrBadChecksum)
				goto bail;		// right file type, bad data

			dierr = WrapperDiskCopy42::Test(fpWrapperGFD, fWrappedLength);
			if (dierr == kDIErrNone) {
				probableFormat = kFileFormatDiskCopy42;
				goto gotit;
			} else if (dierr == kDIErrBadChecksum)
				goto bail;		// right file type, bad data

			if (Wrapper2MG::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone) {
				probableFormat = kFileFormat2MG;
			} else if (WrapperDDD::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone) {
				probableFormat = kFileFormatDDD;
            } else if (WrapperSim2eHDV::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
            {
                probableFormat = kFileFormatSim2eHDV;
			} else if (WrapperTrackStar::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
			{
				probableFormat = kFileFormatTrackStar;
			} else if (WrapperFDI::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone)
			{
				probableFormat = kFileFormatFDI;
			} else if (WrapperUnadornedNibble::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone) {
				probableFormat = kFileFormatUnadorned;
				fPhysical = kPhysicalFormatNib525_6656;  // placeholder
			} else if (WrapperUnadornedSector::Test(fpWrapperGFD, fWrappedLength) == kDIErrNone) {
				probableFormat = kFileFormatUnadorned;
				fPhysical = kPhysicalFormatSectors;
			}
gotit: ;
		}
	}

	/*
	 * Either we recognize it or we don't.  Finish opening the file by
	 * setting up "fLength" and "fPhysical" values, extracting data
	 * into a memory buffer if necessary.  fpDataGFD is set up by the
     * "prep" function.
	 *
	 * If we're lucky, this will also configure "fOrder" for us, which is
	 * important when we can't recognize the filesystem format (for correct
	 * operation of disk tools).
	 */
	switch (probableFormat) {
	case kFileFormat2MG:
		fpImageWrapper = new Wrapper2MG();
		break;
	case kFileFormatDiskCopy42:
		fpImageWrapper = new WrapperDiskCopy42();
		break;
	case kFileFormatSim2eHDV:
		fpImageWrapper = new WrapperSim2eHDV();
		break;
	case kFileFormatTrackStar:
		fpImageWrapper = new WrapperTrackStar();
		break;
	case kFileFormatFDI:
		fpImageWrapper = new WrapperFDI();
		fReadOnly = true;		// writing to FDI not yet supported
		break;
	case kFileFormatNuFX:
		fpImageWrapper = new WrapperNuFX();
        ((WrapperNuFX*)fpImageWrapper)->SetCompressType(
										(NuThreadFormat) fNuFXCompressType);
		break;
    case kFileFormatDDD:
		fpImageWrapper = new WrapperDDD();
        break;
	case kFileFormatUnadorned:
		if (IsSectorFormat(fPhysical))
			fpImageWrapper = new WrapperUnadornedSector();
        else if (IsNibbleFormat(fPhysical))
            fpImageWrapper = new WrapperUnadornedNibble();
        else {
            assert(false);
        }
		break;
	default:
		WMSG0(" DI couldn't figure out the file format\n");
		dierr = kDIErrUnrecognizedFileFmt;
		break;
	}
	if (fpImageWrapper != nil) {
        assert(fpDataGFD == nil);
		dierr = fpImageWrapper->Prep(fpWrapperGFD, fWrappedLength, fReadOnly,
					&fLength, &fPhysical, &fOrder, &fDOSVolumeNum,
					&fpBadBlockMap, &fpDataGFD);
	} else {
		/* could be a mem alloc failure that didn't set dierr */
		if (dierr == kDIErrNone)
			dierr = kDIErrGeneric;
	}

	if (dierr != kDIErrNone) {
		WMSG1(" DI wrapper prep failed (err=%d)\n", dierr);
		goto bail;
	}

	/* check for non-fatal checksum failures, e.g. DiskCopy42 */
	if (fpImageWrapper->IsDamaged()) {
		AddNote(kNoteWarning, "File checksum didn't match.");
		fReadOnly = true;
	}

	fFileFormat = probableFormat;

	assert(fLength >= 0);
	assert(fpDataGFD != nil);
	assert(fOuterFormat != kOuterFormatUnknown);
	assert(fFileFormat != kFileFormatUnknown);
	assert(fPhysical != kPhysicalFormatUnknown);

bail:
    free(extBuf);
	return dierr;
}


/*
 * Try to figure out what we're looking at.
 *
 * Returns an error if we don't think this is even a disk image.  If we
 * just can't figure it out, we return success but with the format value
 * set to "unknown".  This gives the caller a chance to use "override"
 * to help us find our way.
 *
 * On entry:
 *	fpDataGFD, fLength, and fFileFormat are defined
 *	fSectorPairing is specified
 *	fOrder has a semi-reliable guess at sector ordering
 * On exit:
 *	fOrder and fFormat are set to the best of our ability
 *	fNumTracks, fNumSectPerTrack, and fNumBlocks are set
 *  fHasSectors, fHasTracks, and fHasNibbles are set
 *	fFileSysOrder is set
 *  fpNibbleDescr will be set for nibble images
 */
DIError
DiskImg::AnalyzeImage(void)
{
	assert(fLength >= 0);
	assert(fpDataGFD != nil);
	assert(fFileFormat != kFileFormatUnknown);
	assert(fPhysical != kPhysicalFormatUnknown);
	assert(fFormat == kFormatUnknown);
	assert(fFileSysOrder == kSectorOrderUnknown);
	assert(fNumTracks == -1);
	assert(fNumSectPerTrack == -1);
	assert(fNumBlocks == -1);
	if (fpDataGFD == nil)
		return kDIErrInternal;

    /*
     * Figure out how many tracks and sectors the image has.
     *
     * For an odd-sized ProDOS image, there will be no tracks and sectors.
     */
	if (IsSectorFormat(fPhysical)) {
        if (!fLength) {
            WMSG0(" DI zero-length disk images not allowed\n");
            return kDIErrOddLength;
        }

		if (fLength == kD13Length) {
			/* 13-sector .d13 image */
			fHasSectors = true;
			fNumSectPerTrack = 13;
			fNumTracks = kTrackCount525;
			assert(!fHasBlocks);
		} else if (fLength % (16 * kSectorSize) == 0) {
            /* looks like a collection of 16-sector tracks */
            fHasSectors = true;

            fNumSectPerTrack = 16;
            fNumTracks = (int) (fLength / (fNumSectPerTrack * kSectorSize));

            /* sector pairing effectively cuts #of tracks in half */
            if (fSectorPairing) {
                if ((fNumTracks & 0x01) != 0) {
                    WMSG0(" DI error: bad attempt at sector pairing\n");
                    assert(false);
                    fSectorPairing = false;
                }
            }

            if (fSectorPairing)
                fNumTracks /= 2;
        } else {
            if (fSectorPairing) {
                WMSG1("GLITCH: sector pairing enabled, but fLength=%ld\n",
                    (long) fLength);
                return kDIErrOddLength;
            }

            assert(fNumTracks == -1);
            assert(fNumSectPerTrack == -1);
			assert((fLength % kBlockSize) == 0);

			fHasBlocks = true;
			fNumBlocks = (long) (fLength / kBlockSize);
        }
    } else if (IsNibbleFormat(fPhysical)) {
        fHasNibbles = fHasSectors = true;

        /*
         * Figure out if it's 13-sector or 16-sector (or garbage).  We
         * have to make an assessment of the entire disk so we can declare
         * it to be 13-sector or 16-sector, which is useful for DiskFS
         * which will want to scan for DOS VTOCs and other goodies.  We
         * also want to provide a default NibbleDescr.
		 *
		 * Failing that, we still allow it to be opened for raw track access.
		 *
		 * This also sets fNumTracks, which could be more than 35 if we're
		 * working with a TrackStar or FDI image.
         */
        DIError dierr;
        dierr = AnalyzeNibbleData();    // sets nibbleDescr and DOS vol num
        if (dierr == kDIErrNone) {
			assert(fpNibbleDescr != nil);
			fNumSectPerTrack = fpNibbleDescr->numSectors;
			fOrder = kSectorOrderPhysical;

			if (!fReadOnly && !fpNibbleDescr->dataVerifyChecksum) {
				WMSG0("DI nibbleDescr does not verify data checksum, disabling writes\n");
				AddNote(kNoteInfo,
					"Sectors use non-standard data checksums; writing disabled.");
				fReadOnly = true;
			}
		} else {
			//assert(fpNibbleDescr == nil);
			fNumSectPerTrack = -1;
			fOrder = kSectorOrderPhysical;
			fHasSectors = false;
		}
    } else {
        WMSG1("Unsupported physical %d\n", fPhysical);
        assert(false);
        return kDIErrGeneric;
    }

    /*
     * Compute the number of blocks.  For a 13-sector disk, block access
     * is not possible.
     *
     * For nibble formats, we have to base the block count on the number
     * of sectors rather than the file length.
     */
    if (fHasSectors) {
        assert(fNumSectPerTrack > 0);
        if ((fNumSectPerTrack & 0x01) == 0) {
            /* not a 13-sector disk, so define blocks in terms of sectors */
			/* (effects of sector pairing are already taken into account) */
            fHasBlocks = true;
            fNumBlocks = (fNumTracks * fNumSectPerTrack) / 2;
        }
    } else if (fHasBlocks) {
        if ((fLength % kBlockSize) == 0) {
            /* not sector-oriented, so define blocks based on length */
            fHasBlocks = true;
            fNumBlocks = (long) (fLength / kBlockSize);

			 if (fSectorPairing) {
				if ((fNumBlocks & 0x01) != 0) {
					WMSG0(" DI error: bad attempt at sector pairing (blk)\n");
					assert(false);
					fSectorPairing = false;
				} else
				   fNumBlocks /= 2;
			}

        } else {
            assert(false);
            return kDIErrGeneric;
        }
	} else if (fHasNibbles) {
		assert(fNumBlocks == -1);
    } else {
		WMSG0(" DI none of fHasSectors/fHasBlocks/fHasNibbles are set\n");
		assert(false);
		return kDIErrInternal;
	}

    /*
     * We've got the track/sector/block layout sorted out; now figure out
     * what kind of filesystem we're dealing with.
	 */
	AnalyzeImageFS();

	WMSG4(" DI AnalyzeImage tracks=%ld sectors=%d blocks=%ld fileSysOrder=%d\n",
		fNumTracks, fNumSectPerTrack, fNumBlocks, fFileSysOrder);
	WMSG3("    hasBlocks=%d hasSectors=%d hasNibbles=%d\n",
		fHasBlocks, fHasSectors, fHasNibbles);

	return kDIErrNone;
}

/*
 * Try to figure out what filesystem exists on this disk image.
 *
 * We want to test for DOS before ProDOS, because sometimes they overlap (e.g.
 * 800K ProDOS disk with five 160K DOS volumes on it).
 *
 * Sets fFormat, fOrder, and fFileSysOrder.
 */
void
DiskImg::AnalyzeImageFS(void)
{
	/*
	 * In some circumstances it would be useful to have a set describing
	 * what filesystems we might expect to find, e.g. we're not likely to
	 * encounter RDOS embedded in a CF card.
     */
	if (DiskFSMacPart::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
	{
		assert(fFormat == kFormatMacPart);
		WMSG1(" DI found MacPart, order=%d\n", fOrder);
	} else if (DiskFSMicroDrive::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
	{
		assert(fFormat == kFormatMicroDrive);
		WMSG1(" DI found MicroDrive, order=%d\n", fOrder);
	} else if (DiskFSFocusDrive::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
	{
		assert(fFormat == kFormatFocusDrive);
		WMSG1(" DI found FocusDrive, order=%d\n", fOrder);
	} else if (DiskFSCFFA::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
	{
		// The CFFA format doesn't have a partition map, but we do insist
		// on finding multiple volumes.  It needs to come after MicroDrive,
		// because a disk formatted for CFFA then subsequently partitioned
		// for MicroDrive will still look like valid CFFA unless you zero
		// out the blocks.
		assert(fFormat == kFormatCFFA4 || fFormat == kFormatCFFA8);
		WMSG1(" DI found CFFA, order=%d\n", fOrder);
	} else if (DiskFSFAT::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
	{
		// This is really just a trap to catch CFFA cards that were formatted
		// for ProDOS and then re-formatted for MSDOS.  As such it needs to
		// come before the ProDOS test.  It only works on larger volumes,
		// and can be overridden, so it's pretty safe.
		assert(fFormat == kFormatMSDOS);
		WMSG1(" DI found MSDOS, order=%d\n", fOrder);
	} else if (DiskFSDOS33::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
    {
		assert(fFormat == kFormatDOS32 || fFormat == kFormatDOS33);
		WMSG1(" DI found DOS3.x, order=%d\n", fOrder);
		if (fNumSectPerTrack == 13)
			fFormat = kFormatDOS32;
	} else if (DiskFSUNIDOS::TestWideFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
    {
		// Should only succeed on 400K embedded chunks.
		assert(fFormat == kFormatDOS33);
		fNumSectPerTrack = 32;
		fNumTracks /= 2;
		WMSG1(" DI found 'wide' DOS3.3, order=%d\n", fOrder);
	} else if (DiskFSUNIDOS::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
    {
		assert(fFormat == kFormatUNIDOS);
		fNumSectPerTrack = 32;
		fNumTracks /= 2;
		WMSG1(" DI found UNIDOS, order=%d\n", fOrder);
	} else if (DiskFSOzDOS::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
    {
		assert(fFormat == kFormatOzDOS);
		fNumSectPerTrack = 32;
		fNumTracks /= 2;
		WMSG1(" DI found OzDOS, order=%d\n", fOrder);
	} else if (DiskFSProDOS::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
    {
		assert(fFormat == kFormatProDOS);
		WMSG1(" DI found ProDOS, order=%d\n", fOrder);
	} else if (DiskFSPascal::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
    {
		assert(fFormat == kFormatPascal);
		WMSG1(" DI found Pascal, order=%d\n", fOrder);
	} else if (DiskFSCPM::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
    {
		assert(fFormat == kFormatCPM);
		WMSG1(" DI found CP/M, order=%d\n", fOrder);
	} else if (DiskFSRDOS::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
    {
		assert(fFormat == kFormatRDOS33 ||
			   fFormat == kFormatRDOS32 ||
			   fFormat == kFormatRDOS3);
		WMSG1(" DI found RDOS 3.3, order=%d\n", fOrder);
	} else if (DiskFSHFS::TestFS(this, &fOrder, &fFormat, DiskFS::kLeniencyNot) == kDIErrNone)
    {
		assert(fFormat == kFormatMacHFS);
		WMSG1(" DI found HFS, order=%d\n", fOrder);
	} else {
		fFormat = kFormatUnknown;
		WMSG1(" DI no recognizeable filesystem found (fOrder=%d)\n",
            fOrder);
	}

	fFileSysOrder = CalcFSSectorOrder();
}


/*
 * Override the format determined by the analyzer.
 *
 * If they insist on the presence of a valid filesystem, check to make sure
 * that filesystem actually exists.
 *
 * Note that this does not allow overriding the file structure, which must
 * be clearly identifiable to be at all useful.  If the file has no "wrapper"
 * structure, the "unadorned" format should be specified, and the contents
 * identified by the PhysicalFormat.
 */
DIError
DiskImg::OverrideFormat(PhysicalFormat physical, FSFormat format,
	SectorOrder order)
{
	DIError dierr = kDIErrNone;
	SectorOrder newOrder;
	FSFormat newFormat;

	WMSG3(" DI override: physical=%d format=%d order=%d\n",
		physical, format, order);

	if (!IsSectorFormat(physical) && !IsNibbleFormat(physical))
		return kDIErrUnsupportedPhysicalFmt;

    /* don't allow forcing physical format change */
    if (physical != fPhysical)
        return kDIErrInvalidArg;

	/* optimization */
	if (physical == fPhysical && format == fFormat && order == fOrder) {
		WMSG0("  DI override matches existing, ignoring\n");
		return kDIErrNone;
	}

	newOrder = order;
	newFormat = format;

	switch (format) {
	case kFormatDOS33:
	case kFormatDOS32:
        dierr = DiskFSDOS33::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		// Go ahead and allow the override even if the DOS version is wrong.
		// So long as the sector count is correct, it's okay.
		break;
	case kFormatProDOS:
        dierr = DiskFSProDOS::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		break;
	case kFormatPascal:
        dierr = DiskFSPascal::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		break;
	case kFormatMacHFS:
        dierr = DiskFSHFS::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		break;
	case kFormatUNIDOS:
        dierr = DiskFSUNIDOS::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		break;
	case kFormatOzDOS:
        dierr = DiskFSOzDOS::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		break;
	case kFormatCFFA4:
	case kFormatCFFA8:
        dierr = DiskFSCFFA::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		// So long as it's CFFA, we allow the user to force it to be 4-mode
		// or 8-mode.  Don't require newFormat==format.
		break;
	case kFormatMacPart:
		dierr = DiskFSMacPart::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		break;
	case kFormatMicroDrive:
		dierr = DiskFSMicroDrive::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		break;
	case kFormatFocusDrive:
		dierr = DiskFSFocusDrive::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		break;
	case kFormatCPM:
        dierr = DiskFSCPM::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		break;
	case kFormatMSDOS:
        dierr = DiskFSFAT::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		break;
	case kFormatRDOS33:
	case kFormatRDOS32:
	case kFormatRDOS3:
		dierr = DiskFSRDOS::TestFS(this, &newOrder, &newFormat, DiskFS::kLeniencyVery);
		if (newFormat != format)
			dierr = kDIErrFilesystemNotFound;	// found RDOS, but wrong flavor
		break;
	case kFormatGenericPhysicalOrd:
	case kFormatGenericProDOSOrd:
	case kFormatGenericDOSOrd:
	case kFormatGenericCPMOrd:
		/* no discussion possible, since there's no FS to validate */
		newFormat = format;
		newOrder = order;
		break;
	case kFormatUnknown:
		/* only valid in rare situations, e.g. CFFA CreatePlaceholder */
		newFormat = format;
		newOrder = order;
		break;
	default:
		dierr = kDIErrUnsupportedFSFmt;
		break;
	}

	if (dierr != kDIErrNone) {
		WMSG0(" DI override failed\n");
		goto bail;
	}

	/*
	 * We passed in "order" to TestFS.  If it came back with something
	 * different, it means that it didn't like the new order value even
	 * when "leniency" was granted.
	 */
	if (newOrder != order) {
		dierr = kDIErrBadOrdering;
		goto bail;
	}

	fFormat = format;
	fOrder = newOrder;
	fFileSysOrder = CalcFSSectorOrder();

	WMSG0(" DI override accepted\n");

bail:
	return dierr;
}

/*
 * Figure out the sector ordering for this filesystem, so we can decide
 * how the sectors need to be re-arranged when we're reading them.
 *
 * If the value returned by this function matches fOrder, then no swapping
 * will be done.
 *
 * NOTE: this table is redundant with some knowledge embedded in the
 * individual "TestFS" functions.
 */
DiskImg::SectorOrder
DiskImg::CalcFSSectorOrder(void) const
{
	/* in the absence of information, just leave it alone */
	if (fFormat == kFormatUnknown || fOrder == kSectorOrderUnknown) {
		WMSG0(" DI WARNING: FindSectorOrder but format not known\n");
		return fOrder;
	}

	assert(fOrder == kSectorOrderPhysical || fOrder == kSectorOrderCPM ||
		fOrder == kSectorOrderProDOS || fOrder == kSectorOrderDOS);

	switch (fFormat) {
	case kFormatGenericPhysicalOrd:
	case kFormatRDOS32:
	case kFormatRDOS3:
		return kSectorOrderPhysical;

	case kFormatGenericDOSOrd:
	case kFormatDOS33:
	case kFormatDOS32:
	case kFormatUNIDOS:
	case kFormatOzDOS:
		return kSectorOrderDOS;

	case kFormatGenericCPMOrd:
	case kFormatCPM:
		return kSectorOrderCPM;

	case kFormatGenericProDOSOrd:
	case kFormatProDOS:
	case kFormatRDOS33:
	case kFormatPascal:
	case kFormatMacHFS:
	case kFormatMacMFS:
	case kFormatLisa:
	case kFormatMSDOS:
	case kFormatISO9660:
	case kFormatCFFA4:
	case kFormatCFFA8:
	case kFormatMacPart:
	case kFormatMicroDrive:
	case kFormatFocusDrive:
		return kSectorOrderProDOS;

	default:
		assert(false);
		return fOrder;
	}
}

/*
 * Based on the disk format, figure out if we should prefer blocks or
 * sectors when examining disk contents.
 */
bool
DiskImg::ShowAsBlocks(void) const
{
	if (!fHasBlocks)
		return false;

	/* in the absence of information, assume sectors */
	if (fFormat == kFormatUnknown) {
		if (fOrder == kSectorOrderProDOS)
			return true;
		else
			return false;
	}

	switch (fFormat) {
	case kFormatGenericPhysicalOrd:
	case kFormatGenericDOSOrd:
	case kFormatDOS33:
	case kFormatDOS32:
	case kFormatRDOS3:
	case kFormatRDOS33:
	case kFormatUNIDOS:
	case kFormatOzDOS:
		return false;

	case kFormatGenericProDOSOrd:
	case kFormatGenericCPMOrd:
	case kFormatProDOS:
	case kFormatPascal:
	case kFormatMacHFS:
	case kFormatMacMFS:
	case kFormatLisa:
	case kFormatCPM:
	case kFormatMSDOS:
	case kFormatISO9660:
	case kFormatCFFA4:
	case kFormatCFFA8:
	case kFormatMacPart:
	case kFormatMicroDrive:
	case kFormatFocusDrive:
		return true;

	default:
		assert(false);
		return false;
	}
}


/*
 * Format an image with the requested fileystem format.  This only works if
 * the matching DiskFS supports formatting of disks.
 */
DIError
DiskImg::FormatImage(FSFormat format, const char* volName)
{
	DIError dierr = kDIErrNone;
	DiskFS* pDiskFS = nil;
	FSFormat savedFormat;

	WMSG1(" DI FormatImage '%s'\n", volName);

	/*
	 * Open a temporary DiskFS for the requested format.  We do this via the
	 * standard OpenAppropriate call, so we temporarily switch our format
	 * out.  (We will eventually replace it, but we want to make sure that
	 * local error handling works correctly, so we restore it for now.)
	 */
	savedFormat = fFormat;
	fFormat = format;
	pDiskFS = OpenAppropriateDiskFS(false);
	fFormat = savedFormat;

	if (pDiskFS == nil) {
		dierr = kDIErrUnsupportedFSFmt;
		goto bail;
	}

	dierr = pDiskFS->Format(this, volName);
	if (dierr != kDIErrNone)
		goto bail;

	WMSG0("DI format successful\n");
	fFormat = format;

bail:
	delete pDiskFS;
	return dierr;
}

/*
 * Clear an image to zeros, usually done as a prelude to a higher-level format.
 *
 * BUG: this should also handle the track/sector case.
 *
 * HEY: this is awfully slow on large disks... should have some sort of
 * optimized path that just writes to the GFD or something.  Maybe even just
 * a "ZeroBlock" instead of "WriteBlock" so we can memset instead of memcpy?
 */
DIError
DiskImg::ZeroImage(void)
{
	DIError dierr = kDIErrNone;
	unsigned char blkBuf[kBlockSize];
	long block;

	WMSG1(" DI ZeroImage (%ld blocks)\n", GetNumBlocks());
	memset(blkBuf, 0, sizeof(blkBuf));

	for (block = 0; block < GetNumBlocks(); block++) {
		dierr = WriteBlock(block, blkBuf);
		if (dierr != kDIErrNone)
			break;
	}

	return dierr;
}


/*
 * Set the "scan progress" function.
 *
 * We want to use the same function for our sub-volumes too.
 */
void
DiskImg::SetScanProgressCallback(ScanProgressCallback func, void* cookie)
{
	if (fpParentImg != nil) {
		/* unexpected, but perfectly okay */
		DebugBreak();
	}

	fpScanProgressCallback = func;
	fScanProgressCookie = cookie;
	fScanCount = 0;
	fScanMsg[0] = '\0';
	fScanLastMsgWhen = time(nil);
}

/*
 * Update the progress.  Call with a string at the start of a volume, then
 * call with a NULL pointer every time we add a file.
 */
bool
DiskImg::UpdateScanProgress(const char* newStr)
{
	ScanProgressCallback func = fpScanProgressCallback;
	DiskImg* pImg = this;
	bool result = true;

	/* search up the tree to find a progress updater */
	while (func == nil) {
		pImg = pImg->fpParentImg;
		if (pImg == nil)
			return result;		// none defined, bail out
		func = pImg->fpScanProgressCallback;
	}

	time_t now = time(nil);

	if (newStr == NULL) {
		fScanCount++;
		//if ((fScanCount % 100) == 0)
		if (fScanLastMsgWhen != now) {
			result = (*func)(fScanProgressCookie,
						fScanMsg, fScanCount);
			fScanLastMsgWhen = now;
		}
	} else {
		fScanCount = 0;
		strncpy(fScanMsg, newStr, sizeof(fScanMsg));
		fScanMsg[sizeof(fScanMsg)-1] = '\0';
		result = (*func)(fScanProgressCookie, fScanMsg,
					fScanCount);
		fScanLastMsgWhen = now;
	}

	return result;
}


/*
 * ==========================================================================
 *		Block/track/sector I/O
 * ==========================================================================
 */

/*
 * Handle sector order conversions.
 */
DIError
DiskImg::CalcSectorAndOffset(long track, int sector, SectorOrder imageOrder,
    SectorOrder fsOrder, di_off_t* pOffset, int* pNewSector)
{
    if (!fHasSectors)
        return kDIErrUnsupportedAccess;

	/*
	 * Sector order conversions.  No table is needed for Copy ][+ format,
	 * which is equivalent to "physical".
	 */
	static const int raw2dos[16] = {
		0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
	};
	static const int dos2raw[16] = {
		0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
	};
	static const int raw2prodos[16] = {
		0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
	};
	static const int prodos2raw[16] = {
		0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
	};
	static const int raw2cpm[16] = {
		0, 11, 6, 1, 12, 7, 2, 13, 8, 3, 14, 9, 4, 15, 10, 5
	};
	static const int cpm2raw[16] = {
		0, 3, 6, 9, 12, 15, 2, 5, 8, 11, 14, 1, 4, 7, 10, 13
	};

	if (track < 0 || track >= fNumTracks) {
		WMSG1(" DI read invalid track %ld\n", track);
		return kDIErrInvalidTrack;
	}
	if (sector < 0 || sector >= fNumSectPerTrack) {
		WMSG1(" DI read invalid sector %d\n", sector);
		return kDIErrInvalidSector;
	}

	di_off_t offset;
	int newSector = -1;

    /*
     * 16-sector disks write sectors in ascending order and then remap
     * them with a translation table.
     */
	if (fNumSectPerTrack == 16 || fNumSectPerTrack == 32) {
		if (fSectorPairing) {
			assert(fSectorPairOffset == 0 || fSectorPairOffset == 1);
			// this pushes "track" beyond fNumTracks
			track *= 2;
			if (sector >= 16) {
				track++;
				sector -= 16;
			}
			offset = track * fNumSectPerTrack * kSectorSize;

			sector = sector * 2 + fSectorPairOffset;
			if (sector >= 16) {
				offset += 16*kSectorSize;
				sector -= 16;
			}
		} else {
			offset = track * fNumSectPerTrack * kSectorSize;
			if (sector >= 16) {
				offset += 16*kSectorSize;
				sector -= 16;
			}
		}
		assert(sector >= 0 && sector < 16);

		/* convert request to "raw" sector number */
		switch (fsOrder) {
		case kSectorOrderProDOS:
			newSector = prodos2raw[sector];
			break;
		case kSectorOrderDOS:
			newSector = dos2raw[sector];
			break;
		case kSectorOrderCPM:
			newSector = cpm2raw[sector];
			break;
		case kSectorOrderPhysical:	// used for Copy ][+
			newSector = sector;
			break;
		case kSectorOrderUnknown:
			// should never happen; fall through to "default"
		default:
			assert(false);
			newSector = sector;
			break;
		}

		/* convert "raw" request to the image's ordering */
		switch (imageOrder) {
		case kSectorOrderProDOS:
			newSector = raw2prodos[newSector];
			break;
		case kSectorOrderDOS:
			newSector = raw2dos[newSector];
			break;
		case kSectorOrderCPM:
			newSector = raw2cpm[newSector];
			break;
		case kSectorOrderPhysical:
			//newSector = newSector;
			break;
		case kSectorOrderUnknown:
			// should never happen; fall through to "default"
		default:
			assert(false);
			//newSector = newSector;
			break;
		}

		if (imageOrder == fsOrder) {
			assert(sector == newSector);
		}

		offset += newSector * kSectorSize;
	} else if (fNumSectPerTrack == 13) {
		/* sector skew has no meaning, so assume no translation */
		offset = track * fNumSectPerTrack * kSectorSize;
		newSector = sector;
		offset += newSector * kSectorSize;
		if (imageOrder != fsOrder) {
			/* translation expected */
			WMSG2("NOTE: CalcSectorAndOffset for nspt=13 with img=%d fs=%d\n",
				imageOrder, fsOrder);
		}
	} else {
        assert(false);      // should not be here

        /* try to do something reasonable */
		assert(imageOrder == fsOrder);
		offset = (di_off_t)track * fNumSectPerTrack * kSectorSize;
		offset += sector * kSectorSize;
	}

    *pOffset = offset;
    *pNewSector = newSector;
    return kDIErrNone;
}

/*
 * Determine whether an image uses a linear mapping.  This allows us to
 * optimize block reads & writes, very useful when dealing with logical
 * volumes under Windows (which also use 512-byte blocks).
 *
 * The "imageOrder" argument usually comes from fOrder, and "fsOrder"
 * comes from "fFileSysOrder".
 */
inline bool
DiskImg::IsLinearBlocks(SectorOrder imageOrder, SectorOrder fsOrder)
{
	/*
	 * Any time fOrder==fFileSysOrder, we know that we have a linear
	 * mapping.  This holds true for reading ProDOS blocks from a ".po"
	 * file or reading DOS sectors from a ".do" file.
	 */
	return (IsSectorFormat(fPhysical) && fHasBlocks &&
			imageOrder == fsOrder);
}

/*
 * Read the specified track and sector, adjusting for sector ordering as
 * appropriate.
 *
 * Copies 256 bytes into "*buf".
 *
 * Returns 0 on success, nonzero on failure.
 */
DIError
DiskImg::ReadTrackSectorSwapped(long track, int sector, void* buf,
	SectorOrder imageOrder, SectorOrder fsOrder)
{
    DIError dierr;
	di_off_t offset;
	int newSector = -1;

	if (buf == nil)
		return kDIErrInvalidArg;

#if 0	// Pre-d13
	if (fNumSectPerTrack == 13) {
        /* no sector skewing possible for 13-sector disks */
        assert(fHasNibbles);

        return ReadNibbleSector(track, sector, buf, fpNibbleDescr);
    }
#endif

    dierr = CalcSectorAndOffset(track, sector, imageOrder, fsOrder,
                &offset, &newSector);
    if (dierr != kDIErrNone)
        return dierr;

    if (IsSectorFormat(fPhysical)) {
        assert(offset+kSectorSize <= fLength);

        //WMSG2("  DI t=%d s=%d\n", track,
        //	(offset - track * fNumSectPerTrack * kSectorSize) / kSectorSize);

        dierr = CopyBytesOut(buf, offset, kSectorSize);
    } else if (IsNibbleFormat(fPhysical)) {
        if (imageOrder != kSectorOrderPhysical) {
            WMSG2("  NOTE: nibble imageOrder is %d (expected %d)\n",
                imageOrder, kSectorOrderPhysical);
        }
        dierr = ReadNibbleSector(track, newSector, buf, fpNibbleDescr);
    } else {
        assert(false);
        dierr = kDIErrInternal;
    }

    return dierr;
}

/*
 * Write the specified track and sector, adjusting for sector ordering as
 * appropriate.
 *
 * Copies 256 bytes out of "buf".
 *
 * Returns 0 on success, nonzero on failure.
 */
DIError
DiskImg::WriteTrackSector(long track, int sector, const void* buf)
{
    DIError dierr;
	di_off_t offset;
	int newSector = -1;

	if (buf == nil)
		return kDIErrInvalidArg;
    if (fReadOnly)
        return kDIErrAccessDenied;

#if 0	// Pre-d13
	if (fNumSectPerTrack == 13) {
        /* no sector skewing possible for 13-sector disks */
        assert(fHasNibbles);

        return WriteNibbleSector(track, sector, buf, fpNibbleDescr);
    }
#endif

    dierr = CalcSectorAndOffset(track, sector, fOrder, fFileSysOrder,
                &offset, &newSector);
    if (dierr != kDIErrNone)
        return dierr;

    if (IsSectorFormat(fPhysical)) {
        assert(offset+kSectorSize <= fLength);

        //WMSG2("  DI t=%d s=%d\n", track,
        //	(offset - track * fNumSectPerTrack * kSectorSize) / kSectorSize);

        dierr = CopyBytesIn(buf, offset, kSectorSize);
    } else if (IsNibbleFormat(fPhysical)) {
        if (fOrder != kSectorOrderPhysical) {
            WMSG2("  NOTE: nibble fOrder is %d (expected %d)\n",
                fOrder, kSectorOrderPhysical);
        }
        dierr = WriteNibbleSector(track, newSector, buf, fpNibbleDescr);
    } else {
        assert(false);
        dierr = kDIErrInternal;
    }

    return dierr;
}

/*
 * Read a 512-byte block.
 *
 * Copies 512 bytes into "*buf".
 */
DIError
DiskImg::ReadBlockSwapped(long block, void* buf, SectorOrder imageOrder,
	SectorOrder fsOrder)
{
    if (!fHasBlocks)
        return kDIErrUnsupportedAccess;
	if (block < 0 || block >= fNumBlocks)
		return kDIErrInvalidBlock;
	if (buf == nil)
		return kDIErrInvalidArg;

	DIError dierr;
	long track, blkInTrk;

	/* if we have a bad block map, check it */
	if (CheckForBadBlocks(block, 1)) {
		dierr = kDIErrReadFailed;
		goto bail;
	}

    if (fHasSectors && !IsLinearBlocks(imageOrder, fsOrder)) {
        /* run it through the t/s call so we handle DOS ordering */
        track = block / (fNumSectPerTrack/2);
        blkInTrk = block - (track * (fNumSectPerTrack/2));
        dierr = ReadTrackSectorSwapped(track, blkInTrk*2, buf,
                    imageOrder, fsOrder);
        if (dierr != kDIErrNone)
            return dierr;
        dierr = ReadTrackSectorSwapped(track, blkInTrk*2+1,
					(char*)buf+kSectorSize, imageOrder, fsOrder);
    } else if (fHasBlocks) {
        /* no sectors, so no swapping; must be linear blocks */
        if (imageOrder != fsOrder) {
            WMSG2(" DI NOTE: ReadBlockSwapped on non-sector (%d/%d)\n",
                imageOrder, fsOrder);
        }
        dierr = CopyBytesOut(buf, (di_off_t) block * kBlockSize, kBlockSize);
    } else {
        assert(false);
        dierr = kDIErrInternal;
    }

bail:
	return dierr;
}

/*
 * Read multiple blocks.
 *
 * IMPORTANT: this returns immediately when a read fails.  The buffer will
 * probably not contain data from all readable sectors.  The application is
 * expected to retry the blocks individually.
 */
DIError
DiskImg::ReadBlocks(long startBlock, int numBlocks, void* buf)
{
	DIError dierr = kDIErrNone;

	assert(fHasBlocks);
	assert(startBlock >= 0);
	assert(numBlocks > 0);
	assert(buf != nil);

	if (startBlock < 0 || numBlocks + startBlock > GetNumBlocks()) {
		assert(false);
		return kDIErrInvalidArg;
	}

	/* if we have a bad block map, check it */
	if (CheckForBadBlocks(startBlock, numBlocks)) {
		dierr = kDIErrReadFailed;
		goto bail;
	}

	if (!IsLinearBlocks(fOrder, fFileSysOrder)) {
		/*
		 * This isn't a collection of linear blocks, so we need to read it one
		 * block at a time with sector swapping.  This almost certainly means
		 * that we're not reading from physical media, so performance shouldn't
		 * be an issue.
		 */
		if (startBlock == 0) {
			WMSG0(" ReadBlocks: nonlinear, not trying\n");
		}
		while (numBlocks--) {
			dierr = ReadBlock(startBlock, buf);
			if (dierr != kDIErrNone)
				goto bail;
			startBlock++;
			buf = (unsigned char*)buf + kBlockSize;
		}
	} else {
		if (startBlock == 0) {
			WMSG0(" ReadBlocks: doing big linear reads\n");
		}
		dierr = CopyBytesOut(buf,
					(di_off_t) startBlock * kBlockSize, numBlocks * kBlockSize);
	}

bail:
	return dierr;
}

/*
 * Check to see if any blocks in a range of blocks show up in the bad
 * block map.  This is primarily useful for 3.5" disk images converted
 * from nibble images, because we convert them directly to "cooked"
 * 512-byte blocks.
 *
 * Returns "true" if we found bad blocks, "false" if not.
 */
bool
DiskImg::CheckForBadBlocks(long startBlock, int numBlocks)
{
	int i;

	if (fpBadBlockMap == nil)
		return false;

	for (i = startBlock; i < startBlock+numBlocks; i++) {
		if (fpBadBlockMap->IsSet(i))
			return true;
	}
	return false;
}

/*
 * Write a block of data to a DiskImg.
 *
 * Returns immediately when a block write fails.  Does not try to write all
 * blocks before returning failure.
 */
DIError
DiskImg::WriteBlock(long block, const void* buf)
{
    if (!fHasBlocks)
        return kDIErrUnsupportedAccess;
	if (block < 0 || block >= fNumBlocks)
		return kDIErrInvalidBlock;
	if (buf == nil)
		return kDIErrInvalidArg;
    if (fReadOnly)
        return kDIErrAccessDenied;

	DIError dierr;
	long track, blkInTrk;

    if (fHasSectors && !IsLinearBlocks(fOrder, fFileSysOrder)) {
        /* run it through the t/s call so we handle DOS ordering */
        track = block / (fNumSectPerTrack/2);
        blkInTrk = block - (track * (fNumSectPerTrack/2));
        dierr = WriteTrackSector(track, blkInTrk*2, buf);
        if (dierr != kDIErrNone)
            return dierr;
        dierr = WriteTrackSector(track, blkInTrk*2+1, (char*)buf+kSectorSize);
    } else if (fHasBlocks) {
        /* no sectors, so no swapping; must be linear blocks */
        if (fOrder != fFileSysOrder) {
            WMSG2(" DI NOTE: WriteBlock on non-sector (%d/%d)\n",
                fOrder, fFileSysOrder);
        }
        dierr = CopyBytesIn(buf, (di_off_t)block * kBlockSize, kBlockSize);
    } else {
        assert(false);
        dierr = kDIErrInternal;
    }
	return dierr;
}

/*
 * Write multiple blocks.
 */
DIError
DiskImg::WriteBlocks(long startBlock, int numBlocks, const void* buf)
{
	DIError dierr = kDIErrNone;

	assert(fHasBlocks);
	assert(startBlock >= 0);
	assert(numBlocks > 0);
	assert(buf != nil);

	if (startBlock < 0 || numBlocks + startBlock > GetNumBlocks()) {
		assert(false);
		return kDIErrInvalidArg;
	}

	if (!IsLinearBlocks(fOrder, fFileSysOrder)) {
		/*
		 * This isn't a collection of linear blocks, so we need to write it
		 * one block at a time with sector swapping.  This almost certainly
		 * means that we're not reading from physical media, so performance
		 * shouldn't be an issue.
		 */
		if (startBlock == 0) {
			WMSG0(" WriteBlocks: nonlinear, not trying\n");
		}
		while (numBlocks--) {
			dierr = WriteBlock(startBlock, buf);
			if (dierr != kDIErrNone)
				goto bail;
			startBlock++;
			buf = (unsigned char*)buf + kBlockSize;
		}
	} else {
		if (startBlock == 0) {
			WMSG0(" WriteBlocks: doing big linear writes\n");
		}
		dierr = CopyBytesIn(buf,
					(di_off_t) startBlock * kBlockSize, numBlocks * kBlockSize);
	}

bail:
	return dierr;
}


/*
 * Copy a chunk of bytes out of the disk image.
 *
 * (This is the lowest-level read routine in this class.)
 */
DIError
DiskImg::CopyBytesOut(void* buf, di_off_t offset, int size) const
{
	DIError dierr;

	dierr = fpDataGFD->Seek(offset, kSeekSet);
	if (dierr != kDIErrNone) {
		WMSG2(" DI seek off=%ld failed (err=%d)\n", (long) offset, dierr);
		return dierr;
	}

	dierr = fpDataGFD->Read(buf, size);
	if (dierr != kDIErrNone) {
		WMSG3(" DI read off=%ld size=%d failed (err=%d)\n",
            (long) offset, size, dierr);
		return dierr;
	}

	return kDIErrNone;
}

/*
 * Copy a chunk of bytes into the disk image.
 *
 * Sets the "dirty" flag.
 *
 * (This is the lowest-level write routine in DiskImg.)
 */
DIError
DiskImg::CopyBytesIn(const void* buf, di_off_t offset, int size)
{
	DIError dierr;

    if (fReadOnly) {
		DebugBreak();
        return kDIErrAccessDenied;
	}
	assert(fpDataGFD != nil);	// somebody closed the image?

	dierr = fpDataGFD->Seek(offset, kSeekSet);
	if (dierr != kDIErrNone) {
		WMSG2(" DI seek off=%ld failed (err=%d)\n", (long) offset, dierr);
		return dierr;
	}

	dierr = fpDataGFD->Write(buf, size);
	if (dierr != kDIErrNone) {
		WMSG3(" DI write off=%ld size=%d failed (err=%d)\n",
            (long) offset, size, dierr);
		return dierr;
	}

	/* set the dirty flag here and everywhere above */
	DiskImg* pImg = this;
	while (pImg != nil) {
		pImg->fDirty = true;
		pImg = pImg->fpParentImg;
	}

	return kDIErrNone;
}


/*
 * ===========================================================================
 *		Image creation
 * ===========================================================================
 */

/*
 * Create a disk image with the specified parameters.
 *
 * "storageName" and "pNibbleDescr" may be nil.
 */
DIError
DiskImg::CreateImage(const char* pathName, const char* storageName,
    OuterFormat outerFormat, FileFormat fileFormat, PhysicalFormat physical,
    const NibbleDescr* pNibbleDescr, SectorOrder order,
    FSFormat format, long numBlocks, bool skipFormat)
{
    assert(fpDataGFD == nil);       // should not be open already!

	if (numBlocks <= 0) {
		WMSG1("ERROR: bad numBlocks %ld\n", numBlocks);
		assert(false);
		return kDIErrInvalidCreateReq;
	}

    fOuterFormat = outerFormat;
    fFileFormat = fileFormat;
    fPhysical = physical;
	SetCustomNibbleDescr(pNibbleDescr);
    fOrder = order;
    fFormat = format;

    fNumBlocks = numBlocks;
    fHasBlocks = true;

    return CreateImageCommon(pathName, storageName, skipFormat);
}
DIError
DiskImg::CreateImage(const char* pathName, const char* storageName,
    OuterFormat outerFormat, FileFormat fileFormat, PhysicalFormat physical,
    const NibbleDescr* pNibbleDescr, SectorOrder order,
    FSFormat format, long numTracks, long numSectPerTrack, bool skipFormat)
{
    assert(fpDataGFD == nil);       // should not be open already!

	if (numTracks <= 0 || numSectPerTrack == 0) {
		WMSG2("ERROR: bad tracks/sectors %ld/%ld\n", numTracks, numSectPerTrack);
		assert(false);
		return kDIErrInvalidCreateReq;
	}

    fOuterFormat = outerFormat;
    fFileFormat = fileFormat;
    fPhysical = physical;
	SetCustomNibbleDescr(pNibbleDescr);
    fOrder = order;
    fFormat = format;

    fNumTracks = numTracks;
    fNumSectPerTrack = numSectPerTrack;
    fHasSectors = true;
	if (numSectPerTrack < 0) {
		/* nibble image with non-standard formatting */
		if (!IsNibbleFormat(fPhysical)) {
			WMSG0("Whoa: expected nibble format here\n");
			assert(false);
			return kDIErrInvalidCreateReq;
		}
		WMSG0("Sector image w/o sectors, switching to nibble mode\n");
		fHasNibbles = true;
		fHasSectors = false;
		fpNibbleDescr = nil;
	}

    return CreateImageCommon(pathName, storageName, skipFormat);
}

/*
 * Do the actual disk image creation.
 */
DIError
DiskImg::CreateImageCommon(const char* pathName, const char* storageName,
	bool skipFormat)
{
    DIError dierr;

    /*
     * Step 1: figure out fHasBlocks/fHasSectors/fHasNibbles and any
     * other misc fields.
     *
     * If the disk is a nibble image expected to have a particular
     * volume number, it should have already been set by the application.
     */
    if (fHasBlocks) {
        if ((fNumBlocks % 8) == 0) {
            fHasSectors = true;
            fNumSectPerTrack = 16;
            fNumTracks = fNumBlocks / 8;
        } else {
            WMSG0("NOTE: sector access to new image not possible\n");
        }
    } else if (fHasSectors) {
        if ((fNumSectPerTrack & 0x01) == 0) {
            fHasBlocks = true;
            fNumBlocks = (fNumTracks * fNumSectPerTrack) / 2;
        } else {
            WMSG0("NOTE: block access to new image not possible\n");
        }
	}
    if (fHasSectors && fPhysical != kPhysicalFormatSectors)
        fHasNibbles = true;
    assert(fHasBlocks || fHasSectors || fHasNibbles);

    fFileSysOrder = CalcFSSectorOrder();
    fReadOnly = false;
    fDirty = true;

    /*
     * Step 2: check for invalid arguments and bad combinations.
     */
    dierr = ValidateCreateFormat();
    if (dierr != kDIErrNone) {
        WMSG0("ERROR: CIC arg validation failed, bailing\n");
        goto bail;
    }

    /*
     * Step 3: create the destination file.  Put this into fpWrapperGFD
     * or fpOuterGFD.
     *
     * The file must not already exist.
	 *
	 * THOUGHT: should allow creation of an in-memory disk image.  This won't
	 * work for NuFX, but will work for pretty much everything else.
     */
    WMSG1(" CIC: creating '%s'\n", pathName);
    int fd;
    fd = open(pathName, O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        dierr = (DIError) errno;
        WMSG2("ERROR: unable to create file '%s' (errno=%d)\n",
			pathName, dierr);
        goto bail;
    }
    close(fd);

	GFDFile* pGFDFile;
	pGFDFile = new GFDFile;

	dierr = pGFDFile->Open(pathName, false);
	if (dierr != kDIErrNone) {
		delete pGFDFile;
		goto bail;
	}

    if (fOuterFormat == kOuterFormatNone)
        fpWrapperGFD = pGFDFile;
    else
        fpOuterGFD = pGFDFile;
    pGFDFile = nil;

    /*
     * Step 4: if we have an outer GFD and therefore don't currently have
     * an fpWrapperGFD, create an expandable memory buffer to use.
     *
     * We want to take a guess at how big the image will be, so compute
     * fLength now.
     *
     * Create an OuterWrapper as needed.
     */
    if (IsSectorFormat(fPhysical)) {
        if (fHasBlocks)
            fLength = (di_off_t) GetNumBlocks() * kBlockSize;
        else
            fLength = (di_off_t) GetNumTracks() * GetNumSectPerTrack() * kSectorSize;
    } else {
		assert(IsNibbleFormat(fPhysical));
        fLength = GetNumTracks() * GetNibbleTrackAllocLength();
    }
	assert(fLength > 0);

    if (fpWrapperGFD == nil) {
        /* shift GFDs and create a new memory GFD, pre-sized */
        GFDBuffer* pGFDBuffer = new GFDBuffer;

        /* use fLength as a starting point for buffer size; this may expand */
        dierr = pGFDBuffer->Open(nil, fLength, true, true, false);
        if (dierr != kDIErrNone) {
            delete pGFDBuffer;
            goto bail;
        }

        fpWrapperGFD = pGFDBuffer;
        pGFDBuffer = nil;
    }

    /* create an fpOuterWrapper struct */
    switch (fOuterFormat) {
    case kOuterFormatNone:
        break;
    case kOuterFormatGzip:
        fpOuterWrapper = new OuterGzip;
        if (fpOuterWrapper == nil) {
            dierr = kDIErrMalloc;
            goto bail;
        }
        break;
    case kOuterFormatZip:
        fpOuterWrapper = new OuterZip;
        if (fpOuterWrapper == nil) {
            dierr = kDIErrMalloc;
            goto bail;
        }
        break;
    default:
        assert(false);
        dierr = kDIErrInternal;
        goto bail;
    }

    /*
     * Step 5: tell the ImageWrapper to write itself into the GFD, passing
     * in the blank memory buffer.
     *
     *  - Unadorned formats copy from memory buffer to fpWrapperGFD on disk.
     *    (With gz, fpWrapperGFD is actually a memory buffer.)  fpDataGFD
     *    becomes an offset into the file.
     *  - 2MG writes header into GFD and follows it with all data; DC42
     *    and Sim2e do similar things.
     *  - NuFX reopens pathName as SHK file (fpWrapperGFD must point to a
     *    file) and accesses the archive through an fpArchive.  fpDataGFD
     *    is created as a memory buffer and the blank image is copied in.
     *  - DDD leaves fpWrapperGFD alone and copies the blank image into a
     *    new buffer for fpDataGFD.
     *
     * Sets fWrappedLength when possible, determined from fPhysical and
	 * either fNumBlocks or fNumTracks.  Creates fpDataGFD, often as a
	 * GFDGFD offset into fpWrapperGFD.
     */
	switch (fFileFormat) {
	case kFileFormat2MG:
		fpImageWrapper = new Wrapper2MG();
		break;
	case kFileFormatDiskCopy42:
		fpImageWrapper = new WrapperDiskCopy42();
		fpImageWrapper->SetStorageName(storageName);
		break;
	case kFileFormatSim2eHDV:
		fpImageWrapper = new WrapperSim2eHDV();
		break;
	case kFileFormatTrackStar:
		fpImageWrapper = new WrapperTrackStar();
		fpImageWrapper->SetStorageName(storageName);
		break;
	case kFileFormatFDI:
		fpImageWrapper = new WrapperFDI();
		break;
	case kFileFormatNuFX:
		fpImageWrapper = new WrapperNuFX();
        fpImageWrapper->SetStorageName(storageName);
        ((WrapperNuFX*)fpImageWrapper)->SetCompressType(
										(NuThreadFormat) fNuFXCompressType);
		break;
    case kFileFormatDDD:
		fpImageWrapper = new WrapperDDD();
        break;
	case kFileFormatUnadorned:
		if (IsSectorFormat(fPhysical))
			fpImageWrapper = new WrapperUnadornedSector();
        else if (IsNibbleFormat(fPhysical))
            fpImageWrapper = new WrapperUnadornedNibble();
		else {
			assert(false);
		}
		break;
	default:
        assert(fpImageWrapper == nil);
		break;
	}

    if (fpImageWrapper == nil) {
		WMSG0(" DI couldn't figure out the file format\n");
		dierr = kDIErrUnrecognizedFileFmt;
        goto bail;
    }

    /* create the wrapper, write the header, and create fpDataGFD */
    assert(fpDataGFD == nil);
    dierr = fpImageWrapper->Create(fLength, fPhysical, fOrder,
                fDOSVolumeNum, fpWrapperGFD, &fWrappedLength, &fpDataGFD);
    if (dierr != kDIErrNone) {
        WMSG1("ImageWrapper Create failed, err=%d\n", dierr);
        goto bail;
    }
    assert(fpDataGFD != nil);

    /*
     * Step 6: "format" fpDataGFD.
     *
     * Note we don't specify an ordering to the "create blank" functions.
     * Either it's sectors, in which case it's all zeroes, or it's nibbles,
     * in which case it's always in physical order.
     *
     * If we're formatting for nibbles, and the application hasn't specified
     * a disk volume number, use the default (254).
     */
	if (fPhysical == kPhysicalFormatSectors)
		dierr = FormatSectors(fpDataGFD, skipFormat);	// zero out the image
	else {
		assert(!skipFormat);		// don't skip low-level nibble formatting!
		if (fDOSVolumeNum == kVolumeNumNotSet) {
			fDOSVolumeNum = kDefaultNibbleVolumeNum;
			WMSG0("    Using default nibble volume num\n");
		}

		dierr = FormatNibbles(fpDataGFD);	// write basic nibble stuff
	}


    /*
     * We're done!
     *
     * Quick sanity check...
     */
    if (fOuterFormat != kOuterFormatNone) {
        assert(fpOuterGFD != nil);
        assert(fpWrapperGFD != nil);
        assert(fpDataGFD != nil);
    }

bail:
    return dierr;
}

/*
 * Check that the requested format is one we can create.
 *
 * We don't allow .SDK.GZ or 6384-byte nibble 2MG.  2MG sector images
 * must be in DOS or ProDOS order.
 *
 * Only "generic" FS formats may be used.  The application may choose
 * to call AnalyzeImage later on to set the actual FS once data has
 * been written.
 */
DIError
DiskImg::ValidateCreateFormat(void) const
{
    /*
     * Check for invalid arguments.
     */
	if (fHasBlocks && fNumBlocks >= 4194304) {	// 2GB or larger?
		if (fFileFormat != kFileFormatUnadorned) {
			WMSG0("CreateImage: images >= 2GB can only be unadorned\n");
			return kDIErrInvalidCreateReq;
		}
    }
    if (fOuterFormat == kOuterFormatUnknown ||
        fFileFormat == kFileFormatUnknown ||
        fPhysical == kPhysicalFormatUnknown ||
        fOrder == kSectorOrderUnknown ||
        fFormat == kFormatUnknown)
    {
        WMSG0("CreateImage: ambiguous format\n");
        return kDIErrInvalidCreateReq;
    }
    if (fOuterFormat != kOuterFormatNone &&
        fOuterFormat != kOuterFormatGzip &&
		fOuterFormat != kOuterFormatZip)
    {
        WMSG1("CreateImage: unsupported outer format %d\n", fOuterFormat);
        return kDIErrInvalidCreateReq;
    }
    if (fFileFormat != kFileFormatUnadorned &&
        fFileFormat != kFileFormat2MG &&
        fFileFormat != kFileFormatDiskCopy42 &&
        fFileFormat != kFileFormatSim2eHDV &&
		fFileFormat != kFileFormatTrackStar &&
		fFileFormat != kFileFormatFDI &&
        fFileFormat != kFileFormatNuFX &&
        fFileFormat != kFileFormatDDD)
    {
        WMSG1("CreateImage: unsupported file format %d\n", fFileFormat);
        return kDIErrInvalidCreateReq;
    }
    if (fFormat != kFormatGenericPhysicalOrd &&
        fFormat != kFormatGenericProDOSOrd &&
        fFormat != kFormatGenericDOSOrd &&
        fFormat != kFormatGenericCPMOrd)
    {
        WMSG0("CreateImage: may only use 'generic' formats\n");
        return kDIErrInvalidCreateReq;
    }

    /*
     * Check for invalid combinations.
     */
    if (fPhysical != kPhysicalFormatSectors) {
        if (fOrder != kSectorOrderPhysical) {
            WMSG0("CreateImage: nibble images are always 'physical' order\n");
            return kDIErrInvalidCreateReq;
        }

        if (GetHasSectors() == false && GetHasNibbles() == false) {
            WMSG2("CreateImage: must set hasSectors(%d) or hasNibbles(%d)\n",
                GetHasSectors(), GetHasNibbles());
            return kDIErrInvalidCreateReq;
        }

        if (fpNibbleDescr == nil && GetNumSectPerTrack() > 0) {
            WMSG0("CreateImage: must provide NibbleDescr for non-sector\n");
            return kDIErrInvalidCreateReq;
        }

        if (fpNibbleDescr != nil &&
			fpNibbleDescr->numSectors != GetNumSectPerTrack())
		{
            WMSG2("CreateImage: ?? nd->numSectors=%d, GetNumSectPerTrack=%d\n",
                fpNibbleDescr->numSectors, GetNumSectPerTrack());
            return kDIErrInvalidCreateReq;
        }

        if (fpNibbleDescr != nil && (
			(fpNibbleDescr->numSectors == 13 &&
             fpNibbleDescr->encoding != kNibbleEnc53) ||
            (fpNibbleDescr->numSectors == 16 &&
             fpNibbleDescr->encoding != kNibbleEnc62))
			)
        {
            WMSG0("CreateImage: sector count/encoding mismatch\n");
            return kDIErrInvalidCreateReq;
        }

		if (GetNumTracks() != kTrackCount525 &&
			!(GetNumTracks() == 40 && fFileFormat == kFileFormatTrackStar))
		{
			WMSG1("CreateImage: unexpected track count %ld\n", GetNumTracks());
			return kDIErrInvalidCreateReq;
		}
    }
    if (fFileFormat == kFileFormat2MG) {
        if (fPhysical != kPhysicalFormatSectors &&
            fPhysical != kPhysicalFormatNib525_6656)
        {
            WMSG1("CreateImage: 2MG can't handle physical %d\n", fPhysical);
            return kDIErrInvalidCreateReq;
        }

        if (fPhysical == kPhysicalFormatSectors &&
                (fOrder != kSectorOrderProDOS &&
                fOrder != kSectorOrderDOS))
        {
            WMSG0("CreateImage: 2MG requires DOS or ProDOS ordering\n");
            return kDIErrInvalidCreateReq;
        }
    }
    if (fFileFormat == kFileFormatNuFX) {
        if (fOuterFormat != kOuterFormatNone) {
            WMSG0("CreateImage: can't mix NuFX and outer wrapper\n");
            return kDIErrInvalidCreateReq;
        }
        if (fPhysical != kPhysicalFormatSectors) {
            WMSG0("CreateImage: NuFX physical must be sectors\n");
            return kDIErrInvalidCreateReq;
        }
        if (fOrder != kSectorOrderProDOS) {
            WMSG0("CreateImage: NuFX is always ProDOS-order\n");
            return kDIErrInvalidCreateReq;
        }
    }
    if (fFileFormat == kFileFormatDiskCopy42) {
        if (fPhysical != kPhysicalFormatSectors) {
            WMSG0("CreateImage: DC42 physical must be sectors\n");
            return kDIErrInvalidCreateReq;
        }
        if ((GetHasBlocks() && GetNumBlocks() != 1600) ||
            GetHasSectors() &&
                (GetNumTracks() != 200 || GetNumSectPerTrack() != 16))
        {
            WMSG0("CreateImage: DC42 only for 800K disks\n");
            return kDIErrInvalidCreateReq;
        }
        if (fOrder != kSectorOrderProDOS &&
			fOrder != kSectorOrderDOS)		// used for UNIDOS disks??
		{
            WMSG0("CreateImage: DC42 is always ProDOS or DOS\n");
            return kDIErrInvalidCreateReq;
        }
    }
    if (fFileFormat == kFileFormatSim2eHDV) {
        if (fPhysical != kPhysicalFormatSectors) {
            WMSG0("CreateImage: Sim2eHDV physical must be sectors\n");
            return kDIErrInvalidCreateReq;
        }
        if (fOrder != kSectorOrderProDOS) {
            WMSG0("CreateImage: Sim2eHDV is always ProDOS-order\n");
            return kDIErrInvalidCreateReq;
        }
    }
	if (fFileFormat == kFileFormatTrackStar) {
		if (fPhysical != kPhysicalFormatNib525_Var) {
			WMSG0("CreateImage: TrackStar physical must be var-nibbles\n");
			return kDIErrInvalidCreateReq;
		}
	}
	if (fFileFormat == kFileFormatFDI) {
		if (fPhysical != kPhysicalFormatNib525_Var) {
			WMSG0("CreateImage: FDI physical must be var-nibbles\n");
			return kDIErrInvalidCreateReq;
		}
	}
    if (fFileFormat == kFileFormatDDD) {
        if (fPhysical != kPhysicalFormatSectors) {
            WMSG0("CreateImage: DDD physical must be sectors\n");
            return kDIErrInvalidCreateReq;
        }
        if (fOrder != kSectorOrderDOS) {
            WMSG0("CreateImage: DDD is always DOS-order\n");
            return kDIErrInvalidCreateReq;
        }
        if (!GetHasSectors() || GetNumTracks() != 35 ||
            GetNumSectPerTrack() != 16)
        {
            WMSG0("CreateImage: DDD is only for 16-sector 35-track disks\n");
            return kDIErrInvalidCreateReq;
        }
    }

    return kDIErrNone;
}

/*
 * Create a blank image for physical=="sectors".
 *
 * fLength must be a multiple of 256.
 *
 * If "quickFormat" is set, only the very last sector is written (to set
 * the EOF on the file).
 */
DIError
DiskImg::FormatSectors(GenericFD* pGFD, bool quickFormat) const
{
	DIError dierr = kDIErrNone;
    char sctBuf[kSectorSize];
    di_off_t length;

	assert(fLength > 0 && (fLength & 0xff) == 0);

	//if (!(fLength & 0x01))
	//	return FormatBlocks(pGFD);

    memset(sctBuf, 0, sizeof(sctBuf));
    pGFD->Rewind();

	if (quickFormat) {
		dierr = pGFD->Seek(fLength - sizeof(sctBuf), kSeekSet);
		if (dierr != kDIErrNone) {
			WMSG2(" FormatSectors: GFD seek %ld failed (err=%d)\n",
				(long) fLength - sizeof(sctBuf), dierr);
			goto bail;
		}
        dierr = pGFD->Write(sctBuf, sizeof(sctBuf), nil);
        if (dierr != kDIErrNone) {
            WMSG1(" FormatSectors: GFD quick write failed (err=%d)\n", dierr);
            goto bail;
        }
	} else {
		for (length = fLength ; length > 0; length -= sizeof(sctBuf)) {
			dierr = pGFD->Write(sctBuf, sizeof(sctBuf), nil);
			if (dierr != kDIErrNone) {
				WMSG1(" FormatSectors: GFD write failed (err=%d)\n", dierr);
				goto bail;
			}
		}
		assert(length == 0);
	}


bail:
    return dierr;
}

#if 0	// didn't help
/*
 * Create a blank image for physical=="sectors".  This is called from
 * FormatSectors when it looks like we're formatting entire blocks.
 */
DIError
DiskImg::FormatBlocks(GenericFD* pGFD) const
{
	DIError dierr;
    char blkBuf[kBlockSize];
    long length;
	time_t start, end;

	assert(fLength > 0 && (fLength & 0x1ff) == 0);

	start = time(nil);

    memset(blkBuf, 0, sizeof(blkBuf));
    pGFD->Rewind();

    for (length = fLength ; length > 0; length -= sizeof(blkBuf)) {
        dierr = pGFD->Write(blkBuf, sizeof(blkBuf), nil);
        if (dierr != kDIErrNone) {
            WMSG1(" FormatBlocks: GFD write failed (err=%d)\n", dierr);
            return dierr;
        }
    }
    assert(length == 0);

	end = time(nil);
	WMSG1("FormatBlocks complete, time=%ld\n", end - start);

    return kDIErrNone;
}
#endif


/*
 * ===========================================================================
 *		Utility functions
 * ===========================================================================
 */

/*
 * Add a note to this disk image.
 *
 * This is how we communicate cautions and warnings to the user.  Use
 * linefeeds ('\n') to indicate line breaks.
 *
 * The maximum length of a single note is set by the size of "buf".
 */
void
DiskImg::AddNote(NoteType type, const char* fmt, ...)
{
	char buf[512];
	char* cp = buf;
	int maxLen = sizeof(buf);
    va_list args;
	int len;

	/*
	 * Prepend a string that highlights the note.
	 */
	switch (type) {
	case kNoteWarning:
		strcpy(cp, "- WARNING: ");
		break;
	default:
		strcpy(cp, "- ");
		break;
	}
	len = strlen(cp);
	cp += len;
	maxLen -= len;

	/*
	 * Add the note.
	 */
    va_start(args, fmt);
#if defined(HAVE_VSNPRINTF)
	(void) vsnprintf(cp, maxLen, fmt, args);
#elif defined(HAVE__VSNPRINTF)
	(void) _vsnprintf(cp, maxLen, fmt, args);
#else
# error "hosed"
#endif
    va_end(args);

	buf[sizeof(buf)-2] = '\0';		// leave room for additional '\n'
	len = strlen(buf);
	if (len > 0 && buf[len-1] != '\n') {
		buf[len] = '\n';
		buf[len+1] = '\0';
		len++;
	}

	WMSG1("+++ adding note '%s'\n", buf);

	if (fNotes == nil) {
		fNotes = new char[len +1];
		if (fNotes == nil) {
			WMSG1("Unable to create notes[%d]\n", len+1);
			assert(false);
			return;
		}
		strcpy(fNotes, buf);
	} else {
		int existingLen = strlen(fNotes);
		char* newNotes = new char[existingLen + len +1];
		if (newNotes == nil) {
			WMSG1("Unable to create newNotes[%d]\n", existingLen+len+1);
			assert(false);
			return;
		}
		strcpy(newNotes, fNotes);
		strcpy(newNotes + existingLen, buf);
		delete[] fNotes;
		fNotes = newNotes;
	}
}

/*
 * Return a string with the notes in it.
 */
const char*
DiskImg::GetNotes(void) const
{
	if (fNotes == nil)
		return "";
	else
		return fNotes;
}


/*
 * Get length and offset of tracks in a nibble image.  This is necessary
 * because of formats with variable-length tracks (e.g. TrackStar).
 */
int
DiskImg::GetNibbleTrackLength(long track) const
{
	assert(fpImageWrapper != NULL);
	return fpImageWrapper->GetNibbleTrackLength(fPhysical, track);
}
int
DiskImg::GetNibbleTrackOffset(long track) const
{
	assert(fpImageWrapper != NULL);
	return fpImageWrapper->GetNibbleTrackOffset(fPhysical, track);
}


/*
 * Return a new object with the appropriate DiskFS sub-class.
 *
 * If the image hasn't been analyzed, or was analyzed to no avail, "nil"
 * is returned unless "allowUnknown" is set to "true".  In that case, a
 * DiskFSUnknown is returned.
 *
 * This doesn't inspire the DiskFS to do any processing, just creates the
 * new object.
 */
DiskFS*
DiskImg::OpenAppropriateDiskFS(bool allowUnknown)
{
	DiskFS* pDiskFS = nil;

	/*
	 * Create an appropriate DiskFS object.
	 */
	switch (GetFSFormat()) {
	case DiskImg::kFormatDOS33:
	case DiskImg::kFormatDOS32:
		pDiskFS = new DiskFSDOS33();
		break;
	case DiskImg::kFormatProDOS:
		pDiskFS = new DiskFSProDOS();
		break;
	case DiskImg::kFormatPascal:
		pDiskFS = new DiskFSPascal();
		break;
	case DiskImg::kFormatMacHFS:
		pDiskFS = new DiskFSHFS();
		break;
	case DiskImg::kFormatUNIDOS:
		pDiskFS = new DiskFSUNIDOS();
		break;
	case DiskImg::kFormatOzDOS:
		pDiskFS = new DiskFSOzDOS();
		break;
	case DiskImg::kFormatCFFA4:
	case DiskImg::kFormatCFFA8:
		pDiskFS = new DiskFSCFFA();
		break;
	case DiskImg::kFormatMacPart:
		pDiskFS = new DiskFSMacPart();
		break;
	case DiskImg::kFormatMicroDrive:
		pDiskFS = new DiskFSMicroDrive();
		break;
	case DiskImg::kFormatFocusDrive:
		pDiskFS = new DiskFSFocusDrive();
		break;
	case DiskImg::kFormatCPM:
		pDiskFS = new DiskFSCPM();
		break;
	case DiskImg::kFormatMSDOS:
		pDiskFS = new DiskFSFAT();
		break;
	case DiskImg::kFormatRDOS33:
	case DiskImg::kFormatRDOS32:
	case DiskImg::kFormatRDOS3:
		pDiskFS = new DiskFSRDOS();
		break;

	default:
		WMSG1("WARNING: unhandled DiskFS case %d\n", GetFSFormat());
		assert(false);
		/* fall through */
	case DiskImg::kFormatGenericPhysicalOrd:
	case DiskImg::kFormatGenericProDOSOrd:
	case DiskImg::kFormatGenericDOSOrd:
	case DiskImg::kFormatGenericCPMOrd:
	case DiskImg::kFormatUnknown:
		if (allowUnknown) {
			pDiskFS = new DiskFSUnknown();
			break;
		}
	}

	return pDiskFS;
}


/*
 * Fill an array with SectorOrder values.  The ordering specified by "first"
 * will come first.  Unused entries will be set to "unknown" and should be
 * ignored.
 *
 * "orderArray" must have kSectorOrderMax elements.
 */
/*static*/ void
DiskImg::GetSectorOrderArray(SectorOrder* orderArray, SectorOrder first)
{
	// init array
	for (int i = 0; i < kSectorOrderMax; i++)
		orderArray[i] = (SectorOrder) i;

	// pull the best-guess ordering to the front
	assert(orderArray[0] == kSectorOrderUnknown);

	orderArray[0] = first;
	orderArray[(int) first] = kSectorOrderUnknown;

	// don't bother checking CP/M sector order
	orderArray[kSectorOrderCPM] = kSectorOrderUnknown;
}


/*
 * Return a short string describing "format".
 *
 * These are semi-duplicated in ImageFormatDialog.cpp in CiderPress.
 */
/*static*/ const char*
DiskImg::ToStringCommon(int format,	const ToStringLookup* pTable,
	int tableSize)
{
	for (int i = 0; i < tableSize; i++) {
		if (pTable[i].format == format)
			return pTable[i].str;
	}

	assert(false);
	return "(unknown)";
}

/*static*/ const char*
DiskImg::ToString(OuterFormat format)
{
	static const ToStringLookup kOuterFormats[] = {
		{ DiskImg::kOuterFormatUnknown,			"Unknown format" },
		{ DiskImg::kOuterFormatNone,			"(none)" },
		{ DiskImg::kOuterFormatCompress,		"UNIX compress" },
		{ DiskImg::kOuterFormatGzip,			"gzip" },
		{ DiskImg::kOuterFormatBzip2,			"bzip2" },
		{ DiskImg::kOuterFormatZip,				"Zip archive" },
	};

	return ToStringCommon(format, kOuterFormats, NELEM(kOuterFormats));
}
/*static*/ const char*
DiskImg::ToString(FileFormat format)
{
	static const ToStringLookup kFileFormats[] = {
		{ DiskImg::kFileFormatUnknown,			"Unknown format" },
		{ DiskImg::kFileFormatUnadorned,		"Unadorned raw data" },
		{ DiskImg::kFileFormat2MG,				"2MG" },
		{ DiskImg::kFileFormatNuFX,				"NuFX (ShrinkIt)" },
		{ DiskImg::kFileFormatDiskCopy42,		"DiskCopy 4.2" },
		{ DiskImg::kFileFormatDiskCopy60,		"DiskCopy 6.0" },
		{ DiskImg::kFileFormatDavex,			"Davex volume image" },
		{ DiskImg::kFileFormatSim2eHDV,			"Sim //e HDV" },
		{ DiskImg::kFileFormatTrackStar,		"TrackStar image" },
		{ DiskImg::kFileFormatFDI,				"FDI image" },
		{ DiskImg::kFileFormatDDD,				"DDD" },
		{ DiskImg::kFileFormatDDDDeluxe,		"DDDDeluxe" },
	};

	return ToStringCommon(format, kFileFormats, NELEM(kFileFormats));
};
/*static*/ const char*
DiskImg::ToString(PhysicalFormat format)
{
	static const ToStringLookup kPhysicalFormats[] = {
		{ DiskImg::kPhysicalFormatUnknown,		"Unknown format" },
		{ DiskImg::kPhysicalFormatSectors,		"Sectors" },
		{ DiskImg::kPhysicalFormatNib525_6656,	"Raw nibbles (6656-byte)" },
		{ DiskImg::kPhysicalFormatNib525_6384,	"Raw nibbles (6384-byte)" },
		{ DiskImg::kPhysicalFormatNib525_Var,	"Raw nibbles (variable len)" },
	};

	return ToStringCommon(format, kPhysicalFormats, NELEM(kPhysicalFormats));
};
/*static*/ const char*
DiskImg::ToString(SectorOrder format)
{
	static const ToStringLookup kSectorOrders[] = {
		{ DiskImg::kSectorOrderUnknown,			"Unknown ordering" },
		{ DiskImg::kSectorOrderProDOS,			"ProDOS block ordering" },
		{ DiskImg::kSectorOrderDOS,				"DOS sector ordering" },
		{ DiskImg::kSectorOrderCPM,				"CP/M block ordering" },
		{ DiskImg::kSectorOrderPhysical,		"Physical sector ordering" },
	};

	return ToStringCommon(format, kSectorOrders, NELEM(kSectorOrders));
};
/*static*/ const char*
DiskImg::ToString(FSFormat format)
{
	static const ToStringLookup kFSFormats[] = {
		{ DiskImg::kFormatUnknown,				"Unknown" },
		{ DiskImg::kFormatProDOS,				"ProDOS" },
		{ DiskImg::kFormatDOS33,				"DOS 3.3" },
		{ DiskImg::kFormatDOS32,				"DOS 3.2" },
		{ DiskImg::kFormatPascal,				"Pascal" },
		{ DiskImg::kFormatMacHFS,				"HFS" },
		{ DiskImg::kFormatMacMFS,				"MFS" },
		{ DiskImg::kFormatLisa,					"Lisa" },
		{ DiskImg::kFormatCPM,					"CP/M" },
		{ DiskImg::kFormatMSDOS,				"MS-DOS FAT" },
		{ DiskImg::kFormatISO9660,				"ISO-9660" },
		{ DiskImg::kFormatRDOS33,				"RDOS 3.3 (16-sector)" },
		{ DiskImg::kFormatRDOS32,				"RDOS 3.2 (13-sector)" },
		{ DiskImg::kFormatRDOS3,				"RDOS 3 (cracked 13-sector)" },
		{ DiskImg::kFormatGenericDOSOrd,		"Generic DOS sectors" },
		{ DiskImg::kFormatGenericProDOSOrd,		"Generic ProDOS blocks" },
		{ DiskImg::kFormatGenericPhysicalOrd,	"Generic raw sectors" },
		{ DiskImg::kFormatGenericCPMOrd,		"Generic CP/M blocks" },
		{ DiskImg::kFormatUNIDOS,				"UNIDOS (400K DOS x2)" },
		{ DiskImg::kFormatOzDOS,				"OzDOS (400K DOS x2)" },
		{ DiskImg::kFormatCFFA4,				"CFFA (4 or 6 partitions)" },
		{ DiskImg::kFormatCFFA8,				"CFFA (8 partitions)" },
		{ DiskImg::kFormatMacPart,				"Macintosh partitioned disk" },
		{ DiskImg::kFormatMicroDrive,			"MicroDrive partitioned disk" },
		{ DiskImg::kFormatFocusDrive,			"FocusDrive partitioned disk" },
	};

	return ToStringCommon(format, kFSFormats, NELEM(kFSFormats));
};


/*
 * strerror() equivalent for DiskImg errors.
 */
const char*
DiskImgLib::DIStrError(DIError dierr)
{
	if (dierr > 0) {
		const char* msg;
		msg = strerror(dierr);
		if (msg != nil)
			return msg;
	}

    /*
     * BUG: this should be set up as per-thread storage in an MT environment.
     * I would be more inclined to worry about this if I was expecting
     * to hit "default:".  So long as valid values are passed in, and the
     * switch statement is kept up to date, we should never have cause
     * to return this.
     *
     * An easier solution, should this present a problem for someone, would
     * be to have the function return nil or "unknown error" when the
     * error value isn't recognized.  I'd recommend leaving it as-is for
     * debug builds, though, as it's helpful to know *which* error is not
     * recognized.
     */
    static char defaultMsg[32];

    switch (dierr) {
    case kDIErrNone:
        return "(no error)";

	case kDIErrAccessDenied:
        return "access denied";
	case kDIErrVWAccessForbidden:
		return "for safety, write access to this volume is forbidden";
	case kDIErrSharingViolation:
		return "file is already open and cannot be shared";
	case kDIErrNoExclusiveAccess:
		return "couldn't get exclusive access";
	case kDIErrWriteProtected:
		return "write protected";
	case kDIErrCDROMNotSupported:
		return "access to CD-ROM drives is not supported";
	case kDIErrASPIFailure:
		return "an ASPI request failed";
	case kDIErrSPTIFailure:
		return "an SPTI request failed";
	case kDIErrSCSIFailure:
		return "a SCSI request failed";
	case kDIErrDeviceNotReady:
		return "device not ready";

	case kDIErrFileNotFound:
        return "file not found";
	case kDIErrForkNotFound:
		return "fork not found";
	case kDIErrAlreadyOpen:
        return "an image is already open";
	case kDIErrFileOpen:
		return "file is open";
	case kDIErrNotReady:
		return "object not ready";
    case kDIErrFileExists:
        return "file already exists";
    case kDIErrDirectoryExists:
        return "directory already exists";

    case kDIErrEOF:
        return "end of file reached";
	case kDIErrReadFailed:
		return "read failed";
	case kDIErrWriteFailed:
		return "write failed";
	case kDIErrDataUnderrun:
		return "tried to read past end of file";
	case kDIErrDataOverrun:
		return "tried to write past end of file";
	case kDIErrGenericIO:
		return "I/O error";

	case kDIErrOddLength:
        return "image size is wrong";
	case kDIErrUnrecognizedFileFmt:
        return "not a recognized disk image format";
	case kDIErrBadFileFormat:
        return "image file contents aren't in expected format";
	case kDIErrUnsupportedFileFmt:
        return "file format not supported";
	case kDIErrUnsupportedPhysicalFmt:
        return "physical format not supported";
	case kDIErrUnsupportedFSFmt:
        return "filesystem type not supported";
	case kDIErrBadOrdering:
		return "bad sector ordering";
	case kDIErrFilesystemNotFound:
		return "specified filesystem not found";
    case kDIErrUnsupportedAccess:
        return "the method of access used isn't supported for this image";
	case kDIErrUnsupportedImageFeature:
		return "image file uses features that CiderPress doesn't support";

	case kDIErrInvalidTrack:
        return "invalid track number";
	case kDIErrInvalidSector:
        return "invalid sector number";
	case kDIErrInvalidBlock:
        return "invalid block number";
	case kDIErrInvalidIndex:
		return "invalid index number";

	case kDIErrDirectoryLoop:
		return "disk directory structure has an infinite loop";
	case kDIErrFileLoop:
		return "file structure has an infinite loop";
	case kDIErrBadDiskImage:
		return "the filesystem on this image appears damaged";
	case kDIErrBadFile:
		return "file structure appears damaged";
	case kDIErrBadDirectory:
		return "a directory appears damaged";
	case kDIErrBadPartition:
		return "bad partition";

	case kDIErrFileArchive:
		return "this looks like a file archive, not a disk archive";
	case kDIErrUnsupportedCompression:
		return "compression method not supported";
	case kDIErrBadChecksum:
		return "checksum doesn't match, data may be corrupted";
	case kDIErrBadCompressedData:
		return "the compressed data is corrupted";
	case kDIErrBadArchiveStruct:
		return "archive may be damaged";

    case kDIErrBadNibbleSectors:
        return "couldn't read sectors from this image";
    case kDIErrSectorUnreadable:
        return "sector not readable";
    case kDIErrInvalidDiskByte:
        return "found invalid nibble image disk byte";
	case kDIErrBadRawData:
		return "couldn't convert raw data to nibble data";

	case kDIErrInvalidFileName:
		return "invalid file name";
	case kDIErrDiskFull:
		return "disk full";
	case kDIErrVolumeDirFull:
		return "volume directory is full";
	case kDIErrInvalidCreateReq:
		return "invalid disk image create request";
	case kDIErrTooBig:
		return "size is larger than we can handle";

    case kDIErrGeneric:
        return "DiskImg generic error";
    case kDIErrInternal:
        return "DiskImg internal error";
	case kDIErrMalloc:
		return "memory allocation failure";
	case kDIErrInvalidArg:
        return "invalid argument";
	case kDIErrNotSupported:
		return "feature not supported";
	case kDIErrCancelled:
		return "cancelled by user";

	case kDIErrNufxLibInitFailed:
		return "NufxLib initialization failed";

    default:
        sprintf(defaultMsg, "(error=%d)", dierr);
        return defaultMsg;
    }
}

/*
 * High ASCII conversion table, from Technical Note PT515,
 * "Apple File Exchange Q&As".  The table is available in a hopelessly
 * blurry PDF or a pair of GIFs created with small fonts, but I think I
 * have mostly captured it.
 */
/*static*/ const unsigned char DiskImg::kMacHighASCII[128+1] =
	"AACENOUaaaaaaceeeeiiiinooooouuuu"		// 0x80 - 0x9f
	"tocL$oPBrct'.=AO%+<>YudsPpSaoOao"		// 0xa0 - 0xbf
	"?!-vf=d<>. AAOOo--\"\"''/oyY/o<> f"	// 0xc0 - 0xdf
	"|*,,%AEAEEIIIIOOaOUUUi^~-,**,\"? ";	// 0xe0 - 0xff


/*
 * Hack for Win32 systems.  See Win32BlockIO.cpp for commentary.
 */
bool DiskImgLib::gAllowWritePhys0 = false;
/*static*/ void DiskImg::SetAllowWritePhys0(bool val) {
	DiskImgLib::gAllowWritePhys0 = val;
}